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
static struct parse_context* push_parse_context(struct parse_context* cx, const int container, const size_t char_ofs) //{{{
{
	struct parse_context*	last = cx->last;
	struct parse_context*	new;
	Tcl_Obj*				ival;

	if (last->container == JSON_UNDEF) {
		//fprintf(stderr, "push_parse_context: at top of stack\n");
		new = last;
	} else if (likely((ptrdiff_t)last >= (ptrdiff_t)cx && (ptrdiff_t)last < (ptrdiff_t)(cx + CX_STACK_SIZE - 1))) {
		// Space remains on the cx array stack
		new = cx->last+1;
		//fprintf(stderr, "Allocated slot on CX stack: %ld / %d\n", new - cx, CX_STACK_SIZE-1);
	} else {
		new = (struct parse_context*)malloc(sizeof(*new));
		//fprintf(stderr, "Allocated slot on heap, stack full\n");
	}

	ival = JSON_NewJvalObj2(container, container == JSON_OBJECT  ?  Tcl_NewDictObj()  :  Tcl_NewListObj(0, NULL));
	Tcl_IncrRefCount(ival);

	new->prev = last;
	new->val = ival;
	new->hold_key = NULL;
	new->char_ofs = char_ofs;
	new->container = container;
	new->closed = 0;

	cx->last = new;

	return new;
}

//}}}
static struct parse_context* pop_parse_context(struct parse_context* cx) //{{{
{
	struct parse_context*	last = cx->last;

	cx->last->closed = 1;

	//fprintf(stderr, "pop_parse_context %s\n", type_names_dbg[last->container]);
	if (unlikely((ptrdiff_t)cx == (ptrdiff_t)last)) {
		//fprintf(stderr, "pop_parse_context: at top of stack\n");
		return cx->last;
	}

	if (likely(last->val != NULL)) {
		//fprintf(stderr, "Appending to prev cx (%s) val (%s)\n", Tcl_GetString(last->prev->val), Tcl_GetString(last->val));
		append_to_cx2(last->prev, last->val);
		Tcl_DecrRefCount(last->val);
		last->val = NULL;
	}

	if (likely((ptrdiff_t)last >= (ptrdiff_t)cx && (ptrdiff_t)last < (ptrdiff_t)(cx + CX_STACK_SIZE))) {
		// last is on the cx array stack
		cx->last--;
		//fprintf(stderr, "Deallocated slot from CX stack, now %ld / %d\n", cx->last - cx, CX_STACK_SIZE-1);
	} else {
		if (last->prev) {
			cx->last = last->prev;
			//fprintf(stderr, "Freeing slot on heap\n");
			free(last);
		}
	}

	return cx->last;
}

//}}}
static void free_cx(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	{
		struct parse_context*	t = cx;
		struct parse_context*	next;

		//fprintf(stderr, "Unwinding parse_context stack after error\n");
		while (1) {
			//fprintf(stderr, "\t%p: %s\n", t, Tcl_GetString(t->val));

			if (t == cx->last) break;

			next = cx->last;
			while (next->prev != t && next->prev != NULL) next = next->prev;
			t = next;
		}
	}

	//fprintf(stderr, "free_cx: %p\n", cx);
	while (1) {
		if (tail->hold_key != NULL) {
			//fprintf(stderr, "Freeing hold_key: \"%s\"\n", Tcl_GetString(tail->hold_key));
			Tcl_DecrRefCount(tail->hold_key);
			tail->hold_key = NULL;
		}

		if (tail->val != NULL) {
			Tcl_DecrRefCount(tail->val);
			tail->val = NULL;
		}

		//fprintf(stderr, "Freeing tail:   %p\n", tail);
		tail = pop_parse_context(cx);
		//fprintf(stderr, "after pop tail: %p\n", tail);

		if (tail == cx) break;
	}
}

//}}}
	// TESTED {{{
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
static int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj) //{{{
{
	const unsigned char*	p = *s;
	const unsigned char*	start;
	size_t					start_char_adj;

consume_space_or_comment:
	while (is_whitespace(*p)) p++;

	if (unlikely(*p == '/')) {
		start = p;
		start_char_adj = *char_adj;
		p++;
		if (*p == '/') {
			p++;

			while (likely(p < e && *p != '\n' && *p > 0x1f))
				char_advance(&p, char_adj);
		} else if (*p == '*') {
			p++;

			while (likely(p < e-2 && *p != '*')) {
				if (unlikely(*p <= 0x1f)) goto err_illegal_char;
				char_advance(&p, char_adj);
			}

			if (unlikely(*p++ != '*' || *p++ != '/')) goto err_unterminated;
		} else {
			goto err_illegal_char;
		}

		goto consume_space_or_comment;
	}

	*s = p;
	return 0;

err_unterminated:
	*err_at = start;
	*char_adj = start_char_adj;
	*errmsg = "Unterminated comment";
	*s = p;
	return 1;

err_illegal_char:
	*err_at = p;
	*errmsg = "Illegal character";
	*s = p;
	return 1;
}

//}}}
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

					// These tests are where the majority of the parsing time is spent
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
	struct parse_context	cx[CX_STACK_SIZE];
	Tcl_Obj*				obj = NULL;

	cx[0].prev = NULL;
	cx[0].last = cx;
	cx[0].hold_key = NULL;
	cx[0].container = JSON_UNDEF;
	cx[0].val = NULL;
	cx[0].char_ofs = 0;
	cx[0].closed = 0;

	CHECK_ARGS(1, "json_value");

	p = doc = (const unsigned char*)Tcl_GetStringFromObj(objv[1], &len);
	e = p + len;

	//fprintf(stderr, "start parse -----------------------\n%s\n-----------------------------------\n", doc);

	// Skip leading whitespace and comments
	if (skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0) goto whitespace_err;

	while (p < e) {
		//fprintf(stderr, "value_type loop top, byte ofs %d, *p: %c%c\n", p-doc, *p, *(p+1));
		if (cx[0].last->container == JSON_OBJECT) { // Read the key if in object mode {{{
			const unsigned char*	key_start = p;
			size_t					key_start_char_adj = char_adj;

			if (value_type(l, doc, p, e, &char_adj, &p, &type, &val) != TCL_OK) goto err;

			//fprintf(stderr, "Got obj key \"%s\", type %s\n", Tcl_GetString(val), type_names_dbg[type]);

			switch (type) {
				case JSON_DYN_STRING:
				case JSON_DYN_NUMBER:
				case JSON_DYN_BOOL:
				case JSON_DYN_JSON:
				case JSON_DYN_TEMPLATE:
				case JSON_DYN_LITERAL:
					/* Add back the template format prefix, since we can't store the type
					 * in the dict key.  The template generation code reparses it later.
					 */
					// Can do this because val's ref is on loan from new_stringobj_dedup
					val = Tcl_ObjPrintf("~%c:%s", key_start[2], Tcl_GetString(val));
					// Falls through
				case JSON_STRING:
					//fprintf(stderr, "Storing hold_key %p: \"%s\"\n", val, Tcl_GetString(val));
					Tcl_IncrRefCount(cx[0].last->hold_key = val);
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
		if (value_type(l, doc, p, e, &char_adj, &p, &type, &val) != TCL_OK) goto err;

		switch (type) {
			case JSON_OBJECT:
				push_parse_context(cx, JSON_OBJECT, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == '}') {
					pop_parse_context(cx);
					p++;
					goto after_value;
				}
				continue;

			case JSON_ARRAY:
				push_parse_context(cx, JSON_ARRAY, (val_start - doc) - char_adj);
				if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;

				if (*p == ']') {
					pop_parse_context(cx);
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
				append_to_cx2(cx->last, JSON_NewJvalObj2(type, val));
				break;

			default:
				free_cx(cx);
				THROW_ERROR("Unexpected json value type: ", Tcl_GetString(Tcl_NewIntObj(type)));
		}

after_value:	// Yeah, goto.  But the alternative abusing loops was worse
		if (unlikely(skip_whitespace(&p, e, &errmsg, &err_at, &char_adj) != 0)) goto whitespace_err;
		if (p >= e) break;

		switch (cx[0].last->container) { // Handle eof and end-of-context or comma for array and object {{{
			case JSON_OBJECT:
				if (*p == '}') {
					pop_parse_context(cx);
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
					pop_parse_context(cx);
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

	if (unlikely(cx != cx[0].last || !cx[0].closed)) { // Unterminated object or array context {{{
		switch (cx[0].last->container) {
			case JSON_OBJECT:
				_parse_error(interp, "Unterminated object", doc, cx[0].last->char_ofs);
				goto err;

			case JSON_ARRAY:
				_parse_error(interp, "Unterminated array", doc, cx[0].last->char_ofs);
				goto err;
		}
	}
	//}}}

	// TESTED {{{
	if (unlikely(cx[0].val == NULL)) {
		err_at = doc;
		errmsg = "No JSON value found";
		goto whitespace_err;
	}

	obj = Tcl_NewObj();
	obj->typePtr = &json_type;
	obj->internalRep.ptrAndLongRep.value = cx[0].val->internalRep.ptrAndLongRep.value;
	obj->internalRep.ptrAndLongRep.ptr = cx[0].val->internalRep.ptrAndLongRep.ptr;
	Tcl_InvalidateStringRep(obj);

	// We're transferring the ref from cx[0].val to our intrep
	//Tcl_IncrRefcount(obj->internalRep.ptrAndLongRep.ptr
	//Tcl_DecrRefCount(cx[0].val);
	cx[0].val = NULL;

	Tcl_SetObjResult(interp, obj);
	return TCL_OK;

whitespace_err:
	_parse_error(interp, errmsg, doc, (err_at - doc) - char_adj);

	//}}}
err:
	free_cx(cx);
	return TCL_ERROR;
}

//}}}

// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
