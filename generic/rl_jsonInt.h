#ifndef _RL_JSONINT
#define _RL_JSONINT

#include "rl_json.h"
#include "tclstuff.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <tclTomMath.h>
#include "tip445.h"
#include "names.h"

#define CX_STACK_SIZE	6

#ifdef __builtin_expect
#	define likely(exp)   __builtin_expect(!!(exp), 1)
#	define unlikely(exp) __builtin_expect(!!(exp), 0)
#else
#	define likely(exp)   (exp)
#	define unlikely(exp) (exp)
#endif

enum parse_mode {
	PARSE,
	VALIDATE
};

struct parse_context {
	struct parse_context*	last;		// Only valid for the first entry
	struct parse_context*	prev;

	Tcl_Obj*			val;
	Tcl_Obj*			hold_key;
	size_t				char_ofs;
	enum json_types		container;
	int					closed;
	Tcl_ObjType*		objtype;
	struct interp_cx*	l;
	enum parse_mode		mode;
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

enum serialize_modes {
	SERIALIZE_NORMAL,		// We're updating the string rep of a json value or template
	SERIALIZE_TEMPLATE		// We're interpolating values into a template
};

struct serialize_context {
	Tcl_DString*			ds;
	enum serialize_modes	serialize_mode;
	Tcl_Obj*				fromdict;	// NULL if no dict supplied
	struct interp_cx*		l;
	int						allow_null;
};

struct template_cx {
	Tcl_Interp*			interp;
	struct interp_cx*	l;
	Tcl_Obj*			map;
	Tcl_Obj*			actions;
	int					slots_used;
};

struct cx_stack {
	Tcl_Obj*		target;
	Tcl_Obj*		elem;
};

enum modifiers {
	MODIFIER_NONE,
	MODIFIER_LENGTH,	// for arrays and strings: return the length as an int
	MODIFIER_SIZE,		// for objects: return the number of keys as an int
	MODIFIER_TYPE,		// for all types: return the string name as Tcl_Obj
	MODIFIER_KEYS		// for objects: return the keys defined as Tcl_Obj
};

enum action_opcode {
	NOP,
	ALLOCATE,
	FETCH_VALUE,
	DECLARE_LITERAL,
	STORE_STRING,
	STORE_NUMBER,
	STORE_BOOLEAN,
	STORE_JSON,
	STORE_TEMPLATE,
	PUSH_TARGET,
	POP_TARGET,
	REPLACE_VAL,
	REPLACE_ARR,
	REPLACE_ATOM,
	REPLACE_KEY,

	TEMPLATE_ACTIONS_END
};

#if DEDUP
struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

/* Find the best BSF (bit-scan-forward) implementation available:
 * In order of preference:
 *    - __builtin_ffsll     - provided by gcc >= 3.4 and clang >= 5.x
 *    - ffsll               - glibc extension, freebsd libc >= 7.1
 *    - ffs                 - POSIX, but only on int
 * TODO: possibly provide _BitScanForward implementation for Visual Studio >= 2005?
 */
#if defined(HAVE___BUILTIN_FFSLL) || defined(HAVE_FFSLL)
#	define FFS_TMP_STORAGE	/* nothing to declare */
#	if defined(HAVE___BUILTIN_FFSLL)
#		define FFS				__builtin_ffsll
#	else
#		define FFS				ffsll
#	endif
#	define FREEMAP_TYPE		long long
#elif defined(_MSC_VER) && defined(_WIN64) && _MSC_VER >= 1400
#	define FFS_TMP_STORAGE	unsigned long ix;
/* _BitScanForward64 numbers bits starting with 0, ffsll starts with 1 */
#	define FFS(x)			(_BitScanForward64(&ix, x) ? ix+1 : 0)
#	define FREEMAP_TYPE		long long
#else
#	define FFS_TMP_STORAGE	/* nothing to declare */
#	define FFS				ffs
#	define FREEMAP_TYPE		int
#endif


#define KC_ENTRIES		384		// Must be an integer multiple of 8*sizeof(FREEMAP_TYPE)

#endif

struct interp_cx {
	Tcl_Interp*		interp;
	Tcl_Obj*		tcl_true;
	Tcl_Obj*		tcl_false;
	Tcl_Obj*		tcl_empty;
	Tcl_Obj*		tcl_one;
	Tcl_Obj*		tcl_zero;
	Tcl_Obj*		json_true;
	Tcl_Obj*		json_false;
	Tcl_Obj*		json_null;
	Tcl_Obj*		json_empty_string;
	Tcl_Obj*		tcl_empty_dict;
	Tcl_Obj*		tcl_empty_list;
	Tcl_Obj*		action[TEMPLATE_ACTIONS_END];
	Tcl_Obj*		force_num_cmd[3];
	Tcl_Obj*		type_int[JSON_TYPE_MAX];	// Tcl_Obj for JSON_STRING, JSON_ARRAY, etc
	Tcl_Obj*		type[JSON_TYPE_MAX];		// Holds the Tcl_Obj values returned for [json type ...]
#if DEDUP
	Tcl_HashTable	kc;
	int				kc_count;
	FREEMAP_TYPE	freemap[(KC_ENTRIES / (8*sizeof(FREEMAP_TYPE)))+1];	// long long for ffsll
	struct kc_entry	kc_entries[KC_ENTRIES];
#endif
	const Tcl_ObjType*	typeDict;		// Evil hack to identify objects of type dict, used to choose whether to iterate over a list of pairs as a dict or a list, for efficiency

	const Tcl_ObjType*	typeInt;		// Evil hack to snoop on the type of a number, so that we don't have to add 0 to a candidate to know if it's a valid number
	const Tcl_ObjType*	typeDouble;
	const Tcl_ObjType*	typeBignum;
	Tcl_Obj*		apply;
	Tcl_Obj*		decode_bytes;
};

void append_to_cx(struct parse_context *cx, Tcl_Obj *val);
int serialize(Tcl_Interp* interp, struct serialize_context* scx, Tcl_Obj* obj);
void release_instances(void);
int init_types(Tcl_Interp* interp);
Tcl_Obj* new_stringobj_dedup(struct interp_cx *l, const char *bytes, int length);
int lookup_type(Tcl_Interp* interp, Tcl_Obj* typeobj, int* type);
int is_template(const char* s, int len);

extern Tcl_ObjType* g_objtype_for_type[];
extern const char* type_names_int[];

#ifdef TCL_MEM_DEBUG
#	undef JSON_NewJvalObj
Tcl_Obj* JSON_DbNewJvalObj(enum json_types type, Tcl_Obj* val, const char* file, int line);
#	define JSON_NewJvalObj(type, val) JSON_DbNewJvalObj(type, val, __FILE__ " (JVAL)", __LINE__)
#else
Tcl_Obj* JSON_NewJvalObj(enum json_types type, Tcl_Obj* val);
#endif

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

int JSON_SetIntRep(Tcl_Obj* target, enum json_types type, Tcl_Obj* replacement);
int JSON_GetIntrepFromObj(Tcl_Interp* interp, Tcl_Obj* obj, enum json_types* type, Tcl_ObjInternalRep** ir);
int JSON_GetJvalFromObj(Tcl_Interp *interp, Tcl_Obj *obj, enum json_types *type, Tcl_Obj **val);
int JSON_IsJSON(Tcl_Obj* obj, enum json_types* type, Tcl_ObjInternalRep** ir);
int type_is_dynamic(const enum json_types type);
int force_json_number(Tcl_Interp* interp, struct interp_cx* l, Tcl_Obj* obj, Tcl_Obj** forced);
Tcl_Obj* as_json(Tcl_Interp* interp, Tcl_Obj* from);
const char* get_dyn_prefix(enum json_types type);
const char* get_type_name(enum json_types type);
Tcl_Obj* get_unshared_val(Tcl_ObjInternalRep* ir);
int apply_template_actions(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* actions, Tcl_Obj* dict, Tcl_Obj** res);
int build_template_actions(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj** actions);
int convert_to_tcl(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj** out);
int resolve_path(Tcl_Interp* interp, Tcl_Obj* src, Tcl_Obj *const pathv[], int pathc, Tcl_Obj** target, const int exists, const int modifiers, Tcl_Obj* def);
int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent, Tcl_Obj* pad, Tcl_DString* ds);

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

#include "dedup.h"

#endif
