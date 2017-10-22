library rl_json
interface rl_json
declare 0 generic {
	Tcl_Obj *JSON_NewJvalObj(int type, Tcl_Obj *val)
}
declare 1 generic {
	int JSON_GetJvalFromObj(Tcl_Interp *interp, Tcl_Obj *obj, int *type, Tcl_Obj **val)
}
