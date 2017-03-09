# SYNOPSIS
#
#   AX_SEARCH_LIBS_OPT([function], [libraries-list], [libs-variable], [action-if-found], [action-if-not-found], [extra-libraries])
#
# DESCRIPTION
#
#   Behaves exactly like AC_SEARCH_LIBS except the result is stored in
#   "libs-variable" rather than "LIBS"
#
AC_DEFUN([AX_SEARCH_LIBS_OPT],[
    _ax_old_libs=$LIBS
    LIBS=$$3 $LIBS
    AC_SEARCH_LIBS([$1], [$2], [$4], [$5], [$6])
    AC_SUBST([$3], $LIBS)
    LIBS=$_ax_old_libs
])
