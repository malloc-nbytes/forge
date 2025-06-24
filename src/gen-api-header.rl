module NewAPIHeader

import "std/datatypes/char.rl";

# Note: to run this script, EARL must be installed: https://www.github.com/malloc-nbytes/EARL.git
# This script is used for creating a new Forge API header.

set_flag("-xe");

let name = REPL_input("header name (do not include .h): ");
$f"touch forge/{name}.h";

let forge_header_f = open("forge/forge.h", "rw");
let forge_header_content = forge_header_f.read();
forge_header_f.close();

let new_forge_header_content = [];
with hit_new_include = false
in foreach line in forge_header_content.split("\n") {
    if !hit_new_include && len(line) > 0 && line[0] != '#' {
        new_forge_header_content += [f"#include \"forge/{name}.h\""];
        hit_new_include = true;
    }
    new_forge_header_content += [line];
}

let forge_header_overwrite_f = open("forge/forge.h", "w");
for i in 0 to len(new_forge_header_content) {
    forge_header_overwrite_f.write(new_forge_header_content[i]);
    if i != len(new_forge_header_content)-1 {
        forge_header_overwrite_f.write('\n');
    }
}
forge_header_overwrite_f.close();

let new_header_f = open(f"forge/{name}.h", "w");
let macro = "";
foreach c in name { macro.append(Char::toupper(c)); }
macro += "_H_INCLUDED";
new_header_f.write(f"#ifndef {macro}\n");
new_header_f.write(f"#define {macro}\n");
new_header_f.write("\n");
new_header_f.write(f"#endif // {macro}");
new_header_f.close();
