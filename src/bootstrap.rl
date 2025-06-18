#!/usr/local/bin/earl

module Bootstrap

set_flag("-x");

$"earl build.rl -- clean";
$"earl build.rl";
$"sudo ./forge";
$"earl build.rl -- install";
$"sudo ./forge -r";
