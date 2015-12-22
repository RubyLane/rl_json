#include "main.h"

#define MIN(a, b) ( \
		{ __auto_type __a = (a); __auto_type __b = (b); \
		  __a < __b ? __a : __b; })
#define MAX(a, b) ( \
		{ __auto_type __a = (a); __auto_type __b = (b); \
		  __a > __b ? __a : __b; })


static void _parse_error(Tcl_Interp* interp, const char* errmsg, const unsigned char* doc, size_t char_ofs) //{{{
{
	const char*	char_ofs_str = Tcl_GetString(Tcl_NewIntObj(char_ofs));

	Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing JSON value: %s at offset %s", errmsg, char_ofs_str));
	Tcl_SetErrorCode(interp, "RL", "JSON", "PARSE", errmsg, doc, char_ofs_str, NULL);
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

static int is_whitespace(const unsigned char c) //{{{
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
// TESTED {{{
#if 1
static void char_advance(const unsigned char** p, size_t* char_adj) //{{{
{
	// TODO: use Tcl_UtfNext instead?
	// This relies on some properties from the utf-8 returned by Tcl_GetString:
	//	- no invalid encodings (partial utf-8 sequences, etc)
	//	- not truncated in the middle of a char
	const unsigned char	first = **p;
	unsigned int		eat;

	(*p)++;
	if (unlikely(first >= 0xC0)) {
		// Advance to next UTF-8 character
		// TODO: detect invalid sequences?
		if (first < 0b11100000) {
			eat = 1;
#if TCL_UTF_MAX == 3
		} else {
			eat = 2;
#else
		} else if (first < 0b11110000) {
			eat = 2;
		} else if (first < 0b11111000) {
			eat = 3;
		} else if (first < 0b11111100) {
			eat = 4;
		} else {
			eat = 5;
#endif
		}
		//fprintf(stderr, "skipping %d, from (%s) to (%s)\n", eat+1, *p-1, *p+eat);
		*p += eat;
		*char_adj += eat;
	}
}

//}}}
#else
/*
static const unsigned char utf_skip[64] __attribute__((__aligned__(64))) = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
#if TCL_UTF_MAX > 3
    3,3,3,3,3,3,3,3,
#else
    0,0,0,0,0,0,0,0,
#endif
#if TCL_UTF_MAX > 4
    4,4,4,4,
#else
    0,0,0,0,
#endif
#if TCL_UTF_MAX > 5
    5,5,5,5
#else
    0,0,0,0
#endif
};

static void char_advance(const unsigned char** p, size_t* char_adj) //{{{
{
	// TODO: use Tcl_UtfNext instead?
	// This relies on some properties from the utf-8 returned by Tcl_GetString:
	//	- no invalid encodings (partial utf-8 sequences, etc)
	//	- not truncated in the middle of a char
	const unsigned char	first = **p;
	unsigned char		eat;

	(*p)++;
	if (unlikely(first >= 0xC0)) {
		eat = utf_skip[first - 0xC0];
		*p += eat;
		*char_adj += eat;
	}
}

//}}}
*/
static const unsigned char utf_skip[16] __attribute__((__aligned__(64))) = {
    1,1,1,1,1,1,1,1,
    2,2,2,2,
#if TCL_UTF_MAX > 3
    3,3,
#else
    0,0,
#endif
#if TCL_UTF_MAX > 4
    4,
#else
    0,
#endif
#if TCL_UTF_MAX > 5
    5
#else
    0
#endif
};

static void char_advance(const unsigned char** p, size_t* char_adj) //{{{
{
	// This relies on some properties from the utf-8 returned by Tcl_GetString:
	//	- no invalid encodings (partial utf-8 sequences, etc)
	//	- not truncated in the middle of a char
	const int		first = **p - 0xC0;
	unsigned char	eat;
	//const signed char	was = **p;

	(*p)++;
	if (unlikely(first >= 0)) {
		eat = utf_skip[first >> 2];
		//fprintf(stderr, "Byte 0x%02X, was: 0x%02X(%d), first: %d, first>>2: %d, eat: %d, str: %s\n", *((*p)-1), was, was-0xC0, first, first>>2, eat, (*p)-1);
		*p += eat;
		*char_adj += eat;
	}
}

//}}}
#endif
//}}}
static int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj) //{{{
{
	const unsigned char*	p;
	const unsigned char*	start;
	size_t					start_char_adj;

	p = start = *s;

	while (1) {
		while (is_whitespace(*p)) p++;

		if (unlikely(*p == '/')) {
			start = p;
			start_char_adj = *char_adj;
			p++;
			if (*p == '/') {
				p++;

				while (likely(p < e && *p != '\n'))
					char_advance(&p, char_adj);
			} else if (*p == '*') {
				p++;

				while (likely(p < e-2 && *p != '*'))
					char_advance(&p, char_adj);

				if (unlikely(*p++ != '*' || *p++ != '/')) {
					*errmsg = "Unterminated comment";
					goto err;
				}
			} else {
				*errmsg = "Illegal character";
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
	*char_adj = start_char_adj;
	return 1;
}

//}}}
	// TESTED {{{
static int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val) //{{{
{
	const unsigned char*	err_at = NULL;
	const char*				errmsg = NULL;

	*val = NULL;

	if (unlikely(p >= e)) goto err;

	switch (*p) {
		case '"':
			p++;	// Advance past the " to the first byte of the string
			{
				Tcl_Obj*				out = NULL;
				const unsigned char*	chunk;
				size_t					len;
				char					mapped;
				enum json_types			stype = JSON_STRING;

				// Peek ahead to detect template subst markers.
				if (p[0] == '~' && e-p >= 3 && p[2] == ':') {
					switch (p[1]) {
						case 'S': stype = JSON_DYN_STRING; break;
						case 'N': stype = JSON_DYN_NUMBER; break;
						case 'B': stype = JSON_DYN_BOOL; break;
						case 'J': stype = JSON_DYN_JSON; break;
						case 'T': stype = JSON_DYN_TEMPLATE; break;
						case 'L': stype = JSON_DYN_LITERAL; break;
						default:  stype = JSON_STRING; p -= 3; break;
					}
					p += 3;
				}

				while (1) {
					chunk = p;
					while (likely(p < e && *p != '"' && *p != '\\' && *p > 0x1f))
						char_advance(&p, char_adj);

					if (unlikely(p >= e)) goto err;

					len = p-chunk;

					if (likely(out == NULL)) {
#if 0
						// Optimized case - no backquoted sequences to deal with
						out = len == 0 ? l->tcl_empty : Tcl_NewStringObj((const char*)chunk, len);
#else
						//out = Tcl_NewStringObj((const char*)chunk, MIN(len,5));
#if 0
						if (len == 0) {
							out = l->tcl_empty;
						} else if (len < STRING_DEDUP_MAX && *p == '"') { // TODO: check that Tcl_AppendUnicodeToObj translates \u0000 to c080
							out = new_stringobj_dedup(l, (const char*)chunk, len);
						} else {
							out = Tcl_NewStringObj((const char*)chunk, len);
						}
#else
						out = new_stringobj_dedup(l, (const char*)chunk, len);
						//fprintf(stderr, "New string: (%s) for chunk: (%s)\n", Tcl_GetString(out), Tcl_GetString(Tcl_NewStringObj(chunk, len)));
#endif
						/*
						//out = Tcl_NewStringObj((const char*)chunk, 1);
						dbuf = ckalloc(len);
						memcpy(dbuf, chunk, len);
						//ckfree(dbuf);
						*/

						/*
						out = Tcl_NewObj();
						//out = ckalloc(sizeof(Tcl_Obj));
						//out = Tcl_DuplicateObj(l->tcl_empty);
						//out = (Tcl_Obj*)ckalloc(sizeof(Tcl_Obj));
						//out->refCount = 1;
						//out->typePtr = 0;

						//len = MIN(len,4);
						out->bytes = ckalloc(len+1);
						out->length = len;
						memcpy(out->bytes, chunk, len);
						out->bytes[len] = 0;
						*/
#endif
					} else if (len > 0) {
						if (unlikely(Tcl_IsShared(out)))
							out = Tcl_DuplicateObj(out);	// Can do this because the ref were were operating under is on loan from new_stringobj_dedup

						//fprintf(stderr, "Appending chunk (%s) to (%s)\n", Tcl_GetString(Tcl_NewStringObj(chunk,len)), Tcl_GetString(out));
						Tcl_AppendToObj(out, (const char*)chunk, len);
					}

					if (likely(*p == '"')) {
						p++;	// Point at the first byte after the string
						break;
					}

					if (unlikely(*p != '\\')) goto err;

					p++;	// Advance to the backquoted byte

					if (unlikely(Tcl_IsShared(out)))
						out = Tcl_DuplicateObj(out);	// Can do this because the ref were were operating under is on loan from new_stringobj_dedup

					switch (*p) {	// p could point at the NULL terminator at this point
						case '\\':
						case '"':
						case '/':		// RFC4627 allows this for some reason
							mapped = *p;
							goto append_mapped;

						case 'b': mapped='\b'; goto append_mapped;
						case 'f': mapped='\f'; goto append_mapped;
						case 'n': mapped='\n'; goto append_mapped;
						case 'r': mapped='\r'; goto append_mapped;
						case 't': mapped='\t';
append_mapped:				Tcl_AppendToObj(out, &mapped, 1);		// Weird, but arranged this way the compiler optimizes it to a jump table
							break;

						/* Works, but bloats code size by about 150 bytes
						case 'b': Tcl_AppendToObj(out, "\b", 1); break;
						case 'f': Tcl_AppendToObj(out, "\f", 1); break;
						case 'n': Tcl_AppendToObj(out, "\n", 1); break;
						case 'r': Tcl_AppendToObj(out, "\r", 1); break;
						case 't': Tcl_AppendToObj(out, "\t", 1); break;
						*/

						/* Works
						case 'b':
						case 'f':
						case 'n':
						case 'r':
						case 't':
							{
								char	decoded[TCL_UTF_MAX+1];
								int		advanced;
								int		len;

								len = Tcl_UtfBackslash(p-1, &advanced, decoded);
								Tcl_AppendToObj(out, decoded, len);
								break;
							}
						*/

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

				*type = stype;
				*val = out;
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
			//if (unlikely(strncmp(p, "true", 4) != 0)) goto err;
			//if (unlikely(e-p < 4 || *(uint32_t*)p != 0x65757274)) goto err;			// Evil little-endian trick
			//if (unlikely(e-p < 4 || *(uint32_t*)p != 0x74727565)) goto err;			// Evil big-endian trick
			if (unlikely(e-p < 4 || *(uint32_t*)p != *(uint32_t*)"true")) goto err;		// Evil endian-compensated trick

			*type = JSON_BOOL;
			*val = l->tcl_true;
			p += 4;
			break;

		case 'f':
			//if (unlikely(strncmp(p, "false", 5) != 0)) goto err;
			//if (unlikely(e-p < 5 || *(uint32_t*)(p+1) != 0x65736c61)) goto err;		// Evil little-endian trick (alse)
			//if (unlikely(e-p < 5 || *(uint32_t*)(p+1) != 0x616c7365)) goto err;		// Evil big-endian trick (alse)
			if (unlikely(e-p < 5 || *(uint32_t*)(p+1) != *(uint32_t*)"alse")) goto err;	// Evil endian-compensated trick

			*type = JSON_BOOL;
			*val = l->tcl_false;
			p += 5;
			break;

		case 'n':
			//if (unlikely(strncmp(p, "null", 4) != 0)) goto err;
			//if (unlikely(e-p < 4 || *(uint32_t*)p != 0x6c6c756e)) goto err;			// Evil little-endian trick
			//if (unlikely(e-p < 4 || *(uint32_t*)p != 0x6e756c6c)) goto err;			// Evil big-endian trick
			if (unlikely(e-p < 4 || *(uint32_t*)p != *(uint32_t*)"null")) goto err;		// Evil endian-compensated trick

			*type = JSON_NULL;
			*val = l->tcl_empty;
			p += 4;
			break;

		default:
			// TODO: Reject leading zero?  The RFC doesn't allow leading zeros
			{
				const unsigned char*	start = p;
				const unsigned char*	t;

				if (*p == '-') p++;

				t = p;
				while (*p >= '0' && *p <= '9') p++;
				if (unlikely(p == t)) goto err;	// No integer part after the decimal point

				if (*p == '.') {	// p could point at the NULL terminator at this point
					p++;
					t = p;
					while (*p >= '0' && *p <= '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the decimal point
				}

				if (unlikely((*p | 0b100000) == 'e')) {	// p could point at the NULL terminator at this point
					p++;
					if (*p == '+' || *p == '-') p++;
					t = p;
					while (*p >= '0' && *p <= '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the exponent symbol
				}

				*type = JSON_NUMBER;
				//*val = Tcl_NewStringObj((const char*)start, p-start);
				*val = new_stringobj_dedup(l, (const char*)start, p-start);
				//*val = p-start < 6 ? new_stringobj_dedup(l, (const char*)start, p-start) : Tcl_NewStringObj((const char*)start, p-start);
			}
	}

	*next = p;
	return TCL_OK;

err:
	if (err_at == NULL)
		err_at = p;

	if (errmsg == NULL)
		errmsg = (err_at == e) ? "Document truncated" : "Illegal character";

	_parse_error(l->interp, errmsg, doc, (err_at - doc) - *char_adj);

	return TCL_ERROR;
}

//}}}
	//}}}
static void free_cx(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	while (cx->last != cx) {
		cx->last = tail->prev;

		if (tail->hold_key != NULL) {
			Tcl_DecrRefCount(tail->hold_key);
			tail->hold_key = NULL;
		}

		if (tail->val != NULL) {
			Tcl_DecrRefCount(tail->val);
			tail->val = NULL;
		}

		ckfree((char*)tail); tail = NULL;
	}
}

//}}}
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]) //{{{
{
	struct interp_cx*		l = cdata;
	const unsigned char*	err_at = NULL;
	const char*				errmsg = "Illegal character";
	size_t					char_adj = 0;		// Offset addjustment to account for multibyte UTF-8 sequences
	const unsigned char*	doc;
	enum json_types			type;
	Tcl_Obj*				val;
	const unsigned char*	p;
	const unsigned char*	e;
	const unsigned char*	val_start;
	int						len;
	struct parse_context	cx;
	Tcl_Obj*				obj = NULL;

	cx.prev = NULL;
	cx.last = &cx;
	cx.hold_key = NULL;
	cx.container = JSON_UNDEF;
	cx.val = NULL;
	cx.char_ofs = 0;

	CHECK_ARGS(1, "json_value");

	p = doc = (const unsigned char*)Tcl_GetStringFromObj(objv[1], &len);
	e = p + len;

	// Skip leading whitespace and comments
	if (skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0) goto whitespace_err;

	while (p < e) {
		//fprintf(stderr, "value_type loop top, byte ofs %d, *p: %c%c\n", p-doc, *p, *(p+1));
		if (cx.last->container == JSON_OBJECT) { // Read the key if in object mode {{{
			const unsigned char*	key_start = p;
			size_t					key_start_char_adj = char_adj;

			TEST_OK(value_type(l, doc, p, e, &char_adj, &p, &type, &val));
			switch (type) {
				case JSON_DYN_STRING:
				case JSON_DYN_LITERAL:
					/* Add back the template format prefix, since we can't store the type
					 * in the dict key.  The template generation code reparses it later.
					 */
					// Can do this because val's ref is on loan from new_stringobj_dedup
					val = Tcl_ObjPrintf("~%c:%s", type == JSON_DYN_STRING ? 'S' : 'L', Tcl_GetString(val));
					// Falls through
				case JSON_STRING:
					Tcl_IncrRefCount(cx.last->hold_key = val);
					break;

				default:
					_parse_error(interp, "Object key is not a string", doc, (key_start-doc) - key_start_char_adj);
					goto err;
			}

			if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

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

			case JSON_DYN_STRING:
			case JSON_DYN_NUMBER:
			case JSON_DYN_BOOL:
			case JSON_DYN_JSON:
			case JSON_DYN_TEMPLATE:
			case JSON_DYN_LITERAL:
			case JSON_STRING:
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
