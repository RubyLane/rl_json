#ifndef _JSON_MAIN_H
#define _JSON_MAIN_H

#define _GNU_SOURCE

#include <tcl.h>
#include "tclstuff.h"
#include <yajl_version.h>
#include <yajl_common.h>
#include <yajl_gen.h>
#include <yajl_parse.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/time.h>
#include "parser.h"

#define STRING_DEDUP_MAX	32

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif

extern Tcl_ObjType json_type;
extern const char* type_names_dbg[];

struct parse_context {
	struct parse_context*	last;		// Only valid for the first entry
	struct parse_context*	prev;

	Tcl_Obj*	val;
	Tcl_Obj*	hold_key;
	size_t		char_ofs;
	int			container;
};

void append_to_cx(struct parse_context* cx, Tcl_Obj* val);
void append_to_cx2(struct parse_context* cx, Tcl_Obj* val);
Tcl_Obj* JSON_NewJvalObj2(int type, Tcl_Obj* val);

Tcl_Obj* new_stringobj_dedup(struct interp_cx* l, const char* bytes, int length);

#endif
