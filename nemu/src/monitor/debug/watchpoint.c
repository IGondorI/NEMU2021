#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "cpu/reg.h"

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

bool add_watchpoint(char *expression) {
	bool success;
	uint32_t value;
	WP *wp;

	if(expression == NULL || strlen(expression) >= sizeof(wp_pool[0].expression)) {
		printf("Missing expression or expression too long (maximum 255 bytes)\n");
		return false;
	}
	value = expr(expression, &success);
	if(!success) {
		printf("Bad watchpoint expression\n");
		return false;
	}
	wp = new_wp();
	if(wp == NULL) {
		printf("Watchpoint pool is full\n");
		return false;
	}
	strcpy(wp->expression, expression);
	wp->value = value;
	printf("Watchpoint %d: %s = %u (0x%08x)\n",
			wp->NO, wp->expression, value, value);
	return true;
}

bool delete_watchpoint(int number) {
	WP *wp;
	for(wp = head; wp != NULL; wp = wp->next) {
		if(wp->NO == number) {
			free_wp(wp);
			printf("Deleted watchpoint %d\n", number);
			return true;
		}
	}
	printf("No watchpoint numbered %d\n", number);
	return false;
}

void print_watchpoints(void) {
	WP *wp;
	if(head == NULL) {
		printf("No watchpoints\n");
		return;
	}
	printf("NO  Value       Expression\n");
	for(wp = head; wp != NULL; wp = wp->next) {
		printf("%-3d 0x%08x  %s\n", wp->NO, wp->value, wp->expression);
	}
}

bool check_watchpoints(void) {
	WP *wp;
	bool stop = false;
	for(wp = head; wp != NULL; wp = wp->next) {
		bool success;
		uint32_t value = expr(wp->expression, &success);
		if(!success) {
			printf("Watchpoint %d: cannot evaluate %s; execution stopped\n",
					wp->NO, wp->expression);
			stop = true;
			continue;
		}
		if(value != wp->value) {
			printf("Hint watchpoint %d at address 0x%08x\n", wp->NO, cpu.eip);
			printf("Old value = %u (0x%08x)\n", wp->value, wp->value);
			printf("New value = %u (0x%08x)\n", value, value);
			wp->value = value;
			stop = true;
		}
	}
	return stop;
}
