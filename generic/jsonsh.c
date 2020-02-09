#include "rl_json.h"

int Parse_args_Init(Tcl_Interp* interp);
int Parse_args_SafeInit(Tcl_Interp* interp);

int Json_AppInit(Tcl_Interp* interp)
{
	if ((Tcl_Init)(interp) == TCL_ERROR)
		return TCL_ERROR;

#ifdef TCL_XT_TEST
	if (Tclxttest_Init(interp) == TCL_ERROR)
		return TCL_ERROR;
#endif

#ifdef TCL_TEST
	if (Tcltest_Init(interp) == TCL_ERROR)
		return TCL_ERROR;
	Tcl_StaticPackage(interp, "Tcltest", Tcltest_Init, Tcltest_SafeInit);
#endif /* TCL_TEST */

	if (Parse_args_Init(interp) != TCL_OK)
		return TCL_ERROR;
	Tcl_StaticPackage(interp, "parse_args", Parse_args_Init, Parse_args_SafeInit);

	if (Rl_json_Init(interp) != TCL_OK)
		return TCL_ERROR;
	Tcl_StaticPackage(interp, "rl_json", Rl_json_Init, Rl_json_SafeInit);

#ifdef DJGPP
	(Tcl_ObjSetVar2)(interp, Tcl_NewStringObj("tcl_rcFileName", -1), NULL,
			Tcl_NewStringObj("~/tclsh.rc", -1), TCL_GLOBAL_ONLY);
#else
	(Tcl_ObjSetVar2)(interp, Tcl_NewStringObj("tcl_rcFileName", -1), NULL,
			Tcl_NewStringObj("~/.tclshrc", -1), TCL_GLOBAL_ONLY);
#endif

	return TCL_OK;
}
