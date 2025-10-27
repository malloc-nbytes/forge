#ifndef PATHS_H_INCLUDED
#define PATHS_H_INCLUDED

#define DATABASE_DIR           "/var/lib/forge"
#define DATABASE_FP            DATABASE_DIR "/forge.db"

#define C_MODULE_DIR           PREFIX "/src/forge/modules"
#define C_MODULE_USER_DIR      PREFIX "/src/forge/user_modules"
#define C_MODULE_DIR_PARENT    PREFIX "/src/forge"

#define MODULE_LIB_DIR         PREFIX "/lib/forge/modules"
#define PKG_SOURCE_DIR         "/var/cache/forge/sources"
#define FORGE_API_HEADER_DIR   PREFIX "/include/forge"
#define FORGE_CONF_HEADER_FP   FORGE_API_HEADER_DIR "/conf.h"

#endif // PATHS_H_INCLUDED
