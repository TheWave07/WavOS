#ifndef __WAVOS__GDTDESC_H
#define __WAVOS__GDTDESC_H
#include <common/types.h>
void gdt_setup(void);
void change_tss_esp0(uint32_t);
enum gdt_gate_offsets {
	KERNEL_CS = 0x8,
	KERNEL_DS = 0x10,
	USER_CS = 0x18,
	USER_DS = 0x20,
	TSS_SEG = 0x28,
};

typedef struct {
	uint16_t previous_task, __previous_task_unused;
	uint32_t esp0;
	uint16_t ss0, __ss0_unused;
	uint32_t esp1;
	uint16_t ss1, __ss1_unused;
	uint32_t esp2;
	uint16_t ss2, __ss2_unused;
	uint32_t cr3;
	uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	uint16_t es, __es_unused;
	uint16_t cs, __cs_unused;
	uint16_t ss, __ss_unused;
	uint16_t ds, __ds_unused;
	uint16_t fs, __fs_unused;
	uint16_t gs, __gs_unused;
	uint16_t ldt_selector, __ldt_sel_unused;
	uint16_t debug_flag, io_map;
} __attribute__((packed)) TSS;


#endif
