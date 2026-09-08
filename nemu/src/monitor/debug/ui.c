#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_p(char *args);	//表达式求值

static int cmd_si(char *args) {
    uint32_t n = 1;

    if(args != NULL) {
        char *start = args;
        char *end;
        unsigned long value;

        /* 跳过参数开头的空白字符 */
        while(isspace((unsigned char)*start)) {
            start ++;
        }

        /*
         * si 后面没有有效参数，或者参数是负数。
         * strtoul() 对负数的处理比较特殊，所以这里提前拒绝。
         */
        if(*start == '\0' || *start == '-') {
            printf("Usage: si [N]\n");
            return 0;
        }

        errno = 0;

        /* 按十进制把字符串转换成 unsigned long */
        value = strtoul(start, &end, 10);

        /* 跳过数字后面的空白字符 */
        while(isspace((unsigned char)*end)) {
            end ++;
        }

        /*
         * start == end：一个数字都没有读取到
         * *end != '\0'：数字后面还有非法字符
         * errno == ERANGE：数字太大，发生溢出
         * value == 0：执行 0 条指令没有意义
         * value > UINT32_MAX：超出 cpu_exec() 参数范围
         */
        if(start == end ||
           *end != '\0' ||
           errno == ERANGE ||
           value == 0 ||
           value > UINT32_MAX) {
            printf("Invalid instruction count: %s\n", args);
            printf("Usage: si [N]\n");
            return 0;
        }

        n = (uint32_t)value;
    }

    cpu_exec(n);
    return 0;
}

static int cmd_info(char *args) {
	int i;

	if(args == NULL) {
		printf("Usage: info r|w\n");
		return 0;
	}

	while(isspace((unsigned char)*args)) {
		args ++;
	}

	if(strcmp(args, "w") == 0) {
		print_watchpoints();
		return 0;
	}

	if(strcmp(args, "r") != 0) {
		printf("Unknown info target '%s'\n", args);
		printf("Usage: info r|w\n");
		return 0;
	}

	printf("%-8s %-12s %s\n", "register", "hex", "decimal");
	for(i = 0; i < 8; i ++) {
		printf("%-8s 0x%08x   %u\n", regsl[i], reg_l(i), reg_l(i));
	}
	printf("%-8s 0x%08x   %u\n", "eip", cpu.eip, cpu.eip);
	printf("%-8s 0x%08x   %u\n", "eflags", cpu.eflags.val,
			cpu.eflags.val);

	return 0;
}

static int cmd_x(char *args) {
	char *count_start;
	char *expr_start;
	unsigned long count;
	unsigned long i;
	uint32_t address;
	bool success;

	if(args == NULL) {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	count_start = args;
	while(isspace((unsigned char)*count_start)) {
		count_start ++;
	}

	if(*count_start == '\0' || *count_start == '-') {
		printf("Usage: x N EXPR\n");
		return 0;
	}

	errno = 0;
	count = strtoul(count_start, &expr_start, 10);

	/* N 后面必须先出现空白，随后才是地址表达式。 */
	if(count_start == expr_start ||
	   errno == ERANGE ||
	   count == 0 ||
	   count > UINT32_MAX ||
	   !isspace((unsigned char)*expr_start)) {
		printf("Invalid scan count\n");
		printf("Usage: x N EXPR\n");
		return 0;
	}

	while(isspace((unsigned char)*expr_start)) {
		expr_start ++;
	}
	if(*expr_start == '\0') {
		printf("Missing address expression\n");
		printf("Usage: x N EXPR\n");
		return 0;
	}

	address = expr(expr_start, &success);
	if(!success) {
		printf("Bad address expression\n");
		return 0;
	}

	/* 当前 NEMU 尚未实现地址转换，软件地址会直接映射到物理内存。 */
	if(address > HW_MEM_SIZE - 4 || count > (HW_MEM_SIZE - address) / 4) {
		printf("Memory range is out of bound\n");
		return 0;
	}

	for(i = 0; i < count; i ++) {
		swaddr_t current = address + i * 4;
		uint32_t value = swaddr_read(current, 4);

		printf("0x%08x: 0x%08x\n", current, value);
	}

	return 0;
}

static int cmd_w(char *args) {
	if(args == NULL) {
		printf("Usage: w EXPR\n");
		return 0;
	}
	add_watchpoint(args);
	return 0;
}

static int cmd_d(char *args) {
	char *end;
	unsigned long number;

	if(args == NULL || !isdigit((unsigned char)*args)) {
		printf("Usage: d N (non-negative decimal watchpoint number)\n");
		return 0;
	}
	errno = 0;
	number = strtoul(args, &end, 10);
	if(errno == ERANGE || *end != '\0' || number > INT_MAX) {
		printf("Invalid watchpoint number\n");
		return 0;
	}
	delete_watchpoint((int)number);
	return 0;
}

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },

	/* TODO: Add more commands */
	{ "si", "Step one or N instructions", cmd_si },
	{ "info", "Print registers or watchpoints: info r|w", cmd_info },
	{ "x", "Scan memory: x N EXPR", cmd_x },
	{ "w", "Create a watchpoint: w EXPR", cmd_w },
	{ "d", "Delete a watchpoint: d N", cmd_d },
	{ "p", "Evaluate an expression", cmd_p }

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = args;
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_p(char *args) {
    bool success;
    uint32_t value;

    if(args == NULL) {
        printf("Usage: p EXPR\n");
        return 0;
    }

    value = expr(args, &success);

    if(success) {
        printf("%u (0x%08x)\n", value, value);
    }
    else {
        printf("Bad expression\n");
    }

    return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		if(str == NULL) {
			return;
		}
		char *str_end = str + strlen(str);
		while(str_end > str && isspace((unsigned char)str_end[-1])) {
			*--str_end = 0;
		}

		/* extract the first token as the command */
		char *cmd = strtok(str, " \t");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		while(args < str_end && isspace((unsigned char)*args)) {
			args ++;
		}
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
