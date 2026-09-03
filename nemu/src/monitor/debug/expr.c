#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>

enum {
	NOTYPE = 256,  //空格
	HEX,           //十六进制数
	DECIMAL,       //十进制数
	REG,           //寄存器

	// 逻辑运算符
	EQ,            //等于
	NEQ,           //不等于
	AND,           //与
	OR,            //或

	// 根据上下文识别的一元运算符
	NEG,           //负号
	DEREF,         //内存解引用

	// 单字符无需定义，因为它们可以直接使用 ASCII 编码
};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	{"[ \t]+",                         NOTYPE},  //空白字符
	{"0[xX][0-9a-fA-F]+",             HEX},     //十六进制数
	{"[0-9]+",                         DECIMAL}, //十进制数
	{"\\$[a-zA-Z][a-zA-Z0-9]*",      REG},     //寄存器

	// 多字符运算符放在单字符运算符前面
	{"==",                             EQ},
	{"!=",                             NEQ},
	{"&&",                             AND},
	{"\\|\\|",                       OR},

	{"\\+",                           '+'},
	{"-",                              '-'},
	{"\\*",                           '*'},
	{"/",                              '/'},
	{"\\(",                           '('},
	{"\\)",                           ')'},
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

#define NR_TOKEN 32

static Token tokens[NR_TOKEN];
static int nr_token;

/* 判断一个 token 能否作为普通操作数的结尾。这个信息用于区分
 * 乘法和解引用、减法和负号。 */
static bool is_operand_end(int type) {
	return type == HEX || type == DECIMAL || type == REG || type == ')';
}

/* `*' 和 `-' 既可以是二元运算符，也可以是一元运算符。
 * 如果它们位于表达式开头，或前一个 token 不是操作数，则将其
 * 分别解释为解引用和负号。 */
static void identify_unary(void) {
	int i;

	for(i = 0; i < nr_token; i ++) {
		if((tokens[i].type == '*' || tokens[i].type == '-') &&
				(i == 0 || !is_operand_end(tokens[i - 1].type))) {
			tokens[i].type = (tokens[i].type == '*' ? DEREF : NEG);
		}
	}
}

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				if(rules[i].token_type != NOTYPE) {
					if(nr_token >= NR_TOKEN) {
						printf("expression contains too many tokens\n");
						return false;
					}
					if(substr_len >= (int)sizeof(tokens[nr_token].str)) {
						printf("token is too long at position %d\n",
								position - substr_len);
						return false;
					}

					tokens[nr_token].type = rules[i].token_type;
					memcpy(tokens[nr_token].str, substr_start, substr_len);
					tokens[nr_token].str[substr_len] = '\0';
					nr_token ++;
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	identify_unary();
	return true;
}

/* 检查整条表达式的括号是否匹配。 */
static bool validate_parentheses(void) {
	int i;
	int depth = 0;

	for(i = 0; i < nr_token; i ++) {
		if(tokens[i].type == '(') {
			depth ++;
		}
		else if(tokens[i].type == ')') {
			depth --;
			if(depth < 0) {
				return false;
			}
		}
	}

	return depth == 0;
}

/* 判断 [p, q] 是否被一对括号完整包围。 */
static bool enclosed_by_parentheses(int p, int q) {
	int i;
	int depth = 0;

	if(tokens[p].type != '(' || tokens[q].type != ')') {
		return false;
	}

	for(i = p; i <= q; i ++) {
		if(tokens[i].type == '(') {
			depth ++;
		}
		else if(tokens[i].type == ')') {
			depth --;
		}

		/* 外层左括号在 q 之前就已经闭合，说明它没有包住整个区间。 */
		if(depth == 0 && i < q) {
			return false;
		}
	}

	return depth == 0;
}

/* 数值越小，运算符优先级越低。 */
static int precedence(int type) {
	switch(type) {
		case OR: return 1;
		case AND: return 2;
		case EQ:
		case NEQ: return 3;
		case '+':
		case '-': return 4;
		case '*':
		case '/': return 5;
		default: return -1;
	}
}

/* 找到不在括号内、优先级最低的主运算符。相同优先级时选择
 * 最右边的运算符，从而保持 +、-、*、/ 的左结合性。 */
static int find_main_operator(int p, int q) {
	int i;
	int depth = 0;
	int main_op = -1;
	int main_precedence = 100;

	for(i = p; i <= q; i ++) {
		int current_precedence;

		if(tokens[i].type == '(') {
			depth ++;
			continue;
		}
		if(tokens[i].type == ')') {
			depth --;
			continue;
		}
		if(depth != 0) {
			continue;
		}

		current_precedence = precedence(tokens[i].type);
		if(current_precedence >= 0 && current_precedence <= main_precedence) {
			main_precedence = current_precedence;
			main_op = i;
		}
	}

	return main_op;
}

static uint32_t read_register(const char *name, bool *success) {
	int i;

	if(strcmp(name, "eip") == 0) {
		return cpu.eip;
	}
	if(strcmp(name, "eflags") == 0) {
		return cpu.eflags.val;
	}

	for(i = 0; i < 8; i ++) {
		if(strcmp(name, regsl[i]) == 0) {
			return reg_l(i);
		}
		if(strcmp(name, regsw[i]) == 0) {
			return reg_w(i);
		}
		if(strcmp(name, regsb[i]) == 0) {
			return reg_b(i);
		}
	}

	printf("unknown register '$%s'\n", name);
	*success = false;
	return 0;
}

static uint32_t parse_number(const Token *token, int base, bool *success) {
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(token->str, &end, base);
	if(errno == ERANGE || *end != '\0' || value > 0xffffffffUL) {
		printf("invalid number '%s'\n", token->str);
		*success = false;
		return 0;
	}

	return (uint32_t)value;
}

static uint32_t eval(int p, int q, bool *success) {
	int op;
	uint32_t val1, val2;

	if(!*success) {
		return 0;
	}
	if(p > q) {
		printf("missing operand\n");
		*success = false;
		return 0;
	}

	if(p == q) {
		switch(tokens[p].type) {
			case DECIMAL:
				return parse_number(&tokens[p], 10, success);
			case HEX:
				return parse_number(&tokens[p], 16, success);
			case REG:
				return read_register(tokens[p].str + 1, success);
			default:
				printf("token '%s' is not an operand\n", tokens[p].str);
				*success = false;
				return 0;
		}
	}

	if(enclosed_by_parentheses(p, q)) {
		return eval(p + 1, q - 1, success);
	}

	op = find_main_operator(p, q);
	if(op >= 0) {
		val1 = eval(p, op - 1, success);
		if(!*success) {
			return 0;
		}

		/* && 和 || 保持与 C 相同的短路求值行为。 */
		if(tokens[op].type == AND && val1 == 0) {
			return 0;
		}
		if(tokens[op].type == OR && val1 != 0) {
			return 1;
		}

		val2 = eval(op + 1, q, success);
		if(!*success) {
			return 0;
		}

		switch(tokens[op].type) {
			case '+': return val1 + val2;
			case '-': return val1 - val2;
			case '*': return val1 * val2;
			case '/':
				if(val2 == 0) {
					printf("division by zero\n");
					*success = false;
					return 0;
				}
				return val1 / val2;
			case EQ: return val1 == val2;
			case NEQ: return val1 != val2;
			case AND: return val1 && val2;
			case OR: return val1 || val2;
			default: assert(0);
		}
	}

	/* 没有二元主运算符时，只允许前缀一元运算符。 */
	if(tokens[p].type == NEG) {
		return -eval(p + 1, q, success);
	}
	if(tokens[p].type == DEREF) {
		uint32_t addr = eval(p + 1, q, success);
		if(!*success) {
			return 0;
		}
		return swaddr_read(addr, 4);
	}

	printf("invalid expression near '%s'\n", tokens[p].str);
	*success = false;
	return 0;
}

uint32_t expr(char *e, bool *success) {
	bool success_storage;

	/* 允许调用者不关心 success，但内部始终使用一个有效指针。 */
	if(success == NULL) {
		success = &success_storage;
	}
	*success = false;

	if(e == NULL || !make_token(e) || nr_token == 0) {
		return 0;
	}
	if(!validate_parentheses()) {
		printf("parentheses do not match\n");
		return 0;
	}

	*success = true;
	return eval(0, nr_token - 1, success);
}
