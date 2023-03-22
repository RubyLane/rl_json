#include "rl_jsonInt.h"
#include "parser.h"

enum char_advance_status {
	CHAR_ADVANCE_OK,
	CHAR_ADVANCE_UNESCAPED_NULL
};

void parse_error(struct parse_error* details, const char* errmsg, const unsigned char* doc, size_t char_ofs) //{{{
{
	if (details == NULL) return;

	details->errmsg = errmsg;
	details->doc = (const char*)doc;
	details->char_ofs = char_ofs;
}

//}}}
void throw_parse_error(Tcl_Interp* interp, struct parse_error* details) //{{{
{
	char		char_ofs_buf[20];		// 20 bytes allows for 19 bytes of decimal max 64 bit size_t, plus null terminator

	snprintf(char_ofs_buf, 20, "%ld", details->char_ofs);

	Tcl_SetObjResult(interp, Tcl_ObjPrintf("Error parsing JSON value: %s at offset %ld", details->errmsg, details->char_ofs));
	Tcl_SetErrorCode(interp, "RL", "JSON", "PARSE", details->errmsg, details->doc, char_ofs_buf, NULL);
}

//}}}
struct parse_context* push_parse_context(struct parse_context* cx, const enum json_types container, const size_t char_ofs) //{{{
{
	struct parse_context*	last = cx->last;
	struct parse_context*	new;

	if (last->container == JSON_UNDEF) {
		new = last;
	} else if (likely((ptrdiff_t)last >= (ptrdiff_t)cx && (ptrdiff_t)last < (ptrdiff_t)(cx + CX_STACK_SIZE - 1))) {
		// Space remains on the cx array stack
		new = cx->last+1;
	} else {
		new = (struct parse_context*)malloc(sizeof(*new));
	}


	new->prev = last;
	if (last->mode == VALIDATE) {
		new->val = NULL;
	} else {
		Tcl_IncrRefCount(
			new->val = JSON_NewJvalObj(container, container == JSON_OBJECT  ?
				(cx->l ? cx->l->tcl_empty_dict : Tcl_NewDictObj())  :
				(cx->l ? cx->l->tcl_empty_list : Tcl_NewListObj(0, NULL))
		));
	}
	new->hold_key = NULL;
	new->char_ofs = char_ofs;
	new->container = container;
	new->closed = 0;
	new->objtype = g_objtype_for_type[container];
	new->l = last->l;
	new->mode = last->mode;

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
		if (last->prev->val && Tcl_IsShared(last->prev->val))
			replace_tclobj(&last->prev->val, Tcl_DuplicateObj(last->prev->val));

		append_to_cx(last->prev, last->val);
		release_tclobj(&last->val);
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

	while (1) {
		release_tclobj(&tail->hold_key);
		release_tclobj(&tail->val);

		if (tail == cx) break;

		tail = pop_parse_context(cx);
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
static enum char_advance_status char_advance(const unsigned char** p, size_t* char_adj) //{{{
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
		if (first == 0xC0 && **p == 0x80) {
			(*p)--;
			// Have to check for this here - other unescaped control chars are handled by the < 0x1F
			// test, but 0x00 is transformed to 0xC0 0x80 by Tcl (MUTF-8 rather than UTF-8)
			return CHAR_ADVANCE_UNESCAPED_NULL;
		}
		if (first < 0xe0 /* 0b11100000 */) {
			eat = 1;
#if TCL_UTF_MAX == 3
		} else {
			eat = 2;
#else
		} else if (first < 0xf0 /* 0b11110000 */) {
			eat = 2;
		} else if (first < 0xf8 /* 0b11111000 */) {
			eat = 3;
		} else if (first < 0xfc /* 0b11111100 */) {
			eat = 4;
		} else {
			eat = 5;
#endif
		}
		*p += eat;
		*char_adj += eat;
	}

	return CHAR_ADVANCE_OK;
}

//}}}
int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj, enum extensions extensions) //{{{
{
	const unsigned char*		p = *s;
	const unsigned char*		start;
	size_t						start_char_adj;
	enum char_advance_status	status = CHAR_ADVANCE_OK;

consume_space_or_comment:
	while (is_whitespace(*p)) p++;

	if (unlikely((extensions & EXT_COMMENTS) && *p == '/')) {
		start = p;
		start_char_adj = *char_adj;
		p++;
		if (*p == '/') {
			p++;

			while (likely(p < e && (*p > 0x1f || *p == 0x09) && status == CHAR_ADVANCE_OK))
				status = char_advance(&p, char_adj);
		} else if (*p == '*') {
			p++;

			while (likely(p < e-2 && *p != '*')) {
				if (unlikely(*p <= 0x1f && !is_whitespace(*p))) goto err_illegal_char;
				status = char_advance(&p, char_adj);
				if (unlikely(status != CHAR_ADVANCE_OK)) goto err_illegal_char;
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
int is_template(const char* s, int len) //{{{
{
	if (
		len >= 3 &&
		s[0] == '~' &&
		s[2] == ':'
	) {
		switch (s[1]) {
			case 'S':
			case 'N':
			case 'B':
			case 'J':
			case 'T':
			case 'L':
				return 1;
		}
	}

	return 0;
}

//}}}
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val, struct parse_error* details) //{{{
{
	const unsigned char*	err_at = NULL;
	const char*				errmsg = NULL;
	Tcl_Obj*				out = NULL;

	if (val)
		replace_tclobj(val, NULL);

	if (unlikely(p >= e)) goto err;

	switch (*p) {
		case '"':
			p++;	// Advance past the " to the first byte of the string
			{
				const unsigned char*		chunk;
				size_t						len;
				char						mapped;
				enum json_types				stype = JSON_STRING;
				enum char_advance_status	status = CHAR_ADVANCE_OK;

				// Peek ahead to detect template subst markers.
				TEMPLATE_TYPE(p, e-p, stype);

				while (1) {
					chunk = p;

					// These tests are where the majority of the parsing time is spent
					while (likely(p < e && *p != '"' && *p != '\\' && *p > 0x1f && status == CHAR_ADVANCE_OK))
						status = char_advance(&p, char_adj);

					if (unlikely(p >= e)) goto err;

					len = p-chunk;

					if (likely(out == NULL)) {
						replace_tclobj(&out, get_string(l, (const char*)chunk, len));
					} else if (len > 0) {
						if (unlikely(Tcl_IsShared(out)))
							replace_tclobj(&out, Tcl_DuplicateObj(out));

						Tcl_AppendToObj(out, (const char*)chunk, len);
					}

					if (likely(*p == '"')) {
						p++;	// Point at the first byte after the string
						break;
					}

					if (unlikely(*p != '\\')) goto err;

					p++;	// Advance to the backquoted byte

					if (unlikely(Tcl_IsShared(out)))
						replace_tclobj(&out, Tcl_DuplicateObj(out));

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
								Tcl_UniChar	acc = 0;
								char		utfbuf[6];
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

								if (
									(acc >= 0xD800 && acc <= 0xDBFF) ||
									(acc >= 0xDC00 && acc <= 0xDFFF)
								) {
									// Replace invalid codepoints (in the high and low surrogate ranges for UTF-16) with
									// U+FFFD in accordance with Unicode recommendations
									acc = 0xFFFD;
								}
								//const unsigned char* utfend = output_utf8(acc, utfbuf);
								const int len = Tcl_UniCharToUtf(acc, utfbuf);
								Tcl_AppendToObj(out, utfbuf, len);
							}
							break;

						default:
							goto err;
					}
					p++;	// Advance to the first byte after the backquoted sequence
				}

				*type = stype;
				if (val)
					replace_tclobj(val, out);
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
			if (val)
				replace_tclobj(val, l ? l->tcl_true : Tcl_NewBooleanObj(1));
			p += 4;
			break;

		case 'f':
			if (unlikely(e-p < 5 || *(uint32_t*)(p+1) != *(uint32_t*)"alse")) goto err;	// Evil endian-compensated trick

			*type = JSON_BOOL;
			if (val)
				replace_tclobj(val, l ? l->tcl_false : Tcl_NewBooleanObj(0));
			p += 5;
			break;

		case 'n':
			if (unlikely(e-p < 4 || *(uint32_t*)p != *(uint32_t*)"null")) goto err;		// Evil endian-compensated trick

			*type = JSON_NULL;
			if (val)
				replace_tclobj(val, l ? l->tcl_empty : Tcl_NewStringObj("", 0));
			p += 4;
			break;

		default:
			{
				const unsigned char*	start = p;
				const unsigned char*	t;

				if (*p == '-') p++;

				if (unlikely(
						*p == '0' && (
							// Only 3 characters can legally follow a leading '0' according to the spec:
							// . - fraction, e or E - exponent
							(p[1] >= '0' && p[1] <= '9')		// Octal, hex, or decimal with leading 0
						)
				)) {
					// Indexing one beyond p is safe - all the strings we
					// receive are guaranteed to be null terminated by Tcl, and
					// *p here is '0'
					err_at = p;
					errmsg = "Leading 0 not allowed for numbers";
					goto err;
				}

				t = p;
				while (*p >= '0' && *p <= '9') p++;
				if (unlikely(p == t)) goto err;	// No integer part after the decimal point

				if (*p == '.') {	// p could point at the NULL terminator at this point
					p++;
					t = p;
					while (*p >= '0' && *p <= '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the decimal point
				}

				if (unlikely((*p | 0x20 /* 0b100000 */) == 'e')) {	// p could point at the NULL terminator at this point
					p++;
					if (*p == '+' || *p == '-') p++;
					t = p;
					while (*p >= '0' && *p <= '9') p++;
					if (unlikely(p == t)) goto err;	// No integer part after the exponent symbol
				}

				*type = JSON_NUMBER;
				if (val)
					replace_tclobj(val, get_string(l, (const char*)start, p-start));
			}
	}

	release_tclobj(&out);
	*next = p;
	return TCL_OK;

err:
	release_tclobj(&out);

	if (err_at == NULL)
		err_at = p;

	if (errmsg == NULL)
		errmsg = (err_at == e) ? "Document truncated" : "Illegal character";

	parse_error(details, errmsg, doc, (err_at - doc) - *char_adj);

	return TCL_ERROR;
}

//}}}

// vim: foldmethod=marker foldmarker={{{,}}} ts=4 shiftwidth=4
