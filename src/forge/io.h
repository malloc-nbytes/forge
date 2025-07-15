#ifndef FORGE_IO_H_INCLUDED
#define FORGE_IO_H_INCLUDED

/**
 * Parameter: fp -> the filepath
 * Returns: 1 if exists, 0 if not exists
 * Description: Checks if the filepath `fp` exists.
 */
int forge_io_filepath_exists(const char *fp);

/**
 * Parameter: fp              -> the filepath
 * Parameter: force_overwrite -> should we overwrite the file
 *                               even if it exists?
 * Description: Create a file at `fp`. If `force_overwrite` is
 *              set to 1, it will truncate the file if it exists.
 */
void forge_io_create_file(const char *fp, int force_overwrite);

/**
 * Parameter: fp -> the filepath
 * Returns: the bytes of the file
 * Description: Read all bytes at the filepath `fp`.
 */
char *forge_io_read_file_to_cstr(const char *fp);

/**
 * Parameter: fp -> the filepath
 * Returns: lines of the file, end of the array is guaranteed
 *          to be NULL.
 * Description: Read all lines at the filepath `fp`.
 */
char **forge_io_read_file_to_lines(const char *fp);

/**
 * Parameter: fp -> the filepath
 * Returns: the absolute filepath of `fp`
 * Description: Get the absolute path of `fp`.
 */
char *forge_io_resolve_absolute_path(const char *fp);

/**
 * Parameter: fp      -> the filepath
 * Parameter: content -> the bytes to write to `fp`
 * Returns: 1 on success, 0 on failure
 * Description: Write `content` to `fp`.
 */
int forge_io_write_file(const char *fp, const char *content);

/**
 * Parameter: fp      -> the filepath
 * Parameter: lines   -> the lines to write
 * Parameter: lines_n -> how many lines to write
 * Returns: 1 on success, 0 on failure
 * Description: Write `lines_n` lines from `lines` to the filepath `fp`.
 */
int forge_io_write_lines(
        const char *fp,
        const char **lines,
        size_t lines_n
);

/**
 * Parameter: path -> the filepath to check
 * Returns: 1 if it is a directory, and 0 if not
 * Description: Check if `path` is a directory.
 */
int forge_io_is_dir(const char *path);

/**
 * Parameter: dir      -> the directory to search
 * Parameter: filename -> the file to search for
 * Returns: 1 if found, and 0 if otherwise
 * Description: Checks to see if `dir` contains `filename`.
 */
int forge_io_dir_contains_file(const char *dir, const char *filename);

/**
 * Parameter: path -> the path to extract the basename from
 * Returns: the basename
 * Description: Given some path i.e., `/home/user/dev/main.c`,
 *              will extract the basename `main.c` from the path.
 */
const char *forge_io_basename(const char *path);

/**
 * Parameter: path -> the directory to remove
 * Return: 1 on success, and 0 on failure.
 * Description: Remove the directory `path`.
 */
int forge_io_rm_dir(const char *path);

#endif // FORGE_IO_H_INCLUDED
