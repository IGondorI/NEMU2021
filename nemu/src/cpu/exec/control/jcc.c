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

static int jcc_condition(uint8_t condition) {
	switch(condition) {
		case 0x0: return cpu.eflags.OF;
		case 0x1: return !cpu.eflags.OF;
		case 0x2: return cpu.eflags.CF;
		case 0x3: return !cpu.eflags.CF;
		case 0x4: return cpu.eflags.ZF;
		case 0x5: return !cpu.eflags.ZF;
		case 0x6: return cpu.eflags.CF || cpu.eflags.ZF;
		case 0x7: return !cpu.eflags.CF && !cpu.eflags.ZF;
		case 0x8: return cpu.eflags.SF;
		case 0x9: return !cpu.eflags.SF;
		case 0xa: return cpu.eflags.PF;
		case 0xb: return !cpu.eflags.PF;
		case 0xc: return cpu.eflags.SF != cpu.eflags.OF;
		case 0xd: return cpu.eflags.SF == cpu.eflags.OF;
		case 0xe: return cpu.eflags.ZF || cpu.eflags.SF != cpu.eflags.OF;
		case 0xf: return !cpu.eflags.ZF && cpu.eflags.SF == cpu.eflags.OF;
		default: assert(0);
	}
	return 0;
}

make_helper(jcc_b) {
	int8_t displacement = instr_fetch(eip + 1, 1);
	int total_len = eip - cpu.eip + 2;
	swaddr_t target = cpu.eip + total_len + displacement;

	if(jcc_condition(ops_decoded.opcode & 0xf)) {
		cpu.eip = target - total_len;
	}
	print_asm("jcc 0x%x", target);
	return 2;
}

make_helper(jcc_v) {
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
	if(jcc_condition(ops_decoded.opcode & 0xf)) {
		cpu.eip = target - total_len;
	}
	print_asm("jcc 0x%x", target);
	return len;
}

make_helper(setcc_rm_b) {
	int len = decode_rm_b(eip + 1);

	write_operand_b(op_src, jcc_condition(ops_decoded.opcode & 0xf));
	print_asm("setcc %s", op_src->str);
	return len + 1;
}
