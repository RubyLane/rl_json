#ifndef _JSON_MAIN_H
#define _JSON_MAIN_H

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
#include "parser.h"

extern Tcl_ObjType json_type;

struct parse_context {
	struct parse_context*	prev;
	struct parse_context*	last;

	Tcl_Obj*	val;
	int			container;
	Tcl_Obj*	hold_key;
	size_t		char_ofs;
};

void append_to_cx(struct parse_context* cx, Tcl_Obj* val);
Tcl_Obj* JSON_NewJvalObj2(int type, Tcl_Obj* val);

#endif
