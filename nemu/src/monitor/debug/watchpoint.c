#include "monitor/watchpoint.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool(void) {
	int index;
	for(index = 0; index < NR_WP; index ++) {
		wp_pool[index].NO = index;
		wp_pool[index].next = index + 1 < NR_WP ? &wp_pool[index + 1] : NULL;
	}

	head = NULL;
	free_ = wp_pool;
}

WP *new_wp(void) {
	WP *wp;

	if(free_ == NULL) {
		return NULL;
	}

	wp = free_;
	free_ = wp->next;
	wp->next = head;
	head = wp;

	return wp;
}

bool free_wp(WP *wp) {
	WP **link = &head;

	if(wp == NULL) {
		return false;
	}

	while(*link != NULL && *link != wp) {
		link = &(*link)->next;
	}
	if(*link == NULL) {
		return false;
	}

	*link = wp->next;
	wp->next = free_;
	free_ = wp;

	return true;
}
