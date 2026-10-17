#ifndef _AGY_H
#define _AGY_H

#include "types.h"
#include "stdint.h"
#include "stddef.h"

#define AGY_MAX_AGENTS   8
#define AGY_NAME_LEN     32
#define AGY_ROLE_LEN     64
#define AGY_GOAL_LEN     128
#define AGY_MSG_LEN      128
#define AGY_MODEL_LEN    32

typedef enum {
    AGENT_UNUSED = 0,
    AGENT_IDLE,
    AGENT_THINKING,
    AGENT_RUNNING,
    AGENT_WAITING,
    AGENT_COMPLETED,
    AGENT_ERROR
} agent_state_t;

struct agy_agent {
    uint32_t      id;
    char          name[AGY_NAME_LEN];
    char          role[AGY_ROLE_LEN];
    char          current_goal[AGY_GOAL_LEN];
    agent_state_t state;
    int32_t       pid;
    uint32_t      steps_total;
    uint32_t      steps_done;
    uint64_t      last_tick;
    char          last_message[AGY_MSG_LEN];
    void        (*worker_fn)(void);
};

struct agy_orchestrator {
    char     active_model[AGY_MODEL_LEN];
    uint64_t total_tokens;
    uint32_t active_agents;
    uint32_t tasks_done;
};

extern struct agy_orchestrator agy_orchestrator;

/* Function prototypes */
void agy_init(void);
int agy_register_agent(const char *name, const char *role, void (*worker_fn)(void));
int agy_prompt(const char *agent_name, const char *goal);
int agy_spawn(const char *name, const char *role);
void agy_set_model(const char *model_name);
const char *agy_get_model(void);
struct agy_agent *agy_get_agents(int *count);
void agy_get_stats(uint32_t *active_agents, uint32_t *tasks_done, uint64_t *tokens);
void agy_step_all(void);

#endif /* _AGY_H */
