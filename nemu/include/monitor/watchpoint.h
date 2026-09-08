#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	char expression[256];
	uint32_t value;
} WP;

void init_wp_pool(void);
WP *new_wp(void);
bool free_wp(WP *wp);
bool add_watchpoint(char *expression);
bool delete_watchpoint(int number);
void print_watchpoints(void);
bool check_watchpoints(void);

#endif
