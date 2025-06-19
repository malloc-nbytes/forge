# Add VPATH to find headers in src/include
VPATH = $(top_srcdir)/src/include

# Define the library to be built
lib_LTLIBRARIES = libforge.la

# Sources for libforge.so
libforge_la_SOURCES = colors.c depgraph.c flags.c forge-main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c

# Flags for libforge.so
libforge_la_CFLAGS = $(AM_CFLAGS) -fPIC
libforge_la_LDFLAGS = -L$(top_srcdir)/src/sqlite-autoconf-3500100 -lsqlite3 -pthread -ldl

# Define the program to be built
bin_PROGRAMS = forge_production

# Sources for forge executable
forge_production_SOURCES = colors.c depgraph.c flags.c forge-main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c

# Flags for forge executable
forge_production_CFLAGS = $(AM_CFLAGS)
forge_production_LDFLAGS = -L$(top_srcdir)/src/sqlite-autoconf-3500100 -lsqlite3 -pthread -ldl

# Rename forge-production to forge during installation
install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(bindir)
	mv $(DESTDIR)$(bindir)/forge_production $(DESTDIR)$(bindir)/forge

# Specify installation directories
libforge_la_LIBADD =

# Install header files in src/include/forge to <prefix>/include/forge
nobase_include_HEADERS = forge/array.h \
	forge/cmd.h \
	forge/forge.h \
	forge/io.h \
	forge/pkg.h \
	forge/str.h

# Custom uninstall hook to remove additional directories
uninstall-hook:
	rm -rf /usr/src/forge /usr/lib/forge /var/lib/forge /var/cache/forge
	rm -rf $(DESTDIR)$(bindir)/forge $(DESTDIR)$(libdir)/libforge.*
	rm -rf $(DESTDIR)$(includedir)/forge
