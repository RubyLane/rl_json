#ifndef _JSON_PARSER_H
#define _JSON_PARSER_H

#define KC_ENTRIES		384		// Must be an integer multiple of 8*sizeof(long long)

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
};

enum action_opcode {
	NOP,
	ALLOCATE_SLOTS,
	ALLOCATE_STACK,
	FETCH_VALUE,
	JVAL_LITERAL,
	JVAL_STRING,
	JVAL_NUMBER,
	JVAL_BOOLEAN,
	JVAL_JSON,
	FILL_SLOT,
	EVALUATE_TEMPLATE,
	CX_OBJ_KEY,
	CX_ARR_IDX,
	POP_CX,
	REPLACE_VAL,
	REPLACE_KEY,

	TEMPLATE_ACTIONS_END
};

enum char_advance_status {
	CHAR_ADVANCE_OK,
	CHAR_ADVANCE_UNESCAPED_NULL
};

struct interp_cx {
	Tcl_Interp*		interp;
	Tcl_Obj*		tcl_true;
	Tcl_Obj*		tcl_false;
	Tcl_Obj*		tcl_empty;
	Tcl_Obj*		tcl_one;
	Tcl_Obj*		json_true;
	Tcl_Obj*		json_false;
	Tcl_Obj*		json_null;
	Tcl_Obj*		json_empty_string;
	Tcl_Obj*		action[TEMPLATE_ACTIONS_END];
	Tcl_Obj*		force_num_cmd[3];
	Tcl_Obj*		type_int[JSON_TYPE_MAX];	// Tcl_Obj for JSON_STRING, JSON_ARRAY, etc
	Tcl_Obj*		type[JSON_TYPE_MAX];		// Holds the Tcl_Obj values returned for [json type ...]
	Tcl_Obj*		templates;
	Tcl_HashTable	kc;
	int				kc_count;
	long long		freemap[(KC_ENTRIES / (8*sizeof(long long)))+1];	// long long for ffsll
	struct kc_entry	kc_entries[KC_ENTRIES];
	const Tcl_ObjType*	typeDict;		// Evil hack to identify objects of type dict, used to choose whether to iterate over a list of pairs as a dict or a list, for efficiency

	const Tcl_ObjType*	typeInt;		// Evil hack to snoop on the type of a number, so that we don't have to add 0 to a candidate to know if it's a valid number
	const Tcl_ObjType*	typeDouble;
	const Tcl_ObjType*	typeBignum;
	Tcl_Obj*		apply;
	Tcl_Obj*		decode_bytes;
};

#define CX_STACK_SIZE	6

void _parse_error(Tcl_Interp* interp, const char* errmsg, const unsigned char* doc, size_t char_ofs);
struct parse_context* push_parse_context(struct parse_context* cx, const enum json_types container, const size_t char_ofs);
struct parse_context* pop_parse_context(struct parse_context* cx);
void free_cx(struct parse_context* cx);
int skip_whitespace(const unsigned char** s, const unsigned char* e, const char** errmsg, const unsigned char** err_at, size_t* char_adj);
int value_type(struct interp_cx* l, const unsigned char* doc, const unsigned char* p, const unsigned char* e, size_t* char_adj, const unsigned char** next, enum json_types *type, Tcl_Obj** val);
int test_parse(ClientData cdata, Tcl_Interp* interp, int objc, Tcl_Obj *const objv[]);

#endif
