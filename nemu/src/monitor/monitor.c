#include "nemu.h"
#include <elf.h>

#define ENTRY_START 0x100000
#define ENTRY_PROBE_SIZE 16

extern uint8_t entry [];
extern uint32_t entry_len;
extern char *exec_file;

void load_elf_tables(int, char *[]);
void init_regex();
void init_wp_pool();
void init_ddr3();

FILE *log_fp = NULL;

static void init_log() {
	log_fp = fopen("log.txt", "w");
	Assert(log_fp, "Can not open 'log.txt'");
}

static void welcome() {
	printf("Welcome to NEMU!\nThe executable is %s.\nFor help, type \"help\"\n",
			exec_file);
}

void init_monitor(int argc, char *argv[]) {
	/* Perform some global initialization */

	/* Open the log file. */
	init_log();

	/* Load the string table and symbol table from the ELF file for future use. */
	load_elf_tables(argc, argv);

	/* Compile the regular expressions. */
	init_regex();

	/* Initialize the watchpoint pool. */
	init_wp_pool();

	/* Display welcome message. */
	welcome();
}

#ifdef USE_RAMDISK
static void init_ramdisk() {
	int ret;
	const int ramdisk_max_size = 0xa0000;
	FILE *fp = fopen(exec_file, "rb");
	Assert(fp, "Can not open '%s'", exec_file);

	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	Assert(file_size < ramdisk_max_size, "file size(%zd) too large", file_size);

	fseek(fp, 0, SEEK_SET);
	ret = fread(hwa_to_va(0), file_size, 1, fp);
	assert(ret == 1);
	fclose(fp);
}
#endif

/*
 * The final PA2 layout loads the kernel at ENTRY_START. Earlier-stage tests
 * replace "entry" with a raw user program, so detect that case by comparing
 * bytes at the ELF entry point and honor the address recorded in the ELF.
 */
static bool entry_is_exec_file(FILE *entry_fp, size_t entry_size,
        swaddr_t *load_addr, swaddr_t *entry_point) {
    FILE *elf_fp;
    Elf32_Ehdr elf;
    Elf32_Phdr entry_ph;
    uint32_t min_vaddr = UINT32_MAX;
    bool found_entry = false;
    uint8_t elf_bytes[ENTRY_PROBE_SIZE];
    uint8_t entry_bytes[ENTRY_PROBE_SIZE];
    int i;

    elf_fp = fopen(exec_file, "rb");
    if(elf_fp == NULL ||
            fread(&elf, sizeof(elf), 1, elf_fp) != 1 ||
            memcmp(elf.e_ident, ELFMAG, SELFMAG) != 0 ||
            elf.e_ident[EI_CLASS] != ELFCLASS32 ||
            elf.e_ident[EI_DATA] != ELFDATA2LSB ||
            elf.e_machine != EM_386 ||
            elf.e_phentsize != sizeof(Elf32_Phdr)) {
        if(elf_fp != NULL) {
            fclose(elf_fp);
        }
        return false;
    }

    for(i = 0; i < elf.e_phnum; i ++) {
        Elf32_Phdr ph;

        fseek(elf_fp, elf.e_phoff + i * elf.e_phentsize, SEEK_SET);
        if(fread(&ph, sizeof(ph), 1, elf_fp) != 1) {
            fclose(elf_fp);
            return false;
        }
        if(ph.p_type != PT_LOAD || ph.p_filesz == 0) {
            continue;
        }
        if(ph.p_vaddr < min_vaddr) {
            min_vaddr = ph.p_vaddr;
        }
        if(elf.e_entry >= ph.p_vaddr &&
                elf.e_entry - ph.p_vaddr < ph.p_filesz &&
                ph.p_filesz - (elf.e_entry - ph.p_vaddr) >= ENTRY_PROBE_SIZE) {
            entry_ph = ph;
            found_entry = true;
        }
    }

    if(!found_entry || min_vaddr == UINT32_MAX ||
            elf.e_entry < min_vaddr ||
            elf.e_entry - min_vaddr > entry_size ||
            entry_size - (elf.e_entry - min_vaddr) < ENTRY_PROBE_SIZE) {
        fclose(elf_fp);
        return false;
    }

    fseek(elf_fp, entry_ph.p_offset + elf.e_entry - entry_ph.p_vaddr, SEEK_SET);
    fseek(entry_fp, elf.e_entry - min_vaddr, SEEK_SET);
    if(fread(elf_bytes, sizeof(elf_bytes), 1, elf_fp) != 1 ||
            fread(entry_bytes, sizeof(entry_bytes), 1, entry_fp) != 1) {
        fclose(elf_fp);
        rewind(entry_fp);
        return false;
    }

    fclose(elf_fp);
    rewind(entry_fp);
    if(memcmp(elf_bytes, entry_bytes, sizeof(elf_bytes)) != 0) {
        return false;
    }

    *load_addr = min_vaddr;
    *entry_point = elf.e_entry;
    return true;
}

static swaddr_t load_entry() {
    int ret;
    swaddr_t load_addr = ENTRY_START;
    swaddr_t entry_point = ENTRY_START;
    FILE *fp = fopen("entry", "rb");
    Assert(fp, "Can not open 'entry'");

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);

    rewind(fp);
    entry_is_exec_file(fp, file_size, &load_addr, &entry_point);
    Assert(load_addr < HW_MEM_SIZE && file_size <= HW_MEM_SIZE - load_addr,
            "entry does not fit in physical memory");

    rewind(fp);
    ret = fread(hwa_to_va(load_addr), file_size, 1, fp);
    assert(ret == 1);
    fclose(fp);
    return entry_point;
}

void restart() {
	/* Perform some initialization to restart a program */
#ifdef USE_RAMDISK
	/* Read the file with name `argv[1]' into ramdisk. */
	init_ramdisk();
#endif

    /* Read the entry code into memory. */
    cpu.eip = load_entry();

    /* i386 reserves bit 1 of EFLAGS and keeps it set. */
    cpu.eflags.val = 0x2;

    /* Initialize DRAM. */
    init_ddr3();
}
