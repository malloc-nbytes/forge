#ifndef FLAGS_H_INCLUDED
#define FLAGS_H_INCLUDED

#define FLAG_1HY_HELP 'h'

#define FLAG_2HY_HELP "help"
#define FLAG_2HY_LIST "list"
#define FLAG_2HY_DEPS "deps"

typedef enum { FT_NONE = 0, } flag_type;

void usage(void);

#endif // FLAGS_H_INCLUDED
