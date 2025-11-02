#ifndef FORGE_CONF_H_INCLUDED
#define FORGE_CONF_H_INCLUDED

/**
 * This is the main configuration file for forge.
 * Who needs dotfiles anyways? :)
 */

/**
 * If you are editing this file, you should use
 *     forge editconf
 * as it provides a hint as to how you can apply
 * your changes. Just simply editing this file
 * will not work as one would expect!
 */

#ifdef __cplusplus
extern "C" {
#endif

// How many j flags to pass to make.
// Feel free to put a number here (as a string!)
// if you don't want to use nproc for whatever reason.
#define FORGE_PREFERRED_MAKEFILE_JFLAGS "$(nproc)"

// Where to install to.
// Note: Do not remove $DESTDIR as it points to the
//       fakeroot that all packages install to. Only
//       remove it *if you know what you are doing!*
#define FORGE_PREFERRED_INSTALL_PREFIX "$DESTDIR/usr/local"

// Where to install libraries to.
// Note: Do not remove $DESTDIR as it points to the
//       fakeroot that all packages install to. Only
//       remove it *if you know what you are doing!*
#define FORGE_PREFERRED_LIB_PREFIX "$DESTDIR/usr/local/lib64"

// Tells forge which editor to use for editing files.
#define FORGE_EDITOR "vim"

#ifdef __cplusplus
}
#endif

#endif // FORGE_CONF_H_INCLUDED
