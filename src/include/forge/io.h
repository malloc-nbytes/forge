#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

int forge_io_filepath_exists(const char *fp);
char *forge_io_read_file_to_cstr(const char *fp);
char **forge_io_read_file_to_lines(const char *fp);

#endif // IO_H_INCLUDED
