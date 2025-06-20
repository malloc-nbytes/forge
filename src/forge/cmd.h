#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED

int cd(const char *fp);
int cd_silent(const char *fp);
int cmd(const char *cmd);
char *cmdout(const char *cmd);
char *git_clone(char *author, char *name);
char *mkdirp(char *fp);

#endif // CMD_H_INCLUDED
