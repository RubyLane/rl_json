#ifndef _JSON_MAIN_H
#define _JSON_MAIN_H

#include <tcl.h>

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

// Stubs exported API
#include "rl_jsonDecls.h"

EXTERN CONST char* Rl_jsonInitStubs _ANSI_ARGS_((Tcl_Interp* interp, CONST char* version, int exact));
#ifndef USE_TCL_STUBS
#warning Using non-stubs
#	define Rl_jsonInitStubs(interp, version, exact) Tcl_PkgRequire(interp, "rl_json", version, exact)
#endif

EXTERN int Rl_jsonInit _ANSI_ARGS_((Tcl_Interp* interp));

#endif
