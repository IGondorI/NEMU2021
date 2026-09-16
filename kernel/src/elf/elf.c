#include "common.h"
#include "memory.h"
#include <string.h>
#include <elf.h>

#define ELF_OFFSET_IN_DISK 0

#ifdef HAS_DEVICE
void ide_read(uint8_t *, uint32_t, uint32_t);
#else
void ramdisk_read(uint8_t *, uint32_t, uint32_t);
#endif

#define STACK_SIZE (1 << 20)

void create_video_mapping();
uint32_t get_ucr3();

static void read_from_disk(void *buf, uint32_t offset, uint32_t len) {
#ifdef HAS_DEVICE
	ide_read(buf, offset, len);
#else
	ramdisk_read(buf, offset, len);
#endif
}

uint32_t loader() {
	Elf32_Ehdr elf;
	uint32_t i;

	read_from_disk(&elf, ELF_OFFSET_IN_DISK, sizeof(elf));

	/* An ELF file starts with the four bytes 0x7f, 'E', 'L', 'F'. */
	nemu_assert(memcmp(elf.e_ident, ELFMAG, SELFMAG) == 0);
	nemu_assert(elf.e_ident[EI_CLASS] == ELFCLASS32);
	nemu_assert(elf.e_ident[EI_DATA] == ELFDATA2LSB);
	nemu_assert(elf.e_machine == EM_386);
	nemu_assert(elf.e_phentsize == sizeof(Elf32_Phdr));

	/* Scan the program header table and load every loadable segment. */
	for(i = 0; i < elf.e_phnum; i ++) {
		Elf32_Phdr ph;
		uint8_t *dest;
		uint32_t ph_offset = elf.e_phoff + i * elf.e_phentsize;

		read_from_disk(&ph, ELF_OFFSET_IN_DISK + ph_offset, sizeof(ph));
		if(ph.p_type != PT_LOAD) {
			continue;
		}

		nemu_assert(ph.p_filesz <= ph.p_memsz);

#ifdef IA32_PAGE
		/* mm_malloc() establishes the user virtual mapping and returns the
		 * corresponding physical address used by the kernel while loading. */
		dest = (void *)mm_malloc(ph.p_vaddr, ph.p_memsz);
#else
		dest = (void *)ph.p_vaddr;
#endif

		/* Copy the bytes present in the ELF file, then initialize the extra
		 * in-memory area (normally .bss) to zero. */
		read_from_disk(dest, ELF_OFFSET_IN_DISK + ph.p_offset, ph.p_filesz);
		memset(dest + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);

#ifdef IA32_PAGE
		/* Record the program break for future use. */
		extern uint32_t cur_brk, max_brk;
		uint32_t new_brk = ph.p_vaddr + ph.p_memsz - 1;
		if(cur_brk < new_brk) { max_brk = cur_brk = new_brk; }
#endif
	}

	volatile uint32_t entry = elf.e_entry;

#ifdef IA32_PAGE
	mm_malloc(KOFFSET - STACK_SIZE, STACK_SIZE);

#ifdef HAS_DEVICE
	create_video_mapping();
#endif

	write_cr3(get_ucr3());
#endif

	return entry;
}
