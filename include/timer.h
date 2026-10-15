#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"

#define TIMER_HZ 100 /* 100 ticks per second (10ms tick interval) */

extern volatile uint64_t system_ticks;

void timer_init(void);
void timer_handler(void);
uint64_t timer_get_ticks(void);
void timer_sleep_ms(uint32_t ms);

#endif /* _TIMER_H */
