#include <hardwarecomms/isr.h>
#include <io/screen.h>
#include <hardwarecomms/portio.h>
#include <multitasking.h>
void (*isr_callbacks[16])();

void isr_handler(registers_t regs)
{
	terminal_write_string("Recieved interrupt: ");
	terminal_write_int(regs.int_no, 16);
	terminal_write_string("\n");
}

void irq_handler(registers_t regs)
{	outb(0x20, 0x20);
	if (regs.int_no >= IRQ8){
		outb(0xA0, 0x20);
	}
	if (isr_callbacks[regs.int_no-IRQ0]!=0){
		(*isr_callbacks[regs.int_no-IRQ0])();
	}
	else{
		/*terminal_write_string("Recieved IRQ: ");
		terminal_write_int(regs.int_no,16);
		terminal_write_string("\n");*/
	}
}

void register_isr_callback(int irq,void (*callback)()){
	isr_callbacks[irq-IRQ0]=callback;
}

