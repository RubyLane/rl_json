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

#define KC_ENTRIES		384		// Must be an integer multiple of 8*sizeof(long long)

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

struct interp_cx {
	Tcl_Interp*		interp;
	Tcl_Obj*		tcl_true;
	Tcl_Obj*		tcl_false;
	Tcl_Obj*		tcl_empty;
	Tcl_Obj*		json_null;
	Tcl_HashTable	kc;
	int				kc_count;
	long long		freemap[(KC_ENTRIES / (8*sizeof(long long)))+1];	// long long for ffsll
	struct kc_entry	kc_entries[KC_ENTRIES];
};

#define CX_STACK_SIZE	6

void _parse_error(Tcl_Interp* interp, const char* errmsg, const unsigned char* doc, size_t char_ofs);
struct parse_context* push_parse_context(struct parse_context* cx, const int container, const size_t char_ofs);
struct parse_context* pop_parse_context(struct parse_context* cx);
void free_cx(struct parse_context* cx);
int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj);
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val);
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#endif
