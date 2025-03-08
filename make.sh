make
cp kernel.bin iso/kernel/kernel.bin
make clean
mkisofs -R -input-charset utf8 -b boot/grub/stage2_eltorito -boot-info-table -no-emul-boot -boot-load-size 4 -o os.iso iso
qemu-system-x86_64 -nic model=rtl8139 -cdrom os.iso -drive file=drive.img,format=raw,index=1,media=disk -boot order=d
