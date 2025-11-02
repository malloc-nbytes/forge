#ifndef MSGS_H_INCLUDED
#define MSGS_H_INCLUDED

void info(int newline, const char *msg);
void bad(int newline, const char *msg);
void good(int newline, const char *msg);
void info_builder(int newline, const char *first, ...);

#endif // MSGS_H_INCLUDED
