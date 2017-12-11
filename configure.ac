dnl configure.ac
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([liballocator], [0.0.1], [jajones@nvidia.com])

AC_CONFIG_AUX_DIR([bin])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM

dnl Required LIBDRM versions (same as Mesa's DRI drivers)
LIBDRM_NOUVEAU_REQUIRED=2.4.66

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
AC_CHECK_PROGS([PERL], [perl])

if test -z "$DOXYGEN"; then
    AC_MSG_WARN([Doxygen not found - documentation will not be built])
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

if test "x$ac_cv_prog_cc_c99" = xno; then
        AC_MSG_ERROR([Building liballocator requires a C99-enabled compiler])
fi

dnl Available built-in checks
AX_SEARCH_LIBS_OPT([floor], [m], [MATH_LIBS], [], [
    AC_MSG_ERROR([unable to find the floor() function])
])

AX_SEARCH_LIBS_OPT([dlopen], [dl dld], [DL_LIBS], [], [
    AC_MSG_ERROR([unable to find the dlopen() function])
])

AC_CHECK_FUNC([strdup], [], [
    AC_MSG_ERROR([The function strdup() is required and was not found.])
])

dnl Required libraries checks
PKG_CHECK_MODULES(LIBDRM, [libdrm], [have_libdrm="yes"], [have_libdrm="no"])
if test "x$have_libdrm" != xyes; then
    AC_MSG_WARN([libdrm is a hard build-time dependency for some allocator
                 tests. They will be skipped when building.])
fi
AM_CONDITIONAL([HAVE_LIBDRM], [test "x$have_libdrm" = xyes])

dnl What drivers to build
AC_ARG_WITH([drivers],
    [AS_HELP_STRING([--with-drivers@<:@=DIRS...@:>@],
        [comma delimited allocator drivers list, e.g.
         "nouveau" @<:@default=auto@:>@])],
    [with_drivers="$withval"],
    [with_drivers=auto])

if test "x$with_drivers" = xauto; then
    if test "x$have_libdrm" = xyes; then
        with_drivers="nouveau"
    else
        with_drivers=""
    fi
fi

if test -n "$with_drivers"; then
    drivers=`IFS=', '; echo $with_drivers`
    for driver in $drivers; do
        case "x$driver" in
        xnouveau)
            PKG_CHECK_MODULES(LIBDRM_NOUVEAU, [libdrm >= $LIBDRM_NOUVEAU_REQUIRED libdrm_nouveau >= $LIBDRM_NOUVEAU_REQUIRED],
                              [have_libdrm_nouveau="yes"], [have_libdrm_nouveau="no"])
            if test "x$have_libdrm_nouveau" != xyes; then
                AC_MSG_ERROR([$driver requires libdrm and libdrm_nouveau >= $LIBDRM_NOUVEAU_REQUIRED])
            fi
            ;;
        *)
            AC_MSG_ERROR([Allocator driver '$driver' does not exist])
            ;;
        esac
    done

    dnl Perl is needed to generate drivers json files
    if test -z "$PERL"; then
        AC_MSG_ERROR([Perl is required to generate drivers json files])
    fi
fi
AM_CONDITIONAL([ENABLE_NOUVEAU], [test "x$have_libdrm_nouveau" = xyes])


AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/drivers/Makefile
                 include/Makefile
                 tests/Makefile
                 liballocator.pc])
AM_COND_IF([HAVE_DOXYGEN],
    [AC_CONFIG_FILES([Doxyfile])])
AM_COND_IF([ENABLE_NOUVEAU],
    [AC_CONFIG_FILES([src/drivers/nouveau/Makefile])])

AC_OUTPUT
