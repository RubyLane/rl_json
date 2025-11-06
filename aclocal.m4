#
# Include the TEA standard macro set
#

builtin(include,tclconfig/tcl.m4)

#
# Add here whatever m4 macros you want to define for your package
#

builtin(include,teabase/teabase.m4)

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


AC_DEFUN([ENABLE_CBOR], [
	#trap 'echo "val: (${enable_cbor+set}), cbor_ok: ($cbor_ok), CBOR: ($CBOR)"' DEBUG
	AC_MSG_CHECKING([whether to enable CBOR functionality])
	AC_ARG_ENABLE(cbor,
		AS_HELP_STRING([--enable-cbor],[Enable CBOR (Concise Binary Object Representation) encoding/decoding functionality (default: no)]),
		[cbor_ok=$enableval], [cbor_ok=no])

	if test "$cbor_ok" = "yes" -o "${CBOR}" = 1; then
		CBOR=1
		AC_MSG_RESULT([yes])
	else
		CBOR=0
		AC_MSG_RESULT([no])
	fi

	AC_DEFINE_UNQUOTED([CBOR], [$CBOR], [CBOR enabled?])
	#trap '' DEBUG
])

AC_DEFUN([CygPath],[`${CYGPATH} $1`])

AC_DEFUN([TEAX_CONFIG_INCLUDE_LINE], [
	eval "$1=\"-I[]CygPath($2)\""
	AC_SUBST($1)
])

AC_DEFUN([TEAX_CONFIG_LINK_LINE], [
	AS_IF([test ${TCL_LIB_VERSIONS_OK} = nodots], [
		eval "$1=\"-L[]CygPath($2) -l$3${TCL_TRIM_DOTS}\""
	], [
		eval "$1=\"-L[]CygPath($2) -l$3${PACKAGE_VERSION}\""
	])
	AC_SUBST($1)
])

