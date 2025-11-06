#ifndef _JSON_MAIN_H
#define _JSON_MAIN_H

#include <tcl.h>
#include <stdint.h>		// Stubs API uses stdint types

#ifdef BUILD_rl_json
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_rl_json */

enum json_types {		// Order must be preserved
	JSON_UNDEF = 0,
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

enum extensions {
	EXT_NONE     = 0,
	EXT_COMMENTS = (1 << 0)
};

struct parse_error {
	const char*		errmsg;
	const char*		doc;
	size_t			char_ofs;	// Offset in chars, not bytes
};

typedef int (JSON_ForeachBody)(ClientData cdata, Tcl_Interp* interp, Tcl_Obj* loopvars);

#if CBOR
enum cbor_mt {
	M_UINT = 0,
	M_NINT = 1,
	M_BSTR = 2,
	M_UTF8 = 3,
	M_ARR  = 4,
	M_MAP  = 5,
	M_TAG  = 6,
	M_REAL = 7
};
#endif


#ifdef TCL_MEM_DEBUG
#	undef JSON_NewJvalObj
#	define JSON_NewJvalObj(type, val) JSON_DbNewJvalObj(type, val, __FILE__ " (JVAL)", __LINE__)
#endif

// Stubs exported API

#ifdef USE_RL_JSON_STUBS
EXTERN CONST char* Rl_jsonInitStubs _ANSI_ARGS_((Tcl_Interp* interp, CONST char* version, int exact));
#else
#	define Rl_jsonInitStubs(interp, version, exact) Tcl_PkgRequire(interp, "rl_json", version, exact)
#endif
#include "rl_jsonDecls.h"

EXTERN int Rl_json_Init _ANSI_ARGS_((Tcl_Interp* interp));
EXTERN int Rl_json_SafeInit _ANSI_ARGS_((Tcl_Interp* interp));

#endif
