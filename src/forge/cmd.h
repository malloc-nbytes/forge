#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED

/**
 * Parameter: fp -> the filepath to cd into
 * Returns: 1 on success, 0 on failure
 * Description: cd into the filepath `fp`.
 */
int cd(const char *fp);

/**
 * Parameter: fp -> the filepath to cd into
 * Returns: 1 on success, 0 on failure
 * Description: the same as cd(), but silent.
 */
int cd_silent(const char *fp);

/**
 * Parameter: cmd -> the command to execute
 * Returns: 1 on success, 0 on failure
 * Description: Issue a BASH command.
 */
int cmd(const char *cmd);

/**
 * Parameter: cmd      -> the command to execute
 * Parameter: username -> the user to execute the command as
 * Returns: 1 on success, 0 on failure
 * Description: Issue a BASH command as a specific user.
 *              This is useful if the program is being ran
 *              through `sudo` and you need to create some
 *              files not in /root/. It might be useful to
 *              call get_prev_user() to get the user that
 *              ran forge through sudo.
 */
int cmd_as(const char *cmd, const char *username);

/**
 * Parameter: cmd -> the command to execute
 * Returns: the output of the command, or NULL on failure
 * Description: Issue a BASH command and capture the output.
 *              If the command fails or something goes wrong,
 *              return return result will be NULL.
 */
char *cmdout(const char *cmd);

/**
 * Parameter: author -> the author of the program
 * Parameter: name   -> the name of the program
 * Returns: the name of the program
 * Description: Do a `git clone https://www.github.com/<author>/<name>.git`.
 *              This function returns the name of the command as it is
 *              convenient for the download() function in the C modules.
 */
char *git_clone(char *author, char *name);

/**
 * Parameter: fp -> the filepath to create
 * Returns: the filepath, or NULL on failure
 * Description: Create a directory with the `-p` flag.
 */
char *mkdirp(char *fp);

/**
 * Parameter: var -> the environment variable
 * Returns: the value of the environment variable, or NULL on failure
 * Description: Get the value of an environment variable. Do not
 *              include the dollarsign ($) in the variable name.
 */
char *env(const char *var);

/**
 * Returns: the username of the previous user
 * Description: Get the username of the previous user
 *              calling forge. For example, if forge was
 *              ran as: `USER@/bin/sh# sudo forge install ...`, then the
 *              result will be USER.
 */
char *get_prev_user(void);

/**
 * Parameter: path -> the path to the file to change
 * Parameter: user -> the user to give ownership to
 * Returns: 1 on success, or 0 on failure
 * Description: Change the ownership of `path` to `user`.
 */
int change_file_owner(const char *path, const char *user);

#endif // CMD_H_INCLUDED
