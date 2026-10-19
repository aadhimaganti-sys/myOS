#ifndef _LANG_H
#define _LANG_H

#include "types.h"
#include "stddef.h"

#define LANG_MAX_VARS       64
#define LANG_VAR_NAME_LEN   24
#define LANG_MAX_STR_LEN    96
#define LANG_LINE_BUF_LEN   256

typedef enum {
    VAL_NONE,
    VAL_INT,
    VAL_STR
} val_type_t;

struct lang_value {
    val_type_t type;
    int64_t num;
    char str[LANG_MAX_STR_LEN];
};

struct lang_var {
    char name[LANG_VAR_NAME_LEN];
    struct lang_value val;
    int is_set;
};

/* Core Language Engine API */
void lang_init(void);
int  lang_eval_line(const char *line);
int  lang_run_file(const char *filename);
void lang_repl(void);

/* Helper queries */
struct lang_var *lang_get_vars(int *count);
void lang_reset_vars(void);

#endif /* _LANG_H */
