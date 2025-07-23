# Define programs to be built
bin_PROGRAMS = fviewer

# Sources for fviewer executable
fviewer_SOURCES = main.c context.c

# Flags for fviewer executable
fviewer_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/src/fviewer/include -I$(top_srcdir)/src/
fviewer_LDFLAGS = -L$(top_srcdir)/src -lforge
