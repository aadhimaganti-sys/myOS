#include "lang.h"
#include "string.h"
#include "printf.h"
#include "timer.h"
#include "mm.h"
#include "ramfs.h"
#include "sched.h"
#include "coin.h"
#include "keyboard.h"
#include "uart.h"
#include "gui.h"

static struct lang_var var_table[LANG_MAX_VARS];
static int var_count = 0;

void lang_reset_vars(void) {
    memset(var_table, 0, sizeof(var_table));
    var_count = 0;
}

struct lang_var *lang_get_vars(int *count) {
    if (count) *count = var_count;
    return var_table;
}

static struct lang_var *find_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (var_table[i].is_set && strcmp(var_table[i].name, name) == 0) {
            return &var_table[i];
        }
    }
    return NULL;
}

static void set_var_num(const char *name, int64_t num) {
    struct lang_var *v = find_var(name);
    if (!v) {
        if (var_count >= LANG_MAX_VARS) {
            kprintf("[KitScript:ERR] Variable table full!\n");
            return;
        }
        v = &var_table[var_count++];
        strncpy(v->name, name, LANG_VAR_NAME_LEN - 1);
        v->name[LANG_VAR_NAME_LEN - 1] = '\0';
        v->is_set = 1;
    }
    v->val.type = VAL_INT;
    v->val.num = num;
    v->val.str[0] = '\0';
}

static void set_var_str(const char *name, const char *str) {
    struct lang_var *v = find_var(name);
    if (!v) {
        if (var_count >= LANG_MAX_VARS) {
            kprintf("[KitScript:ERR] Variable table full!\n");
            return;
        }
        v = &var_table[var_count++];
        strncpy(v->name, name, LANG_VAR_NAME_LEN - 1);
        v->name[LANG_VAR_NAME_LEN - 1] = '\0';
        v->is_set = 1;
    }
    v->val.type = VAL_STR;
    v->val.num = 0;
    strncpy(v->val.str, str ? str : "", LANG_MAX_STR_LEN - 1);
    v->val.str[LANG_MAX_STR_LEN - 1] = '\0';
}

/* -------------------------------------------------------------------------
 * Tokenizer & Expression Parser
 * ------------------------------------------------------------------------- */
typedef enum {
    TOK_EOF,
    TOK_NUM,
    TOK_STR,
    TOK_IDENT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EQ,       /* = */
    TOK_EQEQ,     /* == */
    TOK_NEQ,      /* != */
    TOK_LT,       /* < */
    TOK_LTE,      /* <= */
    TOK_GT,       /* > */
    TOK_GTE,      /* >= */
    TOK_COMMA,
    TOK_SEMI
} tok_type_t;

struct token {
    tok_type_t type;
    int64_t num;
    char text[LANG_MAX_STR_LEN];
};

static const char *src_ptr = NULL;
static struct token cur_tok;

static void next_tok(void) {
    while (*src_ptr == ' ' || *src_ptr == '\t' || *src_ptr == '\r' || *src_ptr == '\n') {
        src_ptr++;
    }

    if (*src_ptr == '\0' || *src_ptr == '#') { /* # is comment */
        cur_tok.type = TOK_EOF;
        cur_tok.text[0] = '\0';
        return;
    }

    char c = *src_ptr;

    /* Numbers */
    if (c >= '0' && c <= '9') {
        int64_t val = 0;
        int idx = 0;
        while (*src_ptr >= '0' && *src_ptr <= '9') {
            val = val * 10 + (*src_ptr - '0');
            if (idx < LANG_MAX_STR_LEN - 1) cur_tok.text[idx++] = *src_ptr;
            src_ptr++;
        }
        cur_tok.text[idx] = '\0';
        cur_tok.type = TOK_NUM;
        cur_tok.num = val;
        return;
    }

    /* String literals */
    if (c == '"' || c == '\'') {
        char quote = c;
        src_ptr++;
        int idx = 0;
        while (*src_ptr != '\0' && *src_ptr != quote) {
            if (*src_ptr == '\\' && *(src_ptr + 1) != '\0') {
                src_ptr++;
                if (*src_ptr == 'n') cur_tok.text[idx++] = '\n';
                else if (*src_ptr == 't') cur_tok.text[idx++] = '\t';
                else cur_tok.text[idx++] = *src_ptr;
            } else {
                if (idx < LANG_MAX_STR_LEN - 1) cur_tok.text[idx++] = *src_ptr;
            }
            src_ptr++;
        }
        if (*src_ptr == quote) src_ptr++;
        cur_tok.text[idx] = '\0';
        cur_tok.type = TOK_STR;
        cur_tok.num = 0;
        return;
    }

    /* Identifiers and keywords */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        int idx = 0;
        while ((*src_ptr >= 'a' && *src_ptr <= 'z') ||
               (*src_ptr >= 'A' && *src_ptr <= 'Z') ||
               (*src_ptr >= '0' && *src_ptr <= '9') ||
               *src_ptr == '_') {
            if (idx < LANG_MAX_STR_LEN - 1) cur_tok.text[idx++] = *src_ptr;
            src_ptr++;
        }
        cur_tok.text[idx] = '\0';
        cur_tok.type = TOK_IDENT;
        cur_tok.num = 0;
        return;
    }

    /* Multi-char operators */
    if (c == '=' && *(src_ptr + 1) == '=') {
        src_ptr += 2;
        cur_tok.type = TOK_EQEQ;
        strcpy(cur_tok.text, "==");
        return;
    }
    if (c == '!' && *(src_ptr + 1) == '=') {
        src_ptr += 2;
        cur_tok.type = TOK_NEQ;
        strcpy(cur_tok.text, "!=");
        return;
    }
    if (c == '<' && *(src_ptr + 1) == '=') {
        src_ptr += 2;
        cur_tok.type = TOK_LTE;
        strcpy(cur_tok.text, "<=");
        return;
    }
    if (c == '>' && *(src_ptr + 1) == '=') {
        src_ptr += 2;
        cur_tok.type = TOK_GTE;
        strcpy(cur_tok.text, ">=");
        return;
    }

    /* Single char operators */
    src_ptr++;
    cur_tok.text[0] = c;
    cur_tok.text[1] = '\0';

    switch (c) {
        case '+': cur_tok.type = TOK_PLUS; break;
        case '-': cur_tok.type = TOK_MINUS; break;
        case '*': cur_tok.type = TOK_STAR; break;
        case '/': cur_tok.type = TOK_SLASH; break;
        case '%': cur_tok.type = TOK_PERCENT; break;
        case '(': cur_tok.type = TOK_LPAREN; break;
        case ')': cur_tok.type = TOK_RPAREN; break;
        case '=': cur_tok.type = TOK_EQ; break;
        case '<': cur_tok.type = TOK_LT; break;
        case '>': cur_tok.type = TOK_GT; break;
        case ',': cur_tok.type = TOK_COMMA; break;
        case ';': cur_tok.type = TOK_SEMI; break;
        default:  cur_tok.type = TOK_EOF; break;
    }
}

/* Forward declaration */
static struct lang_value parse_expr(void);

static struct lang_value parse_primary(void) {
    struct lang_value v;
    v.type = VAL_NONE;
    v.num = 0;
    v.str[0] = '\0';

    if (cur_tok.type == TOK_NUM) {
        v.type = VAL_INT;
        v.num = cur_tok.num;
        next_tok();
        return v;
    }

    if (cur_tok.type == TOK_STR) {
        v.type = VAL_STR;
        strncpy(v.str, cur_tok.text, LANG_MAX_STR_LEN - 1);
        v.str[LANG_MAX_STR_LEN - 1] = '\0';
        next_tok();
        return v;
    }

    if (cur_tok.type == TOK_LPAREN) {
        next_tok();
        v = parse_expr();
        if (cur_tok.type == TOK_RPAREN) {
            next_tok();
        }
        return v;
    }

    if (cur_tok.type == TOK_IDENT) {
        char name[LANG_VAR_NAME_LEN];
        strncpy(name, cur_tok.text, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        next_tok();

        /* Check if function call e.g. ticks(), mem(), peek(addr), rand(max) */
        if (cur_tok.type == TOK_LPAREN) {
            next_tok();
            struct lang_value arg1;
            arg1.type = VAL_NONE;
            arg1.num = 0;
            if (cur_tok.type != TOK_RPAREN) {
                arg1 = parse_expr();
            }
            if (cur_tok.type == TOK_RPAREN) next_tok();

            if (strcmp(name, "ticks") == 0) {
                v.type = VAL_INT;
                v.num = (int64_t)timer_get_ticks();
            } else if (strcmp(name, "mem") == 0) {
                v.type = VAL_INT;
                v.num = (int64_t)(mm_get_free_pages() * 4); /* KB */
            } else if (strcmp(name, "balance") == 0) {
                struct coin_wallet *w = coin_get_my_wallet();
                v.type = VAL_INT;
                v.num = w ? (int64_t)w->balance : 0;
            } else if (strcmp(name, "peek") == 0) {
                v.type = VAL_INT;
                uintptr_t addr = (uintptr_t)arg1.num;
                v.num = *(volatile uint32_t *)addr;
            } else if (strcmp(name, "rand") == 0) {
                v.type = VAL_INT;
                uint64_t t = timer_get_ticks();
                int64_t max = (arg1.num > 0) ? arg1.num : 100;
                v.num = (int64_t)((t * 1103515245ULL + 12345ULL) % (uint64_t)max);
            } else if (strcmp(name, "len") == 0) {
                v.type = VAL_INT;
                v.num = (arg1.type == VAL_STR) ? strlen(arg1.str) : 0;
            } else if (strcmp(name, "cls") == 0) {
                uart_puts("\033[2J\033[H");
                gui_terminal_clear();
                v.type = VAL_INT;
                v.num = 0;
            } else {
                kprintf("[KitScript:WARN] Unknown function '%s()'\n", name);
            }
            return v;
        }

        /* Variable lookup */
        struct lang_var *var = find_var(name);
        if (var) {
            return var->val;
        } else {
            /* Undefined var defaults to 0 */
            v.type = VAL_INT;
            v.num = 0;
            return v;
        }
    }

    if (cur_tok.type == TOK_MINUS) {
        next_tok();
        struct lang_value inner = parse_primary();
        if (inner.type == VAL_INT) {
            inner.num = -inner.num;
        }
        return inner;
    }

    return v;
}

static struct lang_value parse_mul(void) {
    struct lang_value left = parse_primary();
    while (cur_tok.type == TOK_STAR || cur_tok.type == TOK_SLASH || cur_tok.type == TOK_PERCENT) {
        tok_type_t op = cur_tok.type;
        next_tok();
        struct lang_value right = parse_primary();
        if (left.type == VAL_INT && right.type == VAL_INT) {
            if (op == TOK_STAR) left.num *= right.num;
            else if (op == TOK_SLASH) left.num = (right.num != 0) ? (left.num / right.num) : 0;
            else if (op == TOK_PERCENT) left.num = (right.num != 0) ? (left.num % right.num) : 0;
        }
    }
    return left;
}

static struct lang_value parse_add(void) {
    struct lang_value left = parse_mul();
    while (cur_tok.type == TOK_PLUS || cur_tok.type == TOK_MINUS) {
        tok_type_t op = cur_tok.type;
        next_tok();
        struct lang_value right = parse_mul();
        if (op == TOK_PLUS) {
            if (left.type == VAL_STR || right.type == VAL_STR) {
                char s1[LANG_MAX_STR_LEN], s2[LANG_MAX_STR_LEN];
                if (left.type == VAL_INT) snprintf(s1, sizeof(s1), "%ld", (long)left.num);
                else strncpy(s1, left.str, sizeof(s1) - 1);
                if (right.type == VAL_INT) snprintf(s2, sizeof(s2), "%ld", (long)right.num);
                else strncpy(s2, right.str, sizeof(s2) - 1);

                left.type = VAL_STR;
                snprintf(left.str, sizeof(left.str), "%s%s", s1, s2);
            } else {
                left.num += right.num;
            }
        } else {
            if (left.type == VAL_INT && right.type == VAL_INT) {
                left.num -= right.num;
            }
        }
    }
    return left;
}

static struct lang_value parse_rel(void) {
    struct lang_value left = parse_add();
    while (cur_tok.type == TOK_LT || cur_tok.type == TOK_LTE ||
           cur_tok.type == TOK_GT || cur_tok.type == TOK_GTE) {
        tok_type_t op = cur_tok.type;
        next_tok();
        struct lang_value right = parse_add();
        int res = 0;
        if (left.type == VAL_INT && right.type == VAL_INT) {
            if (op == TOK_LT) res = (left.num < right.num);
            else if (op == TOK_LTE) res = (left.num <= right.num);
            else if (op == TOK_GT) res = (left.num > right.num);
            else if (op == TOK_GTE) res = (left.num >= right.num);
        }
        left.type = VAL_INT;
        left.num = res;
    }
    return left;
}

static struct lang_value parse_expr(void) {
    struct lang_value left = parse_rel();
    while (cur_tok.type == TOK_EQEQ || cur_tok.type == TOK_NEQ) {
        tok_type_t op = cur_tok.type;
        next_tok();
        struct lang_value right = parse_rel();
        int res = 0;
        if (left.type == VAL_INT && right.type == VAL_INT) {
            res = (op == TOK_EQEQ) ? (left.num == right.num) : (left.num != right.num);
        } else if (left.type == VAL_STR && right.type == VAL_STR) {
            int eq = (strcmp(left.str, right.str) == 0);
            res = (op == TOK_EQEQ) ? eq : !eq;
        }
        left.type = VAL_INT;
        left.num = res;
    }
    return left;
}

/* -------------------------------------------------------------------------
 * Statement Execution Engine
 * ------------------------------------------------------------------------- */
static int exec_statement(void);

static void print_val(struct lang_value v) {
    if (v.type == VAL_INT) {
        kprintf("%ld", (long)v.num);
    } else if (v.type == VAL_STR) {
        kprintf("%s", v.str);
    }
}

static int exec_print(int newline) {
    next_tok();
    int count = 0;
    while (cur_tok.type != TOK_EOF && cur_tok.type != TOK_SEMI &&
           strcmp(cur_tok.text, "done") != 0 && strcmp(cur_tok.text, "else") != 0 &&
           strcmp(cur_tok.text, "endif") != 0) {
        if (count > 0 && cur_tok.type == TOK_COMMA) {
            next_tok();
            kprintf(" ");
        }
        struct lang_value v = parse_expr();
        print_val(v);
        count++;
        if (cur_tok.type != TOK_COMMA) break;
    }
    if (newline) {
        kprintf("\n");
    }
    return 0;
}

static int exec_for(void) {
    next_tok(); /* consume 'for' */
    if (cur_tok.type != TOK_IDENT) {
        kprintf("[KitScript:ERR] Expected loop variable name!\n");
        return -1;
    }
    char var_name[LANG_VAR_NAME_LEN];
    strncpy(var_name, cur_tok.text, sizeof(var_name) - 1);
    var_name[sizeof(var_name) - 1] = '\0';
    next_tok();

    if (cur_tok.type != TOK_EQ) {
        kprintf("[KitScript:ERR] Expected '=' after for variable!\n");
        return -1;
    }
    next_tok();

    struct lang_value start_val = parse_expr();
    if (strcmp(cur_tok.text, "to") != 0) {
        kprintf("[KitScript:ERR] Expected 'to' in for loop!\n");
        return -1;
    }
    next_tok();

    struct lang_value end_val = parse_expr();
    int64_t step = 1;
    if (strcmp(cur_tok.text, "step") == 0) {
        next_tok();
        struct lang_value step_val = parse_expr();
        step = (step_val.num != 0) ? step_val.num : 1;
    }

    if (strcmp(cur_tok.text, "do") != 0) {
        kprintf("[KitScript:ERR] Expected 'do' in for loop!\n");
        return -1;
    }
    next_tok();

    /* Save loop body pointer */
    const char *body_start = src_ptr;

    /* Execute loop iterations */
    int64_t current = start_val.num;
    while (1) {
        if (step > 0 && current > end_val.num) break;
        if (step < 0 && current < end_val.num) break;

        set_var_num(var_name, current);
        src_ptr = body_start;
        next_tok();

        while (cur_tok.type != TOK_EOF && strcmp(cur_tok.text, "done") != 0) {
            exec_statement();
            if (cur_tok.type == TOK_SEMI) next_tok();
        }
        current += step;
    }

    /* Advance parser past 'done' */
    src_ptr = body_start;
    next_tok();
    int nest = 1;
    while (cur_tok.type != TOK_EOF) {
        if (strcmp(cur_tok.text, "for") == 0 || strcmp(cur_tok.text, "while") == 0) {
            nest++;
        } else if (strcmp(cur_tok.text, "done") == 0) {
            nest--;
            if (nest == 0) {
                next_tok();
                break;
            }
        }
        next_tok();
    }

    return 0;
}

static int exec_while(void) {
    next_tok(); /* consume 'while' */
    const char *cond_start = src_ptr;
    struct lang_value cond = parse_expr();

    if (strcmp(cur_tok.text, "do") != 0) {
        kprintf("[KitScript:ERR] Expected 'do' in while loop!\n");
        return -1;
    }
    next_tok();
    const char *body_start = src_ptr;

    while (cond.num != 0) {
        src_ptr = body_start;
        next_tok();
        while (cur_tok.type != TOK_EOF && strcmp(cur_tok.text, "done") != 0) {
            exec_statement();
            if (cur_tok.type == TOK_SEMI) next_tok();
        }

        /* Re-evaluate condition */
        src_ptr = cond_start;
        next_tok();
        cond = parse_expr();
    }

    /* Fast forward to 'done' */
    src_ptr = body_start;
    next_tok();
    int nest = 1;
    while (cur_tok.type != TOK_EOF) {
        if (strcmp(cur_tok.text, "while") == 0 || strcmp(cur_tok.text, "for") == 0) nest++;
        else if (strcmp(cur_tok.text, "done") == 0) {
            nest--;
            if (nest == 0) {
                next_tok();
                break;
            }
        }
        next_tok();
    }

    return 0;
}

static int exec_if(void) {
    next_tok(); /* consume 'if' */
    struct lang_value cond = parse_expr();

    if (strcmp(cur_tok.text, "then") != 0) {
        kprintf("[KitScript:ERR] Expected 'then' after if condition!\n");
        return -1;
    }
    next_tok();

    if (cond.num != 0) {
        /* Condition TRUE: execute then-branch */
        while (cur_tok.type != TOK_EOF &&
               strcmp(cur_tok.text, "else") != 0 &&
               strcmp(cur_tok.text, "endif") != 0) {
            exec_statement();
            if (cur_tok.type == TOK_SEMI) next_tok();
        }
        /* Skip optional else-branch */
        if (strcmp(cur_tok.text, "else") == 0) {
            next_tok();
            int nest = 1;
            while (cur_tok.type != TOK_EOF) {
                if (strcmp(cur_tok.text, "if") == 0) nest++;
                else if (strcmp(cur_tok.text, "endif") == 0) {
                    nest--;
                    if (nest == 0) {
                        next_tok();
                        break;
                    }
                }
                next_tok();
            }
        } else if (strcmp(cur_tok.text, "endif") == 0) {
            next_tok();
        }
    } else {
        /* Condition FALSE: skip then-branch */
        int nest = 1;
        while (cur_tok.type != TOK_EOF) {
            if (strcmp(cur_tok.text, "if") == 0) nest++;
            else if (strcmp(cur_tok.text, "else") == 0 && nest == 1) {
                next_tok();
                /* Execute else-branch */
                while (cur_tok.type != TOK_EOF && strcmp(cur_tok.text, "endif") != 0) {
                    exec_statement();
                    if (cur_tok.type == TOK_SEMI) next_tok();
                }
                if (strcmp(cur_tok.text, "endif") == 0) next_tok();
                return 0;
            } else if (strcmp(cur_tok.text, "endif") == 0) {
                nest--;
                if (nest == 0) {
                    next_tok();
                    break;
                }
            }
            next_tok();
        }
    }
    return 0;
}

static int exec_statement(void) {
    if (cur_tok.type == TOK_EOF || cur_tok.type == TOK_SEMI) {
        return 0;
    }

    if (cur_tok.type == TOK_IDENT) {
        if (strcmp(cur_tok.text, "print") == 0) {
            return exec_print(0);
        } else if (strcmp(cur_tok.text, "println") == 0) {
            return exec_print(1);
        } else if (strcmp(cur_tok.text, "for") == 0) {
            return exec_for();
        } else if (strcmp(cur_tok.text, "while") == 0) {
            return exec_while();
        } else if (strcmp(cur_tok.text, "if") == 0) {
            return exec_if();
        } else if (strcmp(cur_tok.text, "let") == 0) {
            next_tok();
            if (cur_tok.type != TOK_IDENT) {
                kprintf("[KitScript:ERR] Expected variable name after let!\n");
                return -1;
            }
            char vname[LANG_VAR_NAME_LEN];
            strncpy(vname, cur_tok.text, sizeof(vname) - 1);
            vname[sizeof(vname) - 1] = '\0';
            next_tok();

            if (cur_tok.type != TOK_EQ) {
                kprintf("[KitScript:ERR] Expected '=' in assignment!\n");
                return -1;
            }
            next_tok();
            struct lang_value val = parse_expr();
            if (val.type == VAL_STR) set_var_str(vname, val.str);
            else set_var_num(vname, val.num);
            return 0;
        } else if (strcmp(cur_tok.text, "vars") == 0) {
            kprintf("\n--- KitScript Variable Table ---\n");
            for (int i = 0; i < var_count; i++) {
                if (!var_table[i].is_set) continue;
                if (var_table[i].val.type == VAL_STR) {
                    kprintf("  %-16s = \"%s\"\n", var_table[i].name, var_table[i].val.str);
                } else {
                    kprintf("  %-16s = %ld\n", var_table[i].name, (long)var_table[i].val.num);
                }
            }
            kprintf(" Total: %d active variables\n\n", var_count);
            next_tok();
            return 0;
        } else if (strcmp(cur_tok.text, "help") == 0) {
            kprintf("\nKitScript Language Reference & Cheat Sheet:\n");
            kprintf("  let x = 10                  Define variable\n");
            kprintf("  print expr, expr            Print values without newline\n");
            kprintf("  println expr                Print expression with newline\n");
            kprintf("  for i = 1 to 5 do ... done  For loop with counter\n");
            kprintf("  while x > 0 do ... done     While loop\n");
            kprintf("  if cond then ... else ... endif Conditional branching\n");
            kprintf("  ticks(), mem(), balance()   Builtin telemetry functions\n");
            kprintf("  cls()                       Clear screen\n");
            kprintf("  vars                        List all variables\n\n");
            next_tok();
            return 0;
        } else {
            /* Check if assignment to existing var: var = expr */
            char vname[LANG_VAR_NAME_LEN];
            strncpy(vname, cur_tok.text, sizeof(vname) - 1);
            vname[sizeof(vname) - 1] = '\0';
            next_tok();
            if (cur_tok.type == TOK_EQ) {
                next_tok();
                struct lang_value val = parse_expr();
                if (val.type == VAL_STR) set_var_str(vname, val.str);
                else set_var_num(vname, val.num);
                return 0;
            } else {
                /* Standalone expression */
                src_ptr -= strlen(vname);
                next_tok();
                struct lang_value val = parse_expr();
                if (val.type != VAL_NONE) {
                    print_val(val);
                    kprintf("\n");
                }
                return 0;
            }
        }
    }

    /* Fallback: parse as expression */
    struct lang_value val = parse_expr();
    if (val.type != VAL_NONE) {
        print_val(val);
        kprintf("\n");
    }
    return 0;
}

int lang_eval_line(const char *line) {
    if (!line) return 0;
    src_ptr = line;
    next_tok();

    while (cur_tok.type != TOK_EOF) {
        exec_statement();
        if (cur_tok.type == TOK_SEMI) {
            next_tok();
        }
    }
    return 0;
}

int lang_run_file(const char *filename) {
    if (!filename) return -1;
    char path[64];
    if (filename[0] == '/') {
        strncpy(path, filename + 1, sizeof(path) - 1);
    } else {
        strncpy(path, filename, sizeof(path) - 1);
    }
    path[sizeof(path) - 1] = '\0';

    struct ramfs_file *f = ramfs_find(path); int sz = f ? (int)f->size : -1;
    if (sz <= 0) {
        kprintf("[KitScript:ERR] File '%s' not found in RAMFS!\n", filename);
        return -1;
    }

    char *buf = (char *)kmalloc(sz + 2);
    if (!buf) {
        kprintf("[KitScript:ERR] Memory allocation failed reading '%s'!\n", filename);
        return -1;
    }

    int read_bytes = ramfs_read(path, buf, sz);
    if (read_bytes <= 0) {
        kfree(buf);
        return -1;
    }
    buf[read_bytes] = '\0';

    kprintf("[*] Executing KitScript file '%s' (%d bytes)...\n", filename, read_bytes);
    uint64_t t0 = timer_get_ticks();

    /* Process line by line */
    char *p = buf;
    char line[LANG_LINE_BUF_LEN];
    while (*p != '\0') {
        int idx = 0;
        while (*p != '\0' && *p != '\n' && *p != '\r' && idx < LANG_LINE_BUF_LEN - 1) {
            line[idx++] = *p++;
        }
        line[idx] = '\0';
        while (*p == '\n' || *p == '\r') p++;

        /* Trim leading whitespace */
        char *lp = line;
        while (*lp == ' ' || *lp == '\t') lp++;
        if (*lp != '\0' && *lp != '#') {
            lang_eval_line(lp);
        }
    }

    uint64_t elapsed = timer_get_ticks() - t0;
    kprintf("[✔] Completed '%s' in %lu ticks (%lu ms).\n", filename, elapsed, elapsed * 10);
    kfree(buf);
    return 0;
}

void lang_repl(void) {
    kprintf("\n====================================================================\n");
    kprintf("                 KitScript (KS) v1.0 Interactive REPL               \n");
    kprintf("====================================================================\n");
    kprintf("Type expressions or commands (e.g. 'print 2 + 2', 'help', or 'exit')\n\n");

    char line[LANG_LINE_BUF_LEN];
    while (1) {
        kprintf("ks> ");
        int pos = 0;
        memset(line, 0, sizeof(line));

        while (1) {
            struct key_event ev;
            if (keyboard_poll(&ev)) {
                if (!ev.pressed) continue;
                char c = ev.ascii;
                if (c == '\r' || c == '\n') {
                    uart_putc('\n');
                    gui_terminal_putc('\n');
                    line[pos] = '\0';
                    break;
                } else if (c == '\b' || c == 0x7F) {
                    if (pos > 0) {
                        pos--;
                        line[pos] = '\0';
                        uart_puts("\b \b");
                        gui_terminal_putc('\b');
                    }
                } else if (c == 0x03) { /* Ctrl+C */
                    uart_puts("^C\n");
                    gui_terminal_puts("^C\n");
                    line[0] = '\0';
                    break;
                } else if (c >= 32 && c <= 126) {
                    if (pos < LANG_LINE_BUF_LEN - 1) {
                        line[pos++] = c;
                        uart_putc(c);
                        gui_terminal_putc(c);
                    }
                }
            }
            gui_refresh();
            task_yield();
        }

        if (pos > 0) {
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
                kprintf("[*] Exiting KitScript REPL.\n");
                break;
            }
            lang_eval_line(line);
        }
    }
}

void lang_init(void) {
    lang_reset_vars();

    /* Pre-install standard sample scripts into RAMFS */
    const char *demo_src =
        "# KitScript Demonstration Script\n"
        "println \"=== KitScript Demonstration on myOS ===\"\n"
        "let x = 10\n"
        "let y = 32\n"
        "println \"Variables: x = \" + x + \", y = \" + y + \", sum = \" + (x + y)\n"
        "println \"Calculating squares 1..5:\"\n"
        "for i = 1 to 5 do println \"  \" + i + \"^2 = \" + (i * i) done\n"
        "println \"Telemetry: System Uptime = \" + ticks() + \" ticks, Free RAM = \" + mem() + \" KB\"\n"
        "println \"Active Wallet Balance = \" + balance() + \" KIT\"\n"
        "println \"=== Demonstration Complete ===\"\n";

    ramfs_create("demo.ks");
    ramfs_write("demo.ks", demo_src, strlen(demo_src));

    const char *fib_src =
        "# Fibonacci Numbers in KitScript\n"
        "println \"--- First 10 Fibonacci Numbers ---\"\n"
        "let a = 0\n"
        "let b = 1\n"
        "for i = 1 to 10 do println \"F(\" + i + \") = \" + b; let c = a + b; let a = b; let b = c done\n";

    ramfs_create("fib.ks");
    ramfs_write("fib.ks", fib_src, strlen(fib_src));

    const char *bench_src =
        "# CPU Benchmark Script\n"
        "println \"--- Computing 10,000 Iterations Benchmark ---\"\n"
        "let start = ticks()\n"
        "let sum = 0\n"
        "for i = 1 to 10000 do let sum = sum + i done\n"
        "let elapsed = ticks() - start\n"
        "println \"Sum 1..10000 = \" + sum + \" in \" + elapsed + \" ticks\"\n";

    ramfs_create("bench.ks");
    ramfs_write("bench.ks", bench_src, strlen(bench_src));

    kprintf("[+] KitScript (KS) v1.0 programming language initialized (3 demo scripts in RAMFS)\n");
}
