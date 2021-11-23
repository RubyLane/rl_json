// Written by Cyan Ogilvie, and placed in the public domain

#ifndef _TCLSTUFF_H
#define _TCLSTUFF_H

#include "tcl.h"

#define NEW_CMD( tcl_cmd, c_cmd ) \
	Tcl_CreateObjCommand( interp, tcl_cmd, \
			(Tcl_ObjCmdProc *) c_cmd, \
			(ClientData *) NULL, NULL );

#define THROW_ERROR( ... )								\
	{													\
		Tcl_AppendResult(interp, ##__VA_ARGS__, NULL);	\
		return TCL_ERROR;								\
	}

#define THROW_PRINTF( fmtstr, ... )											\
	{																		\
		Tcl_SetObjResult(interp, Tcl_ObjPrintf((fmtstr), ##__VA_ARGS__));	\
		return TCL_ERROR;													\
	}

#define THROW_ERROR_LABEL( label, var, ... )				\
	{														\
		Tcl_AppendResult(interp, ##__VA_ARGS__, NULL);		\
		var = TCL_ERROR;									\
		goto label;											\
	}

// convenience macro to check the number of arguments passed to a function
// implementing a tcl command against the number expected, and to throw
// a tcl error if they don't match.  Note that the value of expected does
// not include the objv[0] object (the function itself)
#define CHECK_ARGS(expected, msg)										\
	if (objc != expected + 1) {											\
		Tcl_ResetResult( interp );										\
		Tcl_AppendResult( interp, "Wrong # of arguments.  Must be \"",	\
						  msg, "\"", NULL );							\
		return TCL_ERROR;												\
	}


// A rather frivolous macro that just enhances readability for a common case
#define TEST_OK( cmd )		\
	if (cmd != TCL_OK) return TCL_ERROR;

#define TEST_OK_LABEL( label, var, cmd )		\
	if (cmd != TCL_OK) { \
		var = TCL_ERROR; \
		goto label; \
	}

#define TEST_OK_BREAK(var, cmd) if (TCL_OK != (var=(cmd))) break;

static inline void release_tclobj(Tcl_Obj** obj)
{
	if (*obj) {
		Tcl_DecrRefCount(*obj);
		*obj = NULL;
	}
}
#define RELEASE_MACRO(obj)		if (obj) {Tcl_DecrRefCount(obj); obj=NULL;}
#define REPLACE_MACRO(target, replacement)	\
{ \
	release_tclobj(&target); \
	if (replacement) Tcl_IncrRefCount(target = replacement); \
}
static inline void replace_tclobj(Tcl_Obj** target, Tcl_Obj* replacement)
{
	if (*target) {
		Tcl_DecrRefCount(*target);
		*target = NULL;
	}
	*target = replacement;
	if (*target) Tcl_IncrRefCount(*target);
}

#if DEBUG
#	 include <signal.h>
#	 include <unistd.h>
#	 include <time.h>
#	 include "names.h"
#	 define DBG(...) fprintf(stdout, ##__VA_ARGS__)
#	 define FDBG(...) fprintf(stdout, ##__VA_ARGS__)
#	 define DEBUGGER raise(SIGTRAP)
#	 define TIME(label, task) \
	do { \
		struct timespec first; \
		struct timespec second; \
		struct timespec after; \
		double empty; \
		double delta; \
		clock_gettime(CLOCK_MONOTONIC, &first); /* Warm up the call */ \
		clock_gettime(CLOCK_MONOTONIC, &first); \
		clock_gettime(CLOCK_MONOTONIC, &second); \
		task; \
		clock_gettime(CLOCK_MONOTONIC, &after); \
		empty = second.tv_sec - first.tv_sec + (second.tv_nsec - first.tv_nsec)/1e9; \
		delta = after.tv_sec - second.tv_sec + (after.tv_nsec - second.tv_nsec)/1e9 - empty; \
		DBG("Time for %s: %.1f microseconds\n", label, delta * 1e6); \
	} while(0)
#else
#	define DBG(...) /* nop */
#	define FDBG(...) /* nop */
#	define DEBUGGER /* nop */
#	define TIME(label, task) task
#endif

#endif
