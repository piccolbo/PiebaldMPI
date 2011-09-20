AC_INIT([PiebaldMPI], 0.1.0)

# Checks for compiler warning types

# AC_CHECK_CC_FLAG(FLAG_TO_TEST, VARIABLE_TO_SET_IF_SUPPORTED)
# ---------
AC_DEFUN([AC_CHECK_CC_FLAG],
[{
	AC_LANG_PUSH([C])
	my_cflags_save="$CFLAGS"
	CFLAGS="$my_cflags_save $1"
	AC_MSG_CHECKING([whether $CC accepts $1])
	AC_COMPILE_IFELSE(AC_LANG_PROGRAM(),
			  [AC_MSG_RESULT([yes]); AC_SUBST([$2], ["$1"])],
			  [AC_MSG_RESULT([no])]
			 )
	CFLAGS="$my_cflags_save"
	AC_LANG_POP([C])
}])

AX_PROG_CC_MPI(,,AC_MSG_FAILURE([Could not find MPI compiler.]))

AC_CHECK_CC_FLAG([-Wall], [WARN_ALL])
AC_CHECK_CC_FLAG([-Wextra], [WARN_EXTRA])

AC_SUBST(WARN_ALL)
AC_SUBST(WARN_EXTRA)

AC_CONFIG_FILES([src/Makevars])

AC_OUTPUT

