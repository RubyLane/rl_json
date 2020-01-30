#ifndef _RL_JSONINT
#define _RL_JSONINT

#define KC_ENTRIES		384		// Must be an integer multiple of 8*sizeof(long long)

struct kc_entry {
	Tcl_Obj			*val;
	unsigned int	hits;
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
	Tcl_Obj*		action[TEMPLATE_ACTIONS_END];
	Tcl_Obj*		force_num_cmd[3];
	Tcl_Obj*		type_int[JSON_TYPE_MAX];	// Tcl_Obj for JSON_STRING, JSON_ARRAY, etc
	Tcl_Obj*		type[JSON_TYPE_MAX];		// Holds the Tcl_Obj values returned for [json type ...]
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

void append_to_cx(struct parse_context *cx, Tcl_Obj *val);
int serialize(Tcl_Interp* interp, struct serialize_context* scx, Tcl_Obj* obj);
int init_types(Tcl_Interp* interp);
Tcl_Obj* new_stringobj_dedup(struct interp_cx *l, const char *bytes, int length);
int lookup_type(Tcl_Interp* interp, Tcl_Obj* typeobj, int* type);
int is_template(const char* s, int len);

extern Tcl_ObjType* g_objtype_for_type[];
extern const char* type_names_int[];

#ifdef DEDUP
#	define get_string(l, bytes, length)	new_stringobj_dedup(l, bytes, length)
#else
#	define get_string(l, bytes, length) Tcl_NewStringObj(bytes, length)
#endif

#ifdef TCL_MEM_DEBUG
#	undef JSON_NewJvalObj
Tcl_Obj* JSON_DbNewJvalObj(enum json_types type, Tcl_Obj* val, const char* file, int line);
#	define JSON_NewJvalObj(type, val) JSON_DbNewJvalObj(type, val, __FILE__ " (JVAL)", __LINE__)
#endif

#endif
