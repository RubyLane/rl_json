library rl_json
interface rl_json
declare 0 generic {
	Tcl_Obj *JSON_NewJvalObj(enum json_types type, Tcl_Obj *val)
}
declare 1 generic {
	int JSON_GetJvalFromObj(Tcl_Interp *interp, Tcl_Obj *obj, enum json_types *type, Tcl_Obj **val)
}
declare 2 generic {
	int JSON_Set(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc, Tcl_Obj* replacement)
}
declare 3 generic {
	int JSON_Template(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* dict, Tcl_Obj** res)
}
declare 4 generic {
	int JSON_Unset(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc)
}
declare 5 generic {
	int JSON_GetIntrepFromObj(Tcl_Interp* interp, Tcl_Obj* obj, enum json_types* type, Tcl_ObjIntRep** ir)
}
declare 6 generic {
	int JSON_IsJSON(Tcl_Obj* obj, enum json_types* type, Tcl_ObjIntRep** ir)
}
declare 7 generic {
	int JSON_SetIntRep(Tcl_Obj* target, enum json_types type, Tcl_Obj* replacement)
}
declare 8 generic {
	int JSON_ForceJSON(Tcl_Interp* interp, Tcl_Obj* obj)
}
