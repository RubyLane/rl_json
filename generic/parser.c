#include "main.h"

void _parse_error(Tcl_Interp* interp, const char* errmsg, const unsigned char* doc, size_t char_ofs) //{{{
{
	const char*	char_ofs_str = Tcl_GetString(Tcl_NewIntObj(char_ofs));

	Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing JSON value: %s at offset %s", errmsg, char_ofs_str));
	Tcl_SetErrorCode(interp, "RL", "JSON", "PARSE", errmsg, doc, char_ofs_str, NULL);
}

//}}}
struct parse_context* push_parse_context(struct parse_context* cx, const int container, const size_t char_ofs) //{{{
{
	struct parse_context*	last = cx->last;
	struct parse_context*	new;
	Tcl_Obj*				ival;

	if (last->container == JSON_UNDEF) {
		new = last;
	} else if (likely((ptrdiff_t)last >= (ptrdiff_t)cx && (ptrdiff_t)last < (ptrdiff_t)(cx + CX_STACK_SIZE - 1))) {
		// Space remains on the cx array stack
		new = cx->last+1;
	} else {
		new = (struct parse_context*)malloc(sizeof(*new));
	}

	ival = JSON_NewJvalObj(container, container == JSON_OBJECT  ?  Tcl_NewDictObj()  :  Tcl_NewListObj(0, NULL));
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
struct parse_context* pop_parse_context(struct parse_context* cx) //{{{
{
	struct parse_context*	last = cx->last;

	cx->last->closed = 1;

	if (unlikely((ptrdiff_t)cx == (ptrdiff_t)last)) {
		return cx->last;
	}

	if (likely(last->val != NULL)) {
		append_to_cx(last->prev, last->val);
		Tcl_DecrRefCount(last->val);
		last->val = NULL;
	}

	if (likely((ptrdiff_t)last >= (ptrdiff_t)cx && (ptrdiff_t)last < (ptrdiff_t)(cx + CX_STACK_SIZE))) {
		// last is on the cx array stack
		cx->last--;
	} else {
		if (last->prev) {
			cx->last = last->prev;
			free(last);
		}
	}

	return cx->last;
}

//}}}
void free_cx(struct parse_context* cx) //{{{
{
	struct parse_context*	tail = cx->last;

	{
		struct parse_context*	t = cx;
		struct parse_context*	next;

		while (1) {
			if (t == cx->last) break;

			next = cx->last;
			while (next->prev != t && next->prev != NULL) next = next->prev;
			t = next;
		}
	}

	while (1) {
		if (tail->hold_key != NULL) {
			Tcl_DecrRefCount(tail->hold_key);
			tail->hold_key = NULL;
		}

		if (tail->val != NULL) {
			Tcl_DecrRefCount(tail->val);
			tail->val = NULL;
		}

		tail = pop_parse_context(cx);

		if (tail == cx) break;
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
		*p += eat;
		*char_adj += eat;
	}
}

//}}}
int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj) //{{{
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

			while (likely(p < e && *p != '\n' && (*p > 0x1f || is_whitespace(*p))))
				char_advance(&p, char_adj);
		} else if (*p == '*') {
			p++;

			while (likely(p < e-2 && *p != '*')) {
				if (unlikely(*p <= 0x1f && !is_whitespace(*p))) goto err_illegal_char;
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
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val) //{{{
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
						out = new_stringobj_dedup(l, (const char*)chunk, len);
					} else if (len > 0) {
						if (unlikely(Tcl_IsShared(out)))
							out = Tcl_DuplicateObj(out);	// Can do this because the ref were were operating under is on loan from new_stringobj_dedup

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
			if (unlikely(e-p < 4 || *(uint32_t*)p != *(uint32_t*)"true")) goto err;		// Evil endian-compensated trick

			*type = JSON_BOOL;
			*val = l->tcl_true;
			p += 4;
			break;

		case 'f':
			if (unlikely(e-p < 5 || *(uint32_t*)(p+1) != *(uint32_t*)"alse")) goto err;	// Evil endian-compensated trick

			*type = JSON_BOOL;
			*val = l->tcl_false;
			p += 5;
			break;

		case 'n':
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
				*val = new_stringobj_dedup(l, (const char*)start, p-start);
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

// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
