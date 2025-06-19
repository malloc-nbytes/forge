AC_INIT([forge], [1.0], [your_email@example.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR
LT_INIT

# Check for required libraries
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found])])
AC_CHECK_LIB([dl], [dlopen], [], [AC_MSG_ERROR([dl library not found])])
AC_CHECK_LIB([sqlite3], [sqlite3_open], [], [AC_MSG_ERROR([sqlite3 library not found])])

# Add include directory
AC_SUBST([AM_CFLAGS], ["-I./include -I./forge"])

# Define output files
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
