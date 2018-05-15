library rl_json
interface rl_json
declare 0 generic {
	Tcl_Obj *JSON_NewJvalObj(int type, Tcl_Obj *val)
}
declare 1 generic {
	int JSON_GetJvalFromObj(Tcl_Interp *interp, Tcl_Obj *obj, int *type, Tcl_Obj **val)
}
declare 2 generic {
	int JSON_Set(Tcl_Interp* interp, Tcl_Obj* srcvar, Tcl_Obj *const pathv[], int pathc, Tcl_Obj* replacement)
}
declare 3 generic {
	int JSON_Template(Tcl_Interp* interp, Tcl_Obj* template, Tcl_Obj* dict, Tcl_Obj** res)
}
