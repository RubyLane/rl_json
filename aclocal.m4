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

	if test "${enable_ensemble+set}" = set; then
		enableval="$enable_ensemble"
		ensemble_ok=$enableval
	else
		ensemble_ok=yes
	fi

	if test "ensemble_ok" = "yes" -o "${ENSEMBLE}" = 1; then
		ENSEMBLE=1
	else
		ENSEMBLE=0
	fi

	AC_SUBST(ENSEMBLE)
])

AC_DEFUN([ENABLE_DEDUP], [
	AC_MSG_CHECKING([whether to use a string deduplication mechanism for short strings])
	AC_ARG_ENABLE(dedup,
		AC_HELP_STRING([--enable-dedup],
			[Parsing JSON involves allocating a lot of small string Tcl_Objs, many of which are duplicates.  This mechanism helps reduce that duplication (default: no)]),
		[dedup_ok=$enableval], [dedup_ok=yes])

	if test "${enable_dedup+set}" = set; then
		enableval="$enable_dedup"
		dedup_ok=$enableval
	else
		dedup_ok=yes
	fi

	if test "dedup_ok" = "yes" -o "${DEDUP}" = 1; then
		DEDUP=1
	else
		DEDUP=0
	fi

	AC_SUBST(DEDUP)
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
