CC:=i686-elf-gcc
CFLAGS:=-fno-merge-constants -c -std=gnu99 -O0 -ffreestanding -Wall -Wextra -Iinclude
LD:=i686-elf-ld
LDFLAGS:=-T linker.ld -o kernel.bin
ASM:=nasm
ASMFLAGS:=-f elf

SRCCFILES := $(shell find -type f -name "*.c")
SRCASMFILES := $(shell find -type f -name "*.asm")
OBJFILES := $(patsubst ./src/%.c, ./obj/%.o, $(SRCCFILES)) \
			$(patsubst ./src/%.asm, ./obj/%.o, $(SRCASMFILES)) \
		

DEPFILES := $(patsubst %.c,%.d,$(SRCCFILES))

all: $(OBJFILES)
	$(LD) $(LDFLAGS) $(OBJFILES)

obj/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.asm
	mkdir -p $(@D)
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	$(shell rm -rf obj kernel.bin)
