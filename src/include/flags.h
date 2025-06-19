#ifndef FLAGS_H_INCLUDED
#define FLAGS_H_INCLUDED

#define FLAG_1HY_HELP 'h'
#define FLAG_1HY_REBUILD 'r'

#define FLAG_2HY_HELP "help"
#define FLAG_2HY_LIST "list"
#define FLAG_2HY_DEPS "deps"
#define FLAG_2HY_INSTALL "install"
#define FLAG_2HY_UNINSTALL "uninstall"
#define FLAG_2HY_REBUILD "rebuild"
#define FLAG_2HY_NEW "new"
#define FLAG_2HY_EDIT "edit"

typedef enum { FT_NONE = 1 << 0, } flag_type;

void usage(void);

#endif // FLAGS_H_INCLUDED
