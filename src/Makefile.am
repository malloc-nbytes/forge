# Add VPATH to find headers in src/include
VPATH = $(top_srcdir)/src/include

# Define the library to be built
lib_LTLIBRARIES = libforge.la

# Sources for libforge.so
libforge_la_SOURCES = utils.c msgs.c depgraph.c flags.c main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c \
	forge-headers-src/forge-smap.c forge-headers-src/forge-viewer.c \
	forge-headers-src/forge-colors.c forge-headers-src/forge-mem.c \
	forge-headers-src/forge-ctrl.c forge-headers-src/forge-rdln.c \
	forge-headers-src/forge-arg.c forge-headers-src/forge-lexer.c \
	forge-headers-src/forge-utils.c forge-headers-src/forge-chooser.c \
	forge-headers-src/forge-cstr.c forge-headers-src/forge-logger.c

# Flags for libforge.so
libforge_la_CFLAGS = $(AM_CFLAGS) -fPIC
libforge_la_LDFLAGS = -lsqlite3 -pthread -ldl

# Define programs to be built
bin_PROGRAMS = forge_production

# Sources for forge executable
forge_production_SOURCES = utils.c msgs.c depgraph.c flags.c main.c \
	forge-headers-src/forge-cmd.c forge-headers-src/forge-io.c \
	forge-headers-src/forge-pkg.c forge-headers-src/forge-str.c \
	forge-headers-src/forge-smap.c forge-headers-src/forge-viewer.c \
	forge-headers-src/forge-colors.c forge-headers-src/forge-mem.c \
	forge-headers-src/forge-ctrl.c forge-headers-src/forge-rdln.c \
	forge-headers-src/forge-arg.c forge-headers-src/forge-lexer.c \
	forge-headers-src/forge-utils.c forge-headers-src/forge-chooser.c \
	forge-headers-src/forge-cstr.c forge-headers-src/forge-logger.c

# Flags for forge executable
forge_production_CFLAGS = $(AM_CFLAGS)
forge_production_LDFLAGS = -lsqlite3 -pthread -ldl

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
	forge/cstr.h \
	forge/smap.h \
	forge/conf.h \
	forge/colors.h \
	forge/viewer.h \
	forge/mem.h \
	forge/ctrl.h \
	forge/rdln.h \
	forge/arg.h \
	forge/lexer.h \
	forge/utils.h \
	forge/chooser.h \
	forge/err.h \
	forge/test.h \
	forge/logger.h \
	forge/set.h

# Custom uninstall hook to remove additional directories
uninstall-hook:
	rm -rf /usr/src/forge /usr/lib/forge /var/lib/forge /var/cache/forge
	rm -rf $(DESTDIR)$(bindir)/forge $(DESTDIR)$(bindir)/fviewer $(DESTDIR)$(libdir)/libforge.*
	rm -rf $(DESTDIR)$(includedir)/forge

# Define the directory for Bash completion scripts
bashcompletiondir = $(sysconfdir)/bash_completion.d

# Specify the completion script to install
bashcompletion_DATA = forge-autocomplete.sh

# Ensure the script is included in the distribution
EXTRA_DIST = forge-autocomplete.sh
