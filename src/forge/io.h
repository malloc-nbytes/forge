#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

/**
 * @param fp the filepath
 * @return 1 if exists, 0 on not exists
 * @desc Checks if the filepath `fp` exists
 * or not.
 */
int forge_io_filepath_exists(const char *fp);
void forge_io_create_file(const char *fp, int force_overwrite);
char *forge_io_read_file_to_cstr(const char *fp);
char **forge_io_read_file_to_lines(const char *fp);
char *forge_io_resolve_absolute_path(const char *fp);
int forge_io_write_file(const char *fp, const char *content);

#endif // IO_H_INCLUDED
