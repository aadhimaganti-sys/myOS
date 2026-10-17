#ifndef _DRIVERS_H
#define _DRIVERS_H

#include "types.h"
#include "stdint.h"
#include "stddef.h"

enum driver_status {
    DRIVER_OFFLINE = 0,
    DRIVER_ONLINE,
    DRIVER_ACTIVE,
    DRIVER_STANDBY
};

struct driver_info {
    int id;
    const char *tag;          /* e.g. "UART", "GIC", "TMR", "MMU", "VBUS", "KBD", "MOU", "BLK", "GPU", "SCH", "RFS" */
    const char *name;         /* Full human-readable name */
    const char *category;     /* Hardware category */
    uint64_t base_addr;       /* MMIO physical address or 0 */
    uint32_t irq;             /* Interrupt line or 0 */
    const char *version;      /* Hardware/driver revision */
    enum driver_status status;
    const char *status_str;   /* "ONLINE", "ACTIVE", "CAPTURING", etc. */
    const char *description;  /* Summary of driver architecture */
    void (*get_stats)(char *buf, size_t max_len);
    int (*self_test)(char *detail_buf, size_t max_len);
    char last_test_result[128];
    int test_status; /* 0 = not tested, 1 = passed, -1 = failed */
};

void drivers_init(void);
int drivers_get_count(void);
struct driver_info *drivers_get_by_index(int idx);
struct driver_info *drivers_get_by_id(int id);
struct driver_info *drivers_get_by_tag(const char *tag);
void drivers_dump_cli(void);
int drivers_self_test(int idx);
int drivers_test_by_tag(const char *tag);
int drivers_test_all(void);

#endif /* _DRIVERS_H */
