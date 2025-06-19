AC_INIT([forge], [1.0], [your_email@example.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR
LT_INIT

# Build SQLite amalgamation
AC_MSG_NOTICE([Configuring SQLite amalgamation])
AC_CONFIG_COMMANDS([sqlite-config],
  [cd ./src/sqlite-autoconf-3500100 && ./configure && cd ..],
  [test -d ./src/sqlite-autoconf-3500100 || AC_MSG_ERROR([SQLite directory not found])])

# Check for required libraries
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found])])
AC_CHECK_LIB([dl], [dlopen], [], [AC_MSG_ERROR([dl library not found])])
AC_CHECK_LIB([sqlite3], [sqlite3_open], [], [AC_MSG_ERROR([sqlite3 library not found])], [-L./src/sqlite-autoconf-3500100/.libs])

# Add include directory
AC_SUBST([AM_CFLAGS], ["-I./include -I./forge -I./sqlite-autoconf-3500100"])

# Define output files
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
