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
#include <tclTomMath.h>

#define STRING_DEDUP_MAX	16

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif

enum json_types {		// Order must be preserved
	JSON_UNDEF,
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOL,
	JSON_NULL,

	/* Dynamic types - placeholders for dynamic values in templates */
	JSON_DYN_STRING,	// ~S:
	JSON_DYN_NUMBER,	// ~N:
	JSON_DYN_BOOL,		// ~B:
	JSON_DYN_JSON,		// ~J:
	JSON_DYN_TEMPLATE,	// ~T:
	JSON_DYN_LITERAL,	// ~L:	literal escape - used to quote literal values that start with the above sequences

	JSON_TYPE_MAX		// Not an actual type - records the number of types
};

enum collecting_mode {
	COLLECT_NONE,
	COLLECT_LIST,
	COLLECT_ARRAY,
	COLLECT_OBJECT
};

struct parse_context {
	struct parse_context*	last;		// Only valid for the first entry
	struct parse_context*	prev;

	Tcl_Obj*		val;
	Tcl_Obj*		hold_key;
	size_t			char_ofs;
	enum json_types	container;
	int				closed;
	Tcl_ObjType*	objtype;
};

struct foreach_iterator {
	int				data_c;
	Tcl_Obj**		data_v;
	int				data_i;
	Tcl_Obj*		varlist;
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
	int							collecting;
};

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

#include "rl_jsonDecls.h"
#include "parser.h"

#endif
