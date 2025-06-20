#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED

int cd(const char *fp);
int cd_silent(const char *fp);
int cmd(const char *cmd);
char *cmdout(const char *cmd);
char *git_clone(char *author, char *name);
char *mkdirp(char *fp);
char *env(const char *var);
char *get_prev_user(void);
int change_file_owner(const char *path, const char *user);

#endif // CMD_H_INCLUDED
