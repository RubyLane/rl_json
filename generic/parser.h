#ifndef _JSON_PARSER_H
#define _JSON_PARSER_H

#include "rl_jsonInt.h"

enum char_advance_status {
	CHAR_ADVANCE_OK,
	CHAR_ADVANCE_UNESCAPED_NULL
};

#define CX_STACK_SIZE	6

void _parse_error(Tcl_Interp* interp, const char* errmsg, const unsigned char* doc, size_t char_ofs);
struct parse_context* push_parse_context(struct parse_context* cx, const enum json_types container, const size_t char_ofs);
struct parse_context* pop_parse_context(struct parse_context* cx);
void free_cx(struct parse_context* cx);
int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj);
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val);
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#define TEMPLATE_TYPE(s, len, out) \
	if (s[0] == '~' && (len) >= 3 && s[2] == ':') { \
		switch (s[1]) { \
			case 'S': out = JSON_DYN_STRING;    break; \
			case 'N': out = JSON_DYN_NUMBER;     break; \
			case 'B': out = JSON_DYN_BOOL;       break; \
			case 'J': out = JSON_DYN_JSON;       break; \
			case 'T': out = JSON_DYN_TEMPLATE;   break; \
			case 'L': out = JSON_DYN_LITERAL;    break; \
			default:  out = JSON_STRING; s -= 3; break; \
		} \
		s += 3; \
	} else {out = JSON_STRING;}

#endif
