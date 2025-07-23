# Forge

Forge your system.

## Disclaimer
THIS PROJECT IS A WIP. USE AT YOUR OWN RISK!

## License
`Forge` is licensed under the GNU General Public License v2 or later. See [COPYING](COPYING) for details.
Forge also uses sqlite3. For more information, see https://sqlite.org/.

## About
Forge is a package manager where you "forge" your own packages in the form of C source files.
Every package that you want to install must have an associated C file that contains rules
and functions on how to build, install, uninstall, etc. To help with this, a suite of functions
and data structures are supplied as the forge API.

Along with `forge`, it will also compile and install a tool called `fviewer`. It works similarly to `less`.

## Compiling
Forge requires sqlite3. You must either download and untar it yourself, or just run `bootstrap.sh`.
_Note_: `bootstrap.sh` generates `configure` with `--prefix=/usr` and `--libdir=/usr/lib64`.

Using `bootstrap.sh`:
```
./boostrap.sh
make
```

Without:
```
autoreconf --install
./configure --prefix=<prefix> --libdir=<dir>
make
```

Where `<prefix>` is your preferred installation prefix for the binary i.e., `/usr`, `/usr/local`.
Where `<libdir>` is your preferred installation location for the forge library i.e., `/usr/lib64`, `/usr/local/lib`.

To install, run `sudo make install`. To uninstall, run `sudo make uninstall`.

Forge installs files to these locations:
- `<prefix>/forge` (binary)
- `<prefix>/include/forge` (API Headers)
- `<dir>/libforge.so` (forge library)
- `/var/lib/forge/forge.db` (forge database)
- `/var/cache/forge/sources/` (package sources)
- `/usr/src/forge/modules/` (C package modules)
- `/usr/lib/forge/modules` (compiled C package modules)
- `<prefix>/bin/fviewer` (the fviewer tool binary)

## Getting Started
The first time you run `forge` must be as root. This sets up the database. All subsequent calls do not need it
(unless the action has an R tag, see `--help` for more information).

### Getting the official repository
I have created a repository of a bunch of my projects and a few others that you can begin using. You can get
this repo by doing:
```
forge add-repo https://github.com/malloc-nbytes/forge-modules.git
```

### Creating your own packages
To begin creating a new package, run `sudo forge new <pkg>`. This will open an editor to start definining the
package rules and behavior. When you are finished, save and quit.

When new packages are added, they must be built with `sudo forge --rebuild`. This will compile them and show
any errors if needed.

If there are errors, you can run `sudo forge edit <pkg>` to start editing it again.

When all of your packages have been compiled, run `forge list` to see all available. You can then run the following to install them:
`sudo forge install <pkg1> <pkg2>, ..., <pkgN>`. If you want to remove them, run `sudo forge uninstall <pkg1> <pkg2>, ..., <pkgN>`.
If you want to update, do `sudo forge update <pkg1> <pkg2>, ..., <pkgN>` or have no arguments to update all of them.
