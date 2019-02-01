as --32 -o bootload.o bootload.asm
ld -Ttext 0x7c00 --oformat binary -m elf_i386 -o bootload.bin bootload.o
g++ -ffreestanding -m32 -o kernel.o -c kernel.cpp -w
ld --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o
qemu -fda bootload.bin -fdb kernel.bin -device isa-debug-exit,iobase=0xf4,iosize=0x04
