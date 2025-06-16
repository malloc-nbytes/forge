#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#define err_wargs(msg, ...)                                             \
        do {                                                            \
                fprintf(stderr, "error: " msg "\n", __VA_ARGS__);       \
                exit(1);                                                \
        } while (0)

#define err(msg)                                \
        do {                                    \
                fprintf(stderr, msg "\n");      \
                exit(1);                        \
        } while (0)

#endif // UTILS_H_INCLUDED
