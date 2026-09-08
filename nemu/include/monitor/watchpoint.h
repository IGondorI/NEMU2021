#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */


} WP;

void init_wp_pool(void);
WP *new_wp(void);
bool free_wp(WP *wp);

#endif
