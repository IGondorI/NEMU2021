#include "cpu/exec/helper.h"

make_helper(call_rel_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int len = width + 1;
	int total_len = eip - cpu.eip + len;
	int32_t displacement;
	swaddr_t return_address = cpu.eip + total_len;
	swaddr_t target;

	if(width == 2) {
		displacement = (int16_t)instr_fetch(eip + 1, 2);
		target = (uint16_t)(return_address + displacement);
	}
	else {
		displacement = (int32_t)instr_fetch(eip + 1, 4);
		target = return_address + displacement;
	}

	cpu.esp -= width;
	swaddr_write(cpu.esp, width, return_address);
	cpu.eip = target - total_len;

	print_asm("call 0x%x", target);
	return len;
}

make_helper(call_rm_v) {
	int width = ops_decoded.is_operand_size_16 ? 2 : 4;
	int len = width == 2 ? decode_rm_w(eip + 1) : decode_rm_l(eip + 1);
	int total_len = eip - cpu.eip + len + 1;
	swaddr_t return_address = cpu.eip + total_len;
	swaddr_t target = op_src->val;

	if(width == 2) {
		target = (uint16_t)target;
	}
	cpu.esp -= width;
	swaddr_write(cpu.esp, width, return_address);
	cpu.eip = target - total_len;

	print_asm("call *%s", op_src->str);
	return len + 1;
}
