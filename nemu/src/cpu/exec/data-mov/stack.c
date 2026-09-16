#include "cpu/exec/helper.h"

make_helper(push_r_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int index = ops_decoded.opcode & 0x7;
	uint32_t value = width == 2 ? reg_w(index) : reg_l(index);

	cpu.esp -= width;
	swaddr_write(cpu.esp, width, value);
	print_asm("push%s %%%s", width == 2 ? "w" : "l",
			width == 2 ? regsw[index] : regsl[index]);
	return 1;
}

make_helper(pop_r_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int index = ops_decoded.opcode & 0x7;
	uint32_t value = swaddr_read(cpu.esp, width);

	cpu.esp += width;
	if(width == 2) {
		reg_w(index) = value;
	}
	else {
		reg_l(index) = value;
	}
	print_asm("pop%s %%%s", width == 2 ? "w" : "l",
			width == 2 ? regsw[index] : regsl[index]);
	return 1;
}

make_helper(ret_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int total_len = eip - cpu.eip + 1;
	swaddr_t target = swaddr_read(cpu.esp, width);

	cpu.esp += width;
	if(width == 2) {
		target = (uint16_t)target;
	}
	cpu.eip = target - total_len;
	print_asm("ret");
	return 1;
}

make_helper(push_i_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	uint32_t value = instr_fetch(eip + 1, width);

	cpu.esp -= width;
	swaddr_write(cpu.esp, width, value);
	print_asm("push $0x%x", value);
	return width + 1;
}

make_helper(push_si_b) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	uint32_t value = (int8_t)instr_fetch(eip + 1, 1);

	cpu.esp -= width;
	swaddr_write(cpu.esp, width, value);
	print_asm("push $0x%x", value);
	return 2;
}

make_helper(push_rm_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int len = width == 2 ? decode_rm_w(eip + 1) : decode_rm_l(eip + 1);
	uint32_t value = op_src->val;

	cpu.esp -= width;
	swaddr_write(cpu.esp, width, value);
	print_asm("push %s", op_src->str);
	return len + 1;
}

make_helper(ret_i_w) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	uint16_t stack_adjust = instr_fetch(eip + 1, 2);
	int total_len = eip - cpu.eip + 3;
	swaddr_t target = swaddr_read(cpu.esp, width);

	cpu.esp += width + stack_adjust;
	if(width == 2) {
		target = (uint16_t)target;
	}
	cpu.eip = target - total_len;
	print_asm("ret $0x%x", stack_adjust);
	return 3;
}
