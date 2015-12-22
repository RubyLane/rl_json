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

#define KC_ENTRIES		2048

#define KC_ENTRY_TAILSIZE	(64 - sizeof(Tcl_Obj))
#define KC_ENTRY_MAXPACK	(KC_ENTRY_TAILSIZE - 2)		// tailsize less null byte and hits

struct kc_entry {
	Tcl_Obj			val;
	union {
		unsigned char	hits;
		unsigned char	tail[KC_ENTRY_TAILSIZE];
	};
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

int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#endif
