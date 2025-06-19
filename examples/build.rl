#!/usr/local/bin/earl

module Build

set_flag("-xe");

let name = "";
try     { name = argv()[1]; }
catch _ { panic("A pkg name is required"); }

$"mkdir -p build";
#$f"gcc -L../ -lforge -shared -fPIC {name}.c -o ./build/{name}.so -I../include -Wl,-rpath,../";
$f"gcc -shared -fPIC {name}.c ../forge.c -o ./build/{name}.so -I../include";
