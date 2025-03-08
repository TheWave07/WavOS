#ifndef __WAVOS__MULTITASKING_H
#define __WAVOS__MULTITASKING_H

#include <common/types.h>
typedef struct registers
{
	uint32_t gs, fs, es, ds;
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t int_no, err_code;
	uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

typedef struct task
{
    uint32_t kstack; // kernel stack
	uint32_t kstack_bottom; // kernel stack bottom
} task_t;

typedef struct 
{
	uint32_t ebp, edi, esi, ebx;

	// popped by ret in switch_context
	uint32_t return_addr;

	// popped by us in new_task_entry
	uint32_t data_selector;

	// popped by iret in new_task_entry
	uint32_t eip, cs, eflags, useresp, ss;
} NewTaskInitStack;

extern void new_task_setup();
extern void switch_context();
bool add_task(task_t* task);
task_t create_task(uint32_t callback, uint32_t user_stack,  uint32_t kernel_stack, bool is_kernel_task);
void schedule();
#endif
