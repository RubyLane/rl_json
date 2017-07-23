#ifndef _JSON_MAIN_H
#define _JSON_MAIN_H

#define _GNU_SOURCE

#include <tcl.h>
#include "tclstuff.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include "parser.h"

#define STRING_DEDUP_MAX	16

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
	int			closed;
};

struct foreach_iterator {
	int				data_c;
	Tcl_Obj**		data_v;
	int				data_i;
	int				var_c;
	Tcl_Obj**		var_v;
	int				is_array;

	// Dict search related state - when iterating over JSON objects
	Tcl_DictSearch	search;
	Tcl_Obj*		k;
	Tcl_Obj*		v;
	int				done;
};

struct foreach_state {
	unsigned int				loop_num;
	unsigned int				max_loops;
	unsigned int				iterators;
	struct foreach_iterator*	it;
	Tcl_Obj*					script;
	Tcl_Obj*					res;
};

void append_to_cx(struct parse_context* cx, Tcl_Obj* val);
Tcl_Obj* JSON_NewJvalObj(int type, Tcl_Obj* val);

Tcl_Obj* new_stringobj_dedup(struct interp_cx* l, const char* bytes, int length);


// Taken from tclInt.h:
#if !defined(INT2PTR) && !defined(PTR2INT)
#   if defined(HAVE_INTPTR_T) || defined(intptr_t)
#       define INT2PTR(p) ((void *)(intptr_t)(p))
#       define PTR2INT(p) ((int)(intptr_t)(p))
#   else
#       define INT2PTR(p) ((void *)(p))
#       define PTR2INT(p) ((int)(p))
#   endif
#endif
#if !defined(UINT2PTR) && !defined(PTR2UINT)
#   if defined(HAVE_UINTPTR_T) || defined(uintptr_t)
#       define UINT2PTR(p) ((void *)(uintptr_t)(p))
#       define PTR2UINT(p) ((unsigned int)(uintptr_t)(p))
#   else
#       define UINT2PTR(p) ((void *)(p))
#       define PTR2UINT(p) ((unsigned int)(p))
#   endif
#endif


#endif
