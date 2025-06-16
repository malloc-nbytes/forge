#!/usr/local/bin/earl

module Build

import "std/system.rl"; as sys

set_flag("-xe");

fn compile_pkgs() {
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
}

fn get_sqlite3() {
    @const let sqlite3 = "sqlite-autoconf-3500100";
    if sys::ls(".").contains(f"./{sqlite3}") { return; }
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
}

get_sqlite3();
compile_pkgs();
$"cc -O0 -ggdb -o cpm cpm.c sqlite-autoconf-3500100/sqlite3.c -pthread -ldl";
