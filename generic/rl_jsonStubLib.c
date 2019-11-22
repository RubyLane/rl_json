/*
 * Based on tclOOStubLib.c
 */

#include "rl_json.h"

MODULE_SCOPE const Rl_jsonStubs *rl_jsonStubsPtr;

const Rl_jsonStubs *Rl_jsonStubsPtr = NULL;

#undef rl_jsonInitializeStubs

MODULE_SCOPE const char* rl_jsonInitializeStubs(Tcl_Interp* interp, const char* version)
{
	int				exact = 0;
	const char*		packageName = "rl_json";
	const char* 	errMsg = NULL;
	Rl_jsonStubs*	stubsPtr = NULL;
	const char*		actualVersion = tclStubsPtr->tcl_PkgRequireEx(interp, packageName, version, exact, &stubsPtr);

	if (actualVersion == NULL) {
		return NULL;
	}

	if (stubsPtr == NULL) {
		errMsg = "missing stub table pointer";
		tclStubsPtr->tcl_ResetResult(interp);
		tclStubsPtr->tcl_AppendResult(interp, "Error loading ", packageName,
				" (requested version ", version, ", actual version ",
				actualVersion, "): ", errMsg, NULL);
		return NULL;
	}

	Rl_jsonStubsPtr = stubsPtr;
	return actualVersion;
}

