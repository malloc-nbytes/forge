# Add VPATH to find headers in src/include
VPATH = $(top_srcdir)/src/include

# Ensure SQLite is built before other targets
all-local: sqlite-build

# Define the library to be built
lib_LTLIBRARIES = libforge.la

sqlite-build:
	$(MAKE) -C ./sqlite-autoconf-3500100

# Sources for libforge.so
libforge_la_SOURCES = utils.c colors.c depgraph.c flags.c forge-main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c \
	forge-headers-src/forge-smap.c forge-headers-src/forge-viewer.c \
	forge-headers-src/forge-colors.c forge-headers-src/forge-mem.c \
	forge-headers-src/forge-ctrl.c forge-headers-src/forge-rdln.c

# Flags for libforge.so
libforge_la_CFLAGS = $(AM_CFLAGS) -fPIC
libforge_la_LDFLAGS = -L$(top_srcdir)/src/sqlite-autoconf-3500100 -lsqlite3 -pthread -ldl

# Define the program to be built
bin_PROGRAMS = forge_production

# Sources for forge executable
forge_production_SOURCES = utils.c colors.c depgraph.c flags.c forge-main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c \
	forge-headers-src/forge-smap.c forge-headers-src/forge-viewer.c \
	forge-headers-src/forge-colors.c forge-headers-src/forge-mem.c \
	forge-headers-src/forge-ctrl.c forge-headers-src/forge-rdln.c

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
	forge/str.h \
	forge/smap.h \
	forge/conf.h \
	forge/colors.h \
	forge/viewer.h \
	forge/mem.h \
	forge/ctrl.h \
	forge/rdln.h

# Custom uninstall hook to remove additional directories
uninstall-hook:
	rm -rf /usr/src/forge /usr/lib/forge /var/lib/forge /var/cache/forge
	rm -rf $(DESTDIR)$(bindir)/forge $(DESTDIR)$(libdir)/libforge.*
	rm -rf $(DESTDIR)$(includedir)/forge
