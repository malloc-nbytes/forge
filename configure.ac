AC_INIT([forge], [1.0], [zdhdev@yahoo.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([src/include/config.h])

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

AC_DEFINE_UNQUOTED([COMPILER_NAME], ["$CC"], [Name of the C compiler])
AC_DEFINE_UNQUOTED([COMPILER_VERSION], ["`$CC --version | head -n1`"], [Version of the C compiler])
AC_DEFINE_UNQUOTED([COMPILER_PATH], ["`which $CC`"], [Full path to the C compiler])

# Add include directory
AC_SUBST([AM_CFLAGS], ["-I./include -I./forge -I./sqlite-autoconf-3500100 -march=native -pedantic -Werror -Wextra -Wall -Wno-maybe-uninitialized"])

AC_DEFINE_UNQUOTED([COMPILER_FLAGS], ["$AM_CFLAGS"], [Compiler flags used])

# Define output files
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
