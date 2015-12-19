#include "main.h"

#define MIN(a, b) ( \
		{ __auto_type __a = (a); __auto_type __b = (b); \
		  __a < __b ? __a : __b; })
#define MAX(a, b) ( \
		{ __auto_type __a = (a); __auto_type __b = (b); \
		  __a > __b ? __a : __b; })

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif


static void _parse_error(Tcl_Interp* interp, const char* errmsg, const char* doc, size_t char_ofs) //{{{
{
	const char*	char_ofs_str = Tcl_GetString(Tcl_NewIntObj(char_ofs));

	Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing JSON value: %s at offset %s", errmsg, char_ofs_str));
	Tcl_SetErrorCode(interp, "RL", "JSON", "PARSE", errmsg, doc, char_ofs_str, NULL);
}

//}}}
static void _string_callback(struct parse_context* cx, Tcl_Obj* val) //{{{
{
	int	type, l;
	const char*	s;

	s = Tcl_GetStringFromObj(val, &l);

	if (
			l >= 3 &&
			s[0] == '~' &&
			s[2] == ':'
	) {
		switch (s[1]) {
			case 'S': type = JSON_DYN_STRING; break;
			case 'N': type = JSON_DYN_NUMBER; break;
			case 'B': type = JSON_DYN_BOOL; break;
			case 'J': type = JSON_DYN_JSON; break;
			case 'T': type = JSON_DYN_TEMPLATE; break;
			case 'L': type = JSON_DYN_LITERAL; break;
			default:  type = JSON_UNDEF; break;
		}

		if (type != JSON_UNDEF) {
			append_to_cx(cx, JSON_NewJvalObj2(type, Tcl_NewStringObj(s+3, l-3)));
			return;
		}
	}

	append_to_cx(cx, JSON_NewJvalObj2(JSON_STRING, val));
}

//}}}
static void _start_map_callback(struct parse_context* cx, size_t char_ofs) //{{{
{
	struct parse_context*	new;

	if (cx->container == JSON_UNDEF) {
		new = cx;
	} else {
		new = (struct parse_context*)ckalloc(sizeof(struct parse_context));
		new->prev = cx->last;
		cx->last = new;
		new->hold_key = NULL;
	}
	new->container = JSON_OBJECT;
	new->val = JSON_NewJvalObj2(JSON_OBJECT, Tcl_NewDictObj());
	Tcl_IncrRefCount(new->val);
}

//}}}
static void _end_map_callback(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	if (cx->last != cx) {
		cx->last = tail->prev;

		append_to_cx(cx, tail->val);
		Tcl_DecrRefCount(tail->val); tail->val = NULL;

		ckfree((char*)tail); tail = NULL;
	}
}

//}}}
static void _start_array_callback(struct parse_context* cx, size_t char_ofs) //{{{
{
	struct parse_context*	new;

	if (cx->container == JSON_UNDEF) {
		new = cx;
	} else {
		new = (struct parse_context*)ckalloc(sizeof(struct parse_context));
		new->prev = cx->last;
		new->hold_key = NULL;
		cx->last = new;
	}
	new->container = JSON_ARRAY;
	new->val = JSON_NewJvalObj2(JSON_ARRAY, Tcl_NewListObj(0, NULL));
	Tcl_IncrRefCount(new->val);
}

//}}}
static void _end_array_callback(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	if (cx->last != cx) {
		cx->last = tail->prev;

		append_to_cx(cx, tail->val);
		Tcl_DecrRefCount(tail->val); tail->val = NULL;

		ckfree((char*)tail); tail = NULL;
	}
}

//}}}

static int is_whitespace(const char c) //{{{
{
	switch (c) {
		case 0x20:
		case 0x09:
		case 0x0A:
		case 0x0D:
			return 1;

		default:
			return 0;
	}
}

//}}}
static void char_advance(const char** p, const char* e, size_t* char_adj) //{{{
{
	// TODO: use Tcl_UtfNext instead?
	const char		first = **p;
	unsigned char	eat;
	size_t			remaining;

	(*p)++;
	if (unlikely(first >= 0x80)) {
		remaining = e - *p;
		// Advance to next UTF-8 character
		// TODO: detect invalid sequences?
		if (first < 0b11100000) {
			eat = MIN(1, remaining);
		} else if (first < 0b11110000) {
			eat = MIN(2, remaining);
		} else if (first < 0b11111000) {
			eat = MIN(3, remaining);
		} else if (first < 0b11111100) {
			eat = MIN(4, remaining);
		} else {
			eat = MIN(5, remaining);
		}
		*p += eat;
		*char_adj += eat;
	}
}

//}}}
static int skip_whitespace(const char** s, const char* e, const char** err, const char** err_at, size_t* char_adj) //{{{
{
	const char*	start;
	const char* p;

	p = start = *s;

	while (1) {
		while (p < e && is_whitespace(*p)) p++;

		if (p < e-1 && *p == '/') {
			start = p;
			char_advance(&p, e, char_adj);
			if (*p == '/') {
				p++;

				while (p < e && *p != '\n')
					char_advance(&p, e, char_adj);
			} else if (*p == '*') {
				p++;

				while (p < e-2 && *p != '*')
					char_advance(&p, e, char_adj);

				if (unlikely(p > e-2 || *p++ != '*' || *p++ != '/')) {
					*err = "Unterminated comment";
				}
			} else {
				*err = "Illegal character";
				goto err;
			}
			continue;
		}

		break;
	}

	*s = p;
	return 0;

err:
	*s = p;
	*err_at = start;
	return 1;
}

//}}}
static int value_type(struct interp_cx* l, const char* doc, const char* p, const char* e, size_t* char_adj, const char** next, enum json_types *type, Tcl_Obj** val) //{{{
{
	const char*		err_at = NULL;
	const char*		errmsg = NULL;

	*val = NULL;

	if (unlikely(p >= e)) goto err;

	switch (*p) {
		case '"':
			p++;	// Advance to the first byte of the string
			{
				Tcl_Obj*	out = NULL;
				const char*	chunk;
				size_t		len;

				while (1) {
					chunk = p;
					//fprintf(stderr, "string char walk loop top, index %d: %c\n", p-doc, *p);
					while (likely(p < e && *p != '"' && *p != '\\' && *p > 0x1f))
						char_advance(&p, e, char_adj);

					// TESTED {{{
					if (unlikely(p >= e)) goto err;

					len = p-chunk;

					if (likely(out == NULL)) {
						// Optimized case - no backquoted sequences to deal with
						out = len == 0 ? l->tcl_empty : Tcl_NewStringObj(chunk, len);
					} else if (len > 0) {
						if (unlikely(Tcl_IsShared(out)))
							out = Tcl_DuplicateObj(out);	// Shouldn't be possible to reach

						Tcl_AppendToObj(out, chunk, len);
					}

					if (likely(*p == '"')) {
						p++;	// Point at the first byte after the string
						break;
					}

					if (unlikely(*p != '\\')) goto err;

					p++;	// Advance to the backquoted byte
					if (unlikely(p >= e)) goto err;

					if (Tcl_IsShared(out))
						out = Tcl_DuplicateObj(out);

					switch (*p) {
						case '\\':
						case '"':
						case '/':		// RFC4627 allows this for some reason
							Tcl_AppendToObj(out, p, 1);
							break;

						case 'b': Tcl_AppendToObj(out, "\b", 1); break;
						case 'f': Tcl_AppendToObj(out, "\f", 1); break;
						case 'n': Tcl_AppendToObj(out, "\n", 1); break;
						case 'r': Tcl_AppendToObj(out, "\r", 1); break;
						case 't': Tcl_AppendToObj(out, "\t", 1); break;

						case 'u':
							{
								Tcl_UniChar	acc=0;
								int			i=4, digit;

								if (unlikely(e-p-2 < i)) {	// -2 is for the "u" and the close quote
									err_at = e-1;
									if (err_at < e && *err_at != '"')
										err_at++;	// Bodge the case where the doc was truncated without a closing quote
									errmsg = "Unicode sequence too short";
									goto err;
								}

								while (i--) {
									digit = *++p - '0';
									if (unlikely(digit < 0)) {
										err_at = p;
										errmsg = "Unicode sequence too short";
										goto err;
									}
									if (digit > 9) {
										digit -= (digit < 'a' - '0') ?
											'A' - '0' - 0xA :
											'a' - '0' - 0xA;

										if (unlikely(digit < 0xA || digit > 0xF)) {
											err_at = p;
											errmsg = "Unicode sequence too short";
											goto err;
										}
									}

									acc <<= 4;
									acc += digit;
								}

								Tcl_AppendUnicodeToObj(out, &acc, 1);
							}
							break;

						default:
							goto err;
					}
					p++;	// Advance to the first byte after the backquoted sequence
				}

				*type = JSON_STRING;
				*val = out;
				//}}}
			}
			break;

		case '{':
			*type = JSON_OBJECT;
			p++;
			break;

		case '[':
			*type = JSON_ARRAY;
			p++;
			break;

		case 't':
			if (unlikely(strncmp(p, "true", MIN(4, e-p)) != 0)) goto err;

			*type = JSON_BOOL;
			*val = l->tcl_true;
			p += 4;
			break;

		case 'f':
			if (unlikely(strncmp(p, "false", MIN(5, e-p)) != 0)) goto err;

			*type = JSON_BOOL;
			*val = l->tcl_false;
			p += 5;
			break;

		case 'n':
			if (unlikely(strncmp(p, "null", MIN(4, e-p)) != 0)) goto err;

			*type = JSON_NULL;
			p += 4;
			*val = l->tcl_empty;
			break;

		default:
			// TODO: Reject leading zero?  The RFC doesn't allow leading zeros
			{
				// TESTED {{{
				const char*	start = p;
				const char*	t;

				if (*p == '-') p++;
				if (unlikely(*p < '0' || *p > '9')) goto err;

				while (*p >= '0' && *p < '9') p++;

				if (*p == '.') {	// p could point at the NULL terminator at this point
					p++;
					t = p;
					while (*p >= '0' && *p < '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the decimal point
				}

				if (unlikely((*p | 0b100000) == 'e')) {	// p could point at the NULL terminator at this point
					p++;
					if (*p == '+' || *p == '-') p++;
					t = p;
					while (*p >= '0' && *p < '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the exponent symbol
				}

				*type = JSON_NUMBER;
				*val = Tcl_NewStringObj(start, p-start);
				//}}}
			}
	}

	// TESTED {{{
	*next = p;
	return TCL_OK;

err:
	if (err_at == NULL)
		err_at = p;

	if (errmsg == NULL)
		errmsg = (err_at == e) ? "Document truncated" : "Illegal character";

	_parse_error(l->interp, errmsg, doc, (err_at - doc) - *char_adj);

	return TCL_ERROR;
	//}}}
}

//}}}
static void free_cx(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	while (cx->last != cx) {
		cx->last = tail->prev;
		if (tail->val != NULL) {
			Tcl_DecrRefCount(tail->val); tail->val = NULL;
		}
		ckfree((char*)tail); tail = NULL;
	}
}

//}}}
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct interp_cx* l = cdata;
	const char*		err_at = NULL;
	const char*		errmsg = "Illegal character";
	size_t			char_adj = 0;		// Offset addjustment to account for multibyte UTF-8 sequences
	const char*		doc;
	enum json_types	type;
	Tcl_Obj*		val;
	const char*		p;
	const char*		e;
	const char*		val_start;
	int				len;
	struct parse_context	cx;
	Tcl_Obj*		obj = NULL;

	cx.prev = NULL;
	cx.last = &cx;
	cx.hold_key = NULL;
	cx.container = JSON_UNDEF;
	cx.val = NULL;
	cx.char_ofs = 0;

	CHECK_ARGS(1, "json_value");

	p = doc = Tcl_GetStringFromObj(objv[1], &len);
	e = p + len;

	// Skip leading whitespace and comments
	if (skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0) goto whitespace_err;

	while (p < e) {
		//fprintf(stderr, "value_type loop top, byte ofs %d, *p: %c%c\n", p-doc, *p, *(p+1));
		if (cx.last->container == JSON_OBJECT) { // Read the key if in object mode {{{
			const char*	key_start = p;
			size_t		key_start_char_adj = char_adj;

			TEST_OK(value_type(l, doc, p, e, &char_adj, &p, &type, &val));
			if (unlikely(type != JSON_STRING && type != JSON_DYN_STRING && type != JSON_DYN_LITERAL)) {
				_parse_error(interp, "Object key is not a string", doc, (key_start-doc) - key_start_char_adj);
				goto err;
			}
			cx.last->hold_key = val;

			if (unlikely(*p != ':')) {
				_parse_error(interp, "Expecting : after object key", doc, (p-doc) - char_adj);
				goto err;
			}
			p++;

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		}
		//}}}

		val_start = p;
		TEST_OK(value_type(l, doc, p, e, &char_adj, &p, &type, &val));

		switch (type) {
			case JSON_OBJECT:
				_start_map_callback(&cx, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == '}') {
					_end_map_callback(&cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_ARRAY:
				_start_array_callback(&cx, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == ']') {
					_end_array_callback(&cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_STRING:
				_string_callback(&cx, val);
				break;

			case JSON_BOOL:
			case JSON_NULL:
			case JSON_NUMBER:
				append_to_cx(&cx, JSON_NewJvalObj2(type, val));
				break;

			default:
				free_cx(&cx);
				THROW_ERROR("Unexpected json value type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

after_value:	// Yeah, goto.  But the alternative abusing loops was worse
		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		if (p >= e) break;

		switch (cx.last->container) { // Handle eof and end-of-context or comma for array and object {{{
			case JSON_OBJECT:
				if (*p == '}') {
					_end_map_callback(&cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					_parse_error(interp, "Expecting } or ,", doc, (p-doc) - char_adj);
					goto err;
				}

				p++;
				break;

			case JSON_ARRAY:
				if (*p == ']') {
					_end_array_callback(&cx);
					p++;
					goto after_value;
				} else if (unlikely(*p != ',')) {
					_parse_error(interp, "Expecting ] or ,", doc, (p-doc) - char_adj);
					goto err;
				}

				p++;
				break;

			default:
				if (unlikely(p < e)) {
					_parse_error(interp, "Trailing garbage after value", doc, (p - doc) - char_adj);
					goto err;
				}
		}

		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		//}}}
	}

	if (unlikely(&cx != cx.last)) { // Unterminated object or array context {{{
		switch (cx.last->container) {
			case JSON_OBJECT:
				_parse_error(interp, "Unterminated object", doc, cx.last->char_ofs);
				goto err;

			case JSON_ARRAY:
				_parse_error(interp, "Unterminated array", doc, cx.last->char_ofs);
				goto err;
		}
	}
	//}}}

	// TESTED {{{
	if (unlikely(cx.val == NULL)) {
		err_at = doc;
		errmsg = "No JSON value found";
		goto whitespace_err;
	}

	obj = Tcl_NewObj();
	obj->typePtr = &json_type;
	obj->internalRep.ptrAndLongRep.value = cx.val->internalRep.ptrAndLongRep.value;
	obj->internalRep.ptrAndLongRep.ptr = cx.val->internalRep.ptrAndLongRep.ptr;
	Tcl_InvalidateStringRep(obj);

	// We're transferring the ref from cx.val to our intrep
	//Tcl_IncrRefcount(obj->internalRep.ptrAndLongRep.ptr
	//Tcl_DecrRefCount(cx.val);
	cx.val = NULL;

	Tcl_SetObjResult(interp, obj);
	return TCL_OK;

whitespace_err:
	_parse_error(interp, errmsg, doc, (err_at - doc) - char_adj);

err:
	free_cx(&cx);
	return TCL_ERROR;
	//}}}
}

//}}}

// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
