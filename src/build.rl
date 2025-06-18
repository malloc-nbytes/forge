#!/usr/local/bin/earl

module Build

import "std/system.rl"; as sys
import "std/script.rl"; as scr
import "std/colors.rl"; as clr

set_flag("-x");

let debug, clean, install = (false, false, false);
try { debug = ("g", "d", "ggdb", "debug").contains(argv()[1]); }
try { clean = argv()[1] == "clean"; }
try { install = argv()[1] == "install"; }

fn log_ok(msg)   { println(clr::Tfc.Green, msg, clr::Te.Reset); }
fn log_info(msg) { println(clr::Tfc.Yellow, msg, clr::Te.Reset); }
fn log_bad(msg)  { println(clr::Tfc.Red, msg, clr::Te.Reset); }

@world fn get_sqlite3() {
    log_info(f"Checking for {sqlite3}");

    if sys::ls(".").contains(f"./{sqlite3}") {
        log_ok("ok");
        return;
    }
    log_info("Installing sqlite3 amalgamation");
    log_info("Checking for wget...");
    if !scr::program_exists("wget") {
        log_bad("wget was not found");
        exit(1);
    }

    $f"wget https://www.sqlite.org/2025/{sqlite3}.tar.gz";
    $f"tar -vxzf ./{sqlite3}.tar.gz";

    $"pwd" |> let oldcwd;
    cd(sqlite3);
    ```
    ./configure
    make -j12
    ```;
    cd(oldcwd);
    $f"rm {sqlite3}.tar.gz";
    log_ok("ok");
}

if clean {
    $"sudo rm /usr/local/lib/libforge.so";
    $"sudo rm -r /usr/include/forge";
    $"sudo rm -r /usr/src/forge/";
    $"sudo rm -r /usr/lib/forge/";
    $"sudo rm -r /var/lib/forge";
    $"sudo rm -r /var/cache/forge";
    exit(0);
}

@const let sqlite3 = "sqlite-autoconf-3500100";
@const let flags = "-Iinclude $(pkg-config --cflags ncurses)" + case debug of { true = " -ggdb -O0"; _ = ""; };
@const let name = "-o forge";
@const let lib_name = "-o libforge.so";
@const let lib_flags = "-fPIC -shared";
@const let ld = f"-L{sqlite3} -lsqlite3 -pthread -ldl $(pkg-config --libs ncurses)";

get_sqlite3();

# Build shared library
$f"cc {flags} {lib_flags} {lib_name} *.c forge-headers-src/*.c {ld}";

# Build executable
$f"cc {flags} {name} *.c forge-headers-src/*.c {ld}";

if install {
    $"sudo mkdir -p /usr/local/include/forge/";
    $"sudo cp -r include/forge /usr/include/forge/";
    $"sudo cp pkgs/*.c /usr/src/forge/modules";
}

$"sudo cp ./libforge.so /usr/local/lib/";
