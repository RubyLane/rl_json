#ifndef _JSON_PARSER_H
#define _JSON_PARSER_H

enum json_types {
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
	JSON_DYN_LITERAL	// ~L:	literal escape - used to quote literal values that start with the above sequences
};

#define KC_ENTRIES		400

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

struct interp_cx {
	Tcl_Interp*		interp;
	Tcl_Obj*		tcl_true;
	Tcl_Obj*		tcl_false;
	Tcl_Obj*		tcl_empty;
	Tcl_HashTable	kc;
	int				kc_count;
	long long		freemap[KC_ENTRIES / (8*sizeof(long long))];	// long long for ffsll
	long long		terminator;
	struct kc_entry	kc_entries[KC_ENTRIES];
};

#define CX_STACK_SIZE	6

int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#endif
