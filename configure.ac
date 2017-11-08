dnl configure.ac
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([liballocator], [0.0.1], [jajones@nvidia.com])

AC_CONFIG_AUX_DIR([bin])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM

dnl Add an --enable-debug option
AX_CHECK_ENABLE_DEBUG(no, DEBUG)

AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 foreign silent-rules subdir-objects])
AM_SILENT_RULES([yes])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_CHECK_PROGS([DOXYGEN], [doxygen])

if test "x$ac_cv_prog_cc_c99" = xno; then
        AC_MSG_ERROR([Building liballocator requires a C99-enabled compiler])
fi

AX_SEARCH_LIBS_OPT([floor], [m], [MATH_LIBS], [], [
    AC_MSG_ERROR([unable to find the floor() function])
])

AX_SEARCH_LIBS_OPT([dlopen], [dl dld], [DL_LIBS], [], [
    AC_MSG_ERROR([unable to find the dlopen() function])
])

AC_CHECK_FUNC([strdup], [], [
    AC_MSG_ERROR([The function strdup() is required and was not found.])
])

if test -z "$DOXYGEN"; then
    AC_MSG_WARN([Doxygen not found - documentation will not be built])
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

PKG_CHECK_MODULES(LIBDRM, [libdrm], [have_libdrm="yes"], [have_libdrm="no"])
if test "x$have_libdrm" != xyes; then
    AC_MSG_WARN([libdrm is a hard build-time dependency for some allocator
                 tests. They will be skipped when building.])
fi
AM_CONDITIONAL([HAVE_LIBDRM], [test "x$have_libdrm" = xyes])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 tests/Makefile
                 liballocator.pc])
AM_COND_IF([HAVE_DOXYGEN],
    [AC_CONFIG_FILES([Doxyfile])])

AC_OUTPUT
