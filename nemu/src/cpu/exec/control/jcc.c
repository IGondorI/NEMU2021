#include "cpu/exec/helper.h"

make_helper(je_b) {
	int8_t displacement = instr_fetch(eip + 1, 1);
	int total_len = eip - cpu.eip + 2;
	swaddr_t target = cpu.eip + total_len + displacement;

	if(cpu.eflags.ZF) {
		cpu.eip = target - total_len;
	}
	print_asm("je 0x%x", target);
	return 2;
}

make_helper(je_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int len = width + 1;
	int total_len = eip - cpu.eip + len;
	int32_t displacement;
	swaddr_t target;

	if(width == 2) {
		displacement = (int16_t)instr_fetch(eip + 1, 2);
		target = (uint16_t)(cpu.eip + total_len + displacement);
	}
	else {
		displacement = (int32_t)instr_fetch(eip + 1, 4);
		target = cpu.eip + total_len + displacement;
	}
	if(cpu.eflags.ZF) {
		cpu.eip = target - total_len;
	}
	print_asm("je 0x%x", target);
	return len;
}
