#ifndef _SHELL_H
#define _SHELL_H

#include "stddef.h"

#define SHELL_MAX_LINE 256
#define SHELL_MAX_ARGS 16
#define SHELL_MAX_HISTORY 64

void shell_init(void);
void shell_run(void);
void shell_execute(char *line);

/* Command History Subsystem */
void shell_history_add(const char *cmd);
int  shell_history_count(void);
const char *shell_history_get(int index);
const char *shell_history_find_prefix(const char *prefix);
void shell_history_clear(void);
void shell_history_dump(int limit);
int  shell_expand_history(char *line, size_t max_len);

#endif /* _SHELL_H */
