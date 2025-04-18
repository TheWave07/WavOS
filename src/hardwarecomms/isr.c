#include <hardwarecomms/isr.h>
#include <io/screen.h>
#include <hardwarecomms/portio.h>
#include <multitasking.h>
#include <syscalls.h>
void (*irq_callbacks[16])();

void isr_handler(registers_t regs)
{
	if((uint8_t) regs.int_no != 0x80) {
		terminal_write_string("Recieved interrupt: ");
		terminal_write_int(regs.int_no, 16);
		terminal_write_string("\n");
	} else {
		handle_syscall(regs);
	}
}

void irq_handler(registers_t regs)
{	outb(0x20, 0x20);
	if (regs.int_no >= IRQ8){
		outb(0xA0, 0x20);
	}
	if (irq_callbacks[regs.int_no-IRQ0]!=0){
		(*irq_callbacks[regs.int_no-IRQ0])();
	}
}

void register_irq_callback(int irq,void (*callback)()){
	irq_callbacks[irq-IRQ0]=callback;
}

