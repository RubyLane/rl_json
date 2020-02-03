#ifndef _JSON_PARSER_H
#define _JSON_PARSER_H

#include "rl_jsonInt.h"

int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj, enum extensions extensions);
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val, struct parse_error* details);
void parse_error(struct parse_error* details, const char* errmsg, const unsigned char* doc, size_t char_ofs);
void throw_parse_error(Tcl_Interp* interp, struct parse_error* details);
struct parse_context* push_parse_context(struct parse_context* cx, const enum json_types container, const size_t char_ofs);
struct parse_context* pop_parse_context(struct parse_context* cx);
void free_cx(struct parse_context* cx);
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#endif
