#ifndef FORGE_CONF_H_INCLUDED
#define FORGE_CONF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// How many j flags to pass to make
#define FORGE_PREFERRED_MAKEFILE_JFLAGS "$(nproc)"

// Where to install to
#define FORGE_PREFERRED_INSTALL_PREFIX "/usr/local"

// Where to install libraries to
#define FORGE_PREFERRED_LIB_PREFIX "/usr/local/lib64"

#define FORGE_EDITOR "vim"

#ifdef __cplusplus
}
#endif

#endif // FORGE_CONF_H_INCLUDED
