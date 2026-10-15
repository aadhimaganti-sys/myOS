#ifndef _SHELL_H
#define _SHELL_H

#define SHELL_MAX_LINE 256
#define SHELL_MAX_ARGS 16

void shell_init(void);
void shell_run(void);
void shell_execute(char *line);

#endif /* _SHELL_H */
