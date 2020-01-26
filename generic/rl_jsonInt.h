#ifndef _RL_JSONINT
#define _RL_JSONINT

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

int JSON_SetIntRep(Tcl_Interp* interp, Tcl_Obj* target, int type, Tcl_Obj* replacement);

int serialize(Tcl_Interp* interp, struct serialize_context* scx, Tcl_Obj* obj);

int init_types(Tcl_Interp* interp);

#endif
