#!/usr/local/bin/earl

module Build

import "std/system.rl"; as sys
import "std/script.rl"; as scr
import "std/colors.rl"; as clr

set_flag("-xe");

let debug = false;
try { debug = ("g", "d", "ggdb", "debug").contains(argv()[1]); }

fn log_ok(msg)   { println(clr::Tfc.Green, msg, clr::Te.Reset); }
fn log_info(msg) { println(clr::Tfc.Yellow, msg, clr::Te.Reset); }
fn log_bad(msg)  { println(clr::Tfc.Red, msg, clr::Te.Reset); }

fn compile_pkgs() {
    log_info("Compiling pkgs");

    $"pwd" |> let oldcwd;
    cd("pkgs");

    let cfiles = sys::get_all_files_by_ext(".", "c");

    foreach f in cfiles {
        with parts = sys::name_and_ext(f),
             name = parts[0].unwrap(),
             ext = parts[1]
        in if ext && ext.unwrap() == "c" {
            $f"earl build.rl -- {name}";
        }
    }

    cd(oldcwd);

    log_ok("ok");
}

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

@const let sqlite3 = "sqlite-autoconf-3500100";
@const let flags = "-Iinclude" + case debug of { true = " -ggdb -O0"; _ = ""; };
@const let name = "-o forge";
@const let ld = f"-L{sqlite3} -lsqlite3 -pthread -ldl";
get_sqlite3();
compile_pkgs();
$f"cc {flags} {name} *.c {ld}";
