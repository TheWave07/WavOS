#include <gdtdesc.h>
#include <common/tools.h>
extern void gdt_write(unsigned int);

struct gdt_entry_struct
{
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_middle;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

#define GDT_ENTRIES_COUNT 6
gdt_entry_t gdt_entries[GDT_ENTRIES_COUNT];
TSS tss;

void gdt_set_gate(int idx, unsigned int base, unsigned int limit, unsigned char access, unsigned char granularity)
{
	gdt_entries[idx].base_low = (base&0xFFFF);
	gdt_entries[idx].base_middle = (base>>16)&0xFF;
	gdt_entries[idx].base_high = (base>>24)&0xFF;
	gdt_entries[idx].limit_low = (limit&0xFFFF);
	gdt_entries[idx].granularity = (limit>>16)&0x0F;
	gdt_entries[idx].granularity |= granularity&0xF0;
	gdt_entries[idx].access=access;
}

void change_tss_esp0(uint32_t val)
{
	tss.esp0 = val;
}

void gdt_setup(void)
{
	gdt_ptr_t gdt_ptr;
	gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES_COUNT) - 1;
	gdt_ptr.base  = (unsigned int)&gdt_entries;

	memset((uint8_t*) &tss, 0, sizeof(tss));
	tss.ss0 = KERNEL_DS;

	gdt_set_gate(0, 0, 0, 0, 0);
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Ring 0 CS 0x8
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Ring 0 DS 0x10
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // Ring 3 CS 0x18
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // Ring 3 DS 0x20
	gdt_set_gate(5, (uint32_t) &tss, sizeof(tss), 0x89, 0x40); // TSS 0x28

	gdt_write((unsigned int)&gdt_ptr);

	// load task register with the TSS segment selector 0x28
	asm("ltr %%ax" :: "a"((uint16_t) TSS_SEG));
}

