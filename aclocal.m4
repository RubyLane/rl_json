#
# Include the TEA standard macro set
#

builtin(include,tclconfig/tcl.m4)

#
# Add here whatever m4 macros you want to define for your package
#

AC_DEFUN([ENABLE_ENSEMBLE], [
	AC_MSG_CHECKING([whether to provide the json command as an ensemble])
	AC_ARG_ENABLE(ensemble,
		AC_HELP_STRING([--enable-ensemble],
			[Provide the json command using a proper ensemble, otherwise handle the subcommand dispatch internally (default: no)]),
		[ensemble_ok=$enableval], [ensemble_ok=yes])

	if test "ensemble_ok" = "yes" -o "${ENSEMBLE}" = 1; then
		ENSEMBLE=1
		AC_MSG_RESULT([yes])
	else
		ENSEMBLE=0
		AC_MSG_RESULT([no])
	fi

	AC_SUBST(ENSEMBLE)
])

AC_DEFUN([ENABLE_DEDUP], [
	AC_MSG_CHECKING([whether to use a string deduplication mechanism for short strings])
	AC_ARG_ENABLE(dedup,
		AC_HELP_STRING([--enable-dedup],
			[Parsing JSON involves allocating a lot of small string Tcl_Objs, many of which are duplicates.  This mechanism helps reduce that duplication (default: no)]),
		[dedup_ok=$enableval], [dedup_ok=yes])

	if test "dedup_ok" = "yes" -o "${DEDUP}" = 1; then
		DEDUP=1
		AC_MSG_RESULT([yes])
	else
		DEDUP=0
		AC_MSG_RESULT([no])
	fi

	AC_SUBST(DEDUP)
])

AC_DEFUN([TIP445], [
	AC_MSG_CHECKING([whether we need to polyfill TIP 445])
	saved_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $TCL_INCLUDE_SPEC"
	AC_TRY_COMPILE([#include <tcl.h>], [Tcl_ObjIntRep ir;],
	    have_tcl_objintrep=yes, have_tcl_objintrep=no)
	CFLAGS="$saved_CFLAGS"

	if test "$have_tcl_objintrep" = yes; then
		AC_DEFINE(TIP445_SHIM, 0, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([no])
	else
		AC_DEFINE(TIP445_SHIM, 1, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([yes])
	fi
])

AC_DEFUN([TEAX_CONFIG_INCLUDE_LINE], [
    eval "$1=\"-I[]CygPath($2)\""
    AC_SUBST($1)])

AC_DEFUN([TEAX_CONFIG_LINK_LINE], [
    AS_IF([test ${TCL_LIB_VERSIONS_OK} = nodots], [
	eval "$1=\"-L[]CygPath($2) -l$3${TCL_TRIM_DOTS}\""
    ], [
	eval "$1=\"-L[]CygPath($2) -l$3${PACKAGE_VERSION}\""
    ])
    AC_SUBST($1)])
