# Process this file with autoconf to produce a configure script.
dnl Process this file with autoconf to produce a configure script.

# Note )
#
# AC_DEFINE(VARIABLE, VALUE, DESCRIPTION)
#   output the following to config.h
#   /* DESCRIPTION */
#   #define VARIABLE VALUE
#
# AC_SUBST(VARIABLE, [VALUE])
#   define $(VARIABLE) as VALUE in Makefile

dnl ========================================================================
# initialization
dnl ========================================================================
AC_INIT([e-mod-tizen-keyrouter], [0.1], [duna.oh@samsung.com])

# check for tools needed by automake generated Makefiles
# -Wall  : Turn all warnings on.
# -Werror: report warings as errors.
# foreign: relax some GNU standard requirements
#AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_INIT_AUTOMAKE([-Wall foreign])

dnl ========================================================================
# checks for programs
dnl ========================================================================
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl ========================================================================
# checks for libraries
dnl ========================================================================

dnl ========================================================================
# checks for header files
dnl ========================================================================
#AC_HEADER_STDC
AC_CHECK_HEADERS([math.h fcntl.h stdlib.h string.h unistd.h])

dnl ========================================================================
# checks for typedefs, structures, and compiler characteristics
AC_C_CONST

dnl ========================================================================
# checks for library functions
dnl ========================================================================
#AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])

dnl ========================================================================
# checks for pkg-config
dnl ========================================================================
PKG_PROG_PKG_CONFIG

dnl ========================================================================
# checks for pkg-config
dnl ========================================================================
PKG_CHECK_MODULES(ENLIGHTENMENT, [enlightenment])
ENLIGHTENMENT_CFLAGS="${ENLIGHTENMENT_CFLAGS} -D_GNU_SOURCE "
AC_SUBST(ENLIGHTENMENT_CFLAGS)
AC_SUBST(ENLIGHTENMENT_LIBS)

# Find edje_cc
#PKG_CHECK_MODULES(EDJE, [edje >= 0.5.0])
#AC_ARG_WITH(edje-cc,
#	AC_HELP_STRING([--with-edje-cc=PATH], [specify a specific path to edje_cc]),
#	[
#		v=$withval;
#		EDJE_CC=$v
#	],
#	[
#		EDJE_CC=$(pkg-config --variable=prefix edje)/bin/edje_cc
#	]
#)
#AC_SUBST(EDJE_CC)
#AC_MSG_CHECKING([Which edje_cc to use])
#AC_MSG_RESULT(${EDJE_CC})

# Checks for ttrace header files
PKG_CHECK_MODULES(TTRACE,
		[ttrace],
		[have_ttrace="yes"], [have_ttrace="no"])

if test "x$have_ttrace" = "xyes"; then
	AC_DEFINE(ENABLE_TTRACE, 1, [ttrace available])
fi

dnl ========================================================================
# checks for wayland only argument
dnl ========================================================================
have_wayland_only=no
AC_ARG_ENABLE([wayland-only],
  AS_HELP_STRING([--enable-wayland-only],[enable wayland-only version of enlightenment @<:@default=disabled@:>@]),
  [have_wayland_only=$enableval],
  [have_wayland_only=no])
AC_MSG_CHECKING([whether wayland-only version is enabled])
AM_CONDITIONAL(WAYLAND_ONLY, test x$have_wayland_only = xyes)
if test "x${have_wayland_only}" != "xno"; then
  AC_DEFINE_UNQUOTED([HAVE_WAYLAND_ONLY],[1],[enable wayland-only version of enlightenment])
fi

# to include e_comp_wl.h
if test "x${have_wayland_only}" != "xno"; then
  AC_DEFINE_UNQUOTED([HAVE_WAYLAND],[1],[enable wayland support])
fi

dnl ========================================================================
# checks for wayland only argument
dnl ========================================================================
if test "x${have_wayland_only}" = "xyes"; then
  PKG_CHECK_MODULES(WAYLAND, [wayland-server, tizen-extension-server, capi-system-device])
fi

release=$(pkg-config --variable=release enlightenment)
MODULE_ARCH="$host_os-$host_cpu-$release"
AC_SUBST(MODULE_ARCH)
AC_DEFINE_UNQUOTED(MODULE_ARCH, "$MODULE_ARCH", "Module architecture")

datadir=$(pkg-config --variable=modules enlightenment)/${PACKAGE}
AC_ARG_ENABLE(homedir-install,
	AS_HELP_STRING([--enable-homedir-install], [Install module in homedir]),
	[ datadir="${HOME}/.e/e/modules/${PACKAGE}" ]
)

have_cynara=no
AC_ARG_ENABLE(cynara,
	AS_HELP_STRING([--enable-cynara], [enable cynara]),
	[have_cynara=$enableval],
	[have_cynara=no]
)
if test "x${have_cynara}" = "xyes"; then
	PKG_CHECK_MODULES(CYNARA, [cynara-client, cynara-creds-socket, cynara-session, libsmack])
	AC_DEFINE([ENABLE_CYNARA],[1],[enable cynara])
fi

dnl ========================================================================
# output files
dnl ========================================================================

# create HEADER for all HEADER.in.
# HEADERS contain definitions made with AC_DEFINE.
# the following command will create config.h from config.h.in
AC_CONFIG_HEADERS([config.h])

# create FILE for all FILE.in.
# FILES contains definitions made with AC_SUBST.
AC_CONFIG_FILES([
		 Makefile
		 src/Makefile
		 ])

AC_OUTPUT


