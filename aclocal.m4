#
# Include the TEA standard macro set
#

AC_PREREQ(2.50)
builtin(include,tclconfig/tcl.m4)

dnl: aclocal.m4 --
dnl:
dnl:	Macros used for controlling output of autoconf. Some are general
dnl:	library macros, others are things that are probably eventually going
dnl:	to end up in TEA. All have the prefix of TEAX (TEA eXtension).
dnl:
dnl: Copyright (c) 2007-2008 by Donal K. Fellows
dnl:
dnl: See the file "license.terms" for information on usage and redistribution
dnl: of this file, and for a DISCLAIMER OF ALL WARRANTIES.

dnl Helper macros
AC_DEFUN([TEAX_LAPPEND], [$1="[$]{$1} $2"])
AC_DEFUN([TEAX_FOREACH], [for $1 in $2; do $3; done])
AC_DEFUN([TEAX_IFEQ], [AS_IF([test "x$1" = "x$2"], [$3])])
AC_DEFUN([TEAX_IFNEQ], [AS_IF([test "x$1" != "x$2"], [$3])])
AC_DEFUN([TEAX_SWITCH], [case "$1" in TEAX_SWITCH_Cases(m4_shift($@)) esac])
AC_DEFUN([TEAX_SWITCH_Cases], [m4_if([$#],0,,[$#],1,,[TEAX_SWITCH_OneCase([$1],[$2])TEAX_SWITCH_Cases(m4_shift(m4_shift($@)))])])
AC_DEFUN([TEAX_SWITCH_OneCase],[ $1) $2;;])
AC_DEFUN([CygPath],[`${CYGPATH} $1`])

dnl Interesting macros
AC_DEFUN([TEAX_INCLUDE_DIR], [
    TEAX_LAPPEND(TeaXIncludeDirs, $1)
    TEA_ADD_INCLUDES([-I\"]CygPath($1)[\"])])
AC_DEFUN([TEAX_SUBST_RESOURCE], [
    AC_REQUIRE([TEA_CONFIG_CFLAGS])dnl
    TEAX_IFEQ($TEA_PLATFORM, windows, [
	AC_CHECK_PROGS(RC_, 'windres -o' 'rc -nologo -fo', none)
	TEAX_SWITCH($RC_,
	    windres*, [
		rcdef_inc="--include "
		rcdef_start="--define "
		rcdef_q='\"'
		AC_SUBST(RES_SUFFIX, [res.o])
		TEAX_LAPPEND(PKG_OBJECTS, ${PACKAGE_NAME}.res.o)],
	    rc*, [
dnl		rcdef_inc="-i "
dnl		rcdef_start="-d "
dnl		rcdef_q='"'
dnl		AC_SUBST(RES_SUFFIX, [res])
dnl		TEAX_LAPPEND(PKG_OBJECTS, ${PACKAGE_NAME}.res)
		AC_MSG_WARN([resource compiler problems; skipping...])
		RC_=: ],
	    *, [
		AC_MSG_WARN([could not find resource compiler])
		RC_=: ])])
    # This next line is because of the brokenness of TEA...
    AC_SUBST(RC, $RC_)
    TEAX_FOREACH(i, $TeaXIncludeDirs, [
	TEAX_LAPPEND(RES_DEFS, ${rcdef_inc}\"CygPath($i)\")])
    TEAX_FOREACH(i, $1, [
	TEAX_LAPPEND(RES_DEFS, ${rcdef_start}$i='${rcdef_q}\$($i)${rcdef_q}')])
    AC_SUBST(RES_DEFS)])
AC_DEFUN([TEAX_ADD_PRIVATE_HEADERS], [
    TEAX_FOREACH(i, $@, [
	# check for existence, be strict because it should be present!
	AS_IF([test ! -f "${srcdir}/$i"], [
	    AC_MSG_ERROR([could not find header file '${srcdir}/$i'])])
	TEAX_LAPPEND(PKG_PRIVATE_HEADERS, $i)])
    AC_SUBST(PKG_PRIVATE_HEADERS)])
dnl Extra magic to make things work with Vista and VC
AC_DEFUN([TEAX_VC_MANIFEST], [
    CC_OUT="-o [\$]@"
    AC_SUBST(CC_OUT)
    AS_IF([test ${TEA_PLATFORM} = windows \
	    -a "$GCC" != yes \
	    -a "${SHARED_BUILD}" = 1], [
	# This refers to "Manifest Tool" not "Magnetic Tape utility"
	AC_CHECK_PROGS(MT, "mt -nologo", none)
	TEAX_IFNEQ($MT, none, [
	    CC_OUT="-Fo[\$]@"
	    ADD_MANIFEST="${MT} -manifest [\$]@.manifest -outputresource:[\$]@\;2"
	    AC_SUBST(ADD_MANIFEST)
	    TEAX_LAPPEND(CLEANFILES, ${PKG_LIB_FILE}.manifest)])])])
AC_DEFUN([TEAX_SDX], [
    AC_ARG_WITH([sdx], [AS_HELP_STRING([--with-sdx],
[where to find the Starkit Developer Extensions (default: search path)])], [:],
[with_sdx=search])
    TEAX_SWITCH($with_sdx,
	no, [
	    AC_MSG_NOTICE([configured without sdx; building starkits will fail])
	    AC_MSG_NOTICE([building as a normal library still supported])],
	search, [
	    AC_PATH_PROG([SDX], [sdx], [none])
	    TEAX_IFEQ($SDX, none, [
		AC_PATH_PROG(SDX_KIT, sdx.kit, none)
		TEAX_IFNEQ($SDX_KIT, none, [
		    # We assume that sdx.kit is on the path, and that the
		    # default tclsh is activetcl
		    SDX="tclsh '${SDX_KIT}'"])])
	    TEAX_IFEQ($SDX, none, [
		AC_MSG_WARN([cannot find sdx; building starkits will fail])
		AC_MSG_NOTICE([building as a normal library still supported])])],
	*, [
	    AC_PATH_PROG(SDX, $with_sdx, none)
	    TEAX_IFEQ($SDX, none, [
		AC_MSG_WARN([cannot find $with_sdx; building starkits may fail])
		AC_MSG_NOTICE([building as a normal library still supported])])])])
dnl TODO: Adapt this for OSX Frameworks...
dnl This next bit is a bit ugly, but it makes things for tclooConfig.sh...
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

dnl Local Variables:
dnl mode: autoconf
dnl End:
