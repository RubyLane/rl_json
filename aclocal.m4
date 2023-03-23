#
# Include the TEA standard macro set
#

builtin(include,tclconfig/tcl.m4)

#
# Add here whatever m4 macros you want to define for your package
#

builtin(include,ax_gcc_builtin.m4)
builtin(include,ax_cc_for_build.m4)
builtin(include,ax_check_compile_flag.m4)

AC_DEFUN([ENABLE_ENSEMBLE], [
	#trap 'echo "val: (${enable_ensemble+set}), ensemble_ok: ($ensemble_ok), ensemble: ($ENSEMBLE)"' DEBUG
	AC_MSG_CHECKING([whether to provide the json command as an ensemble])
	AC_ARG_ENABLE(ensemble,
		AS_HELP_STRING([--enable-ensemble],[Provide the json command using a proper ensemble, otherwise handle the subcommand dispatch internally (default: no)]),
		[ensemble_ok=$enableval], [ensemble_ok=no])

	if test "$ensemble_ok" = "yes" -o "${ENSEMBLE}" = 1; then
		ENSEMBLE=1
		AC_MSG_RESULT([yes])
	else
		ENSEMBLE=0
		AC_MSG_RESULT([no])
	fi

	AC_DEFINE_UNQUOTED([ENSEMBLE], [$ENSEMBLE], [Ensemble enabled?])
	#trap '' DEBUG
])


AC_DEFUN([ENABLE_DEDUP], [
	#trap 'echo "val: (${enable_dedup+set}), dedup_ok: ($dedup_ok), DEDUP: ($DEDUP)"' DEBUG
	AC_MSG_CHECKING([whether to use a string deduplication mechanism for short strings])
	AC_ARG_ENABLE(dedup,
		AS_HELP_STRING([--enable-dedup],[Parsing JSON involves allocating a lot of small string Tcl_Objs, many of which are duplicates.  This mechanism helps reduce that duplication (default: yes)]),
		[dedup_ok=$enableval], [dedup_ok=yes])

	if test "$dedup_ok" = "yes" -o "${DEDUP}" = 1; then
		DEDUP=1
		AC_MSG_RESULT([yes])
	else
		DEDUP=0
		AC_MSG_RESULT([no])
	fi

	AC_DEFINE_UNQUOTED([DEDUP], [$DEDUP], [Dedup enabled?])
	#trap '' DEBUG
])

AC_DEFUN([ENABLE_DEBUG], [
	#trap 'echo "val: (${enable_debug+set}), debug_ok: ($debug_ok), DEBUG: ($DEBUG)"' DEBUG
	AC_MSG_CHECKING([whether to support debuging])
	AC_ARG_ENABLE(debug,
		AS_HELP_STRING([--enable-debug],[Enable debug mode (not symbols, but portions of the code that are only used in debug builds) (default: no)]),
		[debug_ok=$enableval], [debug_ok=no])

	if test "$debug_ok" = "yes" -o "${DEBUG}" = 1; then
		DEBUG=1
		AC_MSG_RESULT([yes])
	else
		DEBUG=0
		AC_MSG_RESULT([no])
	fi

	AC_DEFINE_UNQUOTED([DEBUG], [$DEBUG], [Debug enabled?])
	#trap '' DEBUG
])

AC_DEFUN([ENABLE_UNLOAD], [
	#trap 'echo "val: (${enable_unload+set}), unload_ok: ($unload_ok), UNLOAD: ($UNLOAD)"' DEBUG
	AC_MSG_CHECKING([whether to support unloading])
	AC_ARG_ENABLE(unload,
		AS_HELP_STRING([--enable-unload],[Add support for unloading this shared library (default: no)]),
		[unload_ok=$enableval], [unload_ok=no])

	if test "$unload_ok" = "yes" -o "${UNLOAD}" = 1; then
		UNLOAD=1
		AC_MSG_RESULT([yes])
	else
		UNLOAD=0
		AC_MSG_RESULT([no])
	fi

	AC_DEFINE_UNQUOTED([UNLOAD], [$UNLOAD], [Unload enabled?])
	#trap '' DEBUG
])

AC_DEFUN([TIP445], [
	AC_MSG_CHECKING([whether we need to polyfill TIP 445])
	saved_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $TCL_INCLUDE_SPEC"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <tcl.h>]], [[Tcl_ObjInternalRep ir;]])],[have_tcl_objintrep=yes],[have_tcl_objintrep=no])
	CFLAGS="$saved_CFLAGS"

	if test "$have_tcl_objintrep" = yes; then
		AC_DEFINE(TIP445_SHIM, 0, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([no])
	else
		AC_DEFINE(TIP445_SHIM, 1, [Do we need to polyfill TIP 445?])
		AC_MSG_RESULT([yes])
	fi
])


dnl AC_DEFUN([TEAX_CONFIG_INCLUDE_LINE], [
dnl     eval "$1=\"-I[]CygPath($2)\""
dnl     AC_SUBST($1)])
dnl 
dnl AC_DEFUN([TEAX_CONFIG_LINK_LINE], [
dnl     AS_IF([test ${TCL_LIB_VERSIONS_OK} = nodots], [
dnl 	eval "$1=\"-L[]CygPath($2) -l$3${TCL_TRIM_DOTS}\""
dnl     ], [
dnl 	eval "$1=\"-L[]CygPath($2) -l$3${PACKAGE_VERSION}\""
dnl     ])
dnl     AC_SUBST($1)])
