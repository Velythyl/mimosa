# file: "makefile"

OS_NAME = "\"MIMOSA version 1.2\""
KERNEL_START = 0x20000

KERNEL_OBJECTS = kernel.o libc/libc_os.o main.o fs.o ide.o disk.o thread.o chrono.o ps2.o term.o video.o intr.o rtlib.o fat32.o uart.o $(NETWORK_OBJECTS)
#NETWORK_OBJECTS =
#NETWORK_OBJECTS = eepro100.o tulip.o timer2.o misc.o pci.o config.o net.o
DEFS = -DUSE_IRQ4_FOR_UART -DUSE_IRQ1_FOR_KEYBOARD -DINCLUDE_EEPRO100 
#DEFS = -DINCLUDE_TULIP
#DEFS = -DINCLUDE_TULIP -DINCLUDE_EEPRO100 

GCC = gcc-3.4 -m32 -Wno-write-strings -ggdb3
GPP = g++-3.4 -m32 -Wno-write-strings -ggdb3

SPECIAL_OPTIONS =

GCC_OPTIONS = $(SPECIAL_OPTIONS) $(DEFS) -DOS_NAME=$(OS_NAME) -DKERNEL_START=$(KERNEL_START) -fomit-frame-pointer -fno-strict-aliasing -Wall -O3 -nostdinc -Iinclude -Ilibc

GPP_OPTIONS = $(GCC_OPTIONS) -fno-rtti -fno-builtin -fno-exceptions -nostdinc++

.SUFFIXES:
.SUFFIXES: .h .s .c .cpp .o .asm .bin .map .d

all: bin_files

rebuild: build createimg run

build:
	mkdir -p mimosa-build
	tar --exclude='*.img' -cf  - .  | ssh administrator@localhost -p 10022 "cd mimosa-build;tar xf -;rm -f kernel.bin; rm -f kernel.elf;make"
	# ssh administrator@localhost -p 10022 "cat mimosa-build/bootsect.bin" > mimosa-build/bootsect.bin
	# ssh administrator@localhost -p 10022 "cat mimosa-build/kernel.bin"   > mimosa-build/kernel.bin
	ssh administrator@localhost -p 10022 "cat mimosa-build/kernel.elf"   > mimosa-build/kernel.elf

createimg:
	ssh administrator@localhost -p 10022 "sudo mimosa-build/createimg.sh";
	ssh administrator@localhost -p 10022 "cat mimosa-build/floppy.img" > mimosa-build/floppy.img

run:
	qemu-system-i386 -s -m 1G -hda mimosa-build/floppy.img -debugcon stdio

run-with-telnet:
	qemu-system-i386 -s -m 1G -hda mimosa-build/floppy.img \
	-serial tcp:localhost:4444,server,nowait

debug:
	qemu-system-i386 -s -S -m 1G -hda mimosa-build/floppy.img -debugcon stdio

mf:
	make clean
	make SPECIAL_OPTIONS=-MMD
	sed "/^# dependencies:$$/q" makefile | cat - *.d > mf
	rm -f *.d
	mv makefile makefile.old
	mv mf makefile

bin_files: bootsect.bin kernel.bin

kernel.bin: $(KERNEL_OBJECTS)
	ld --script=script.ld $(KERNEL_OBJECTS) -o $*.bin -Ttext $(KERNEL_START) --omagic --entry=kernel_entry --oformat elf32-i386 -Map kernel.map
	cp kernel.bin kernel.elf
	objcopy -O binary kernel.elf kernel.bin

kernel.bss:
	cat kernel.map | grep '\.bss ' | grep -v '\.o' | sed 's/.*0x/0x/'

kernel.o: kernel.s
	as --defsym KERNEL_START=$(KERNEL_START) -o $*.o $*.s

.o.asm:
	objdump --disassemble-all $*.o > $*.asm

bootsect.o: bootsect.s kernel.bin
	as --defsym KERNEL_START=$(KERNEL_START) --defsym KERNEL_SIZE=`cat kernel.bin | wc --bytes | sed -e "s/ //g"` -o $*.o $*.s

bootsect.bin: bootsect.o
	ld $*.o -o $*.bin -Ttext 0x7c00 --omagic --entry=bootsect_entry --oformat binary -Map bootsect.map

.cpp.o:
	$(GPP) $(GPP_OPTIONS) -c -o $*.o $*.cpp

.c.o:
	$(GCC) $(GCC_OPTIONS) -c -o $*.o $*.c

.s.o: kernel.bin
	as --defsym OS_NAME=$(OS_NAME) --defsym KERNEL_START=$(KERNEL_START) --defsym KERNEL_SIZE=`cat kernel.bin | wc --bytes | sed -e "s/ //g"` -o $*.o $*.s

clean:
	ssh administrator@localhost -p 10022 "rm -rf mimosa-build;"
	rm -rf mimosa-build
	rm -f *.o *.asm *.bin *.tmp *.d

# dependencies:
config.o: config.c etherboot.h osdep.h include/asm.h include/general.h \
	nic.h pci.h cards.h
disk.o: disk.cpp include/disk.h include/general.h include/ide.h \
	include/thread.h include/intr.h include/asm.h include/pic.h \
	include/apic.h include/chrono.h include/pit.h include/queue.h \
	include/term.h include/video.h include/rtlib.h
eepro100.o: eepro100.c etherboot.h osdep.h include/asm.h \
	include/general.h nic.h pci.h cards.h timer2.h
# fifo.o: fifo.cpp include/fifo.h include/general.h include/thread.h \
	include/intr.h include/asm.h include/pic.h include/apic.h \
	include/chrono.h include/pit.h include/queue.h include/term.h \
	include/video.h include/rtlib.h
fs.o: fs.cpp include/fs.h include/general.h include/disk.h include/ide.h \
	include/thread.h include/intr.h include/asm.h include/pic.h \
	include/apic.h include/chrono.h include/pit.h include/queue.h \
	include/term.h include/video.h include/rtlib.h
ide.o: ide.cpp include/ide.h include/general.h include/thread.h \
	include/intr.h include/asm.h include/pic.h include/apic.h \
	include/chrono.h include/pit.h include/queue.h include/term.h \
	include/video.h include/rtlib.h include/disk.h
intr.o: intr.cpp include/intr.h include/general.h include/asm.h \
	include/pic.h include/apic.h include/term.h include/video.h
main.o: main.cpp include/general.h include/term.h include/video.h \
	include/thread.h include/intr.h include/asm.h \
	include/pic.h include/apic.h include/chrono.h include/pit.h \
	include/queue.h include/ps2.h include/fat32.h
misc.o: misc.c etherboot.h osdep.h include/asm.h include/general.h
net.o: net.cpp include/net.h include/general.h include/rtlib.h \
	include/term.h include/video.h include/chrono.h include/asm.h \
	include/pit.h include/thread.h include/intr.h include/pic.h \
	include/apic.h include/queue.h etherboot.h osdep.h nic.h
pci.o: pci.c etherboot.h osdep.h include/asm.h include/general.h pci.h
ps2.o: ps2.cpp include/ps2.h include/general.h include/intr.h \
	include/asm.h include/pic.h include/apic.h include/chrono.h include/pit.h \
	include/video.h include/term.h include/thread.h include/queue.h
rtlib.o: rtlib.cpp include/rtlib.h include/general.h include/intr.h \
	include/asm.h include/pic.h include/apic.h include/chrono.h include/pit.h \
	include/ide.h include/thread.h include/queue.h include/term.h \
	include/video.h include/disk.h include/fs.h include/ps2.h
term.o: term.cpp include/term.h include/general.h include/video.h
thread.o: thread.cpp include/thread.h include/general.h include/intr.h \
	include/asm.h include/pic.h include/apic.h include/chrono.h include/pit.h \
	include/queue.h include/term.h include/video.h include/rtlib.h
chrono.o: chrono.cpp include/chrono.h include/general.h include/asm.h \
	include/pit.h include/apic.h include/intr.h include/pic.h include/rtc.h \
	include/term.h include/video.h
timer2.o: timer2.c etherboot.h osdep.h include/asm.h include/general.h \
	timer2.h
tulip.o: tulip.c etherboot.h osdep.h include/asm.h include/general.h \
	nic.h pci.h cards.h
video.o: video.cpp include/video.h include/general.h include/asm.h \
	include/vga.h include/term.h mono_5x7.cpp mono_6x9.cpp
fat32.o: fat32.cpp include/fat32.h include/general.h

libc/libc_os.o: libc/libc_os.cpp \
                libc/include/dirent.h \
                libc/include/errno.h \
                libc/include/float.h \
                libc/include/libc_common.h \
                libc/include/libc_header.h \
                libc/include/libc_link.h \
                libc/include/libc_redirect.h \
                libc/include/limits.h \
                libc/include/math.h \
                libc/include/setjmp.h \
                libc/include/signal.h \
                libc/include/stddef.h \
                libc/include/stdio.h \
                libc/include/stdlib.h \
                libc/include/string.h \
                libc/include/sys/resource.h \
                libc/include/sys/time.h \
                libc/include/termios.h \
                libc/include/time.h \
                libc/include/unistd.h \
                libc/include/wchar.h \
                libc/src/dirent.c \
                libc/src/errno.c \
                libc/src/libc_link.c \
                libc/src/libc_support.c \
                libc/src/math.c \
                libc/src/setjmp.c \
                libc/src/signal.c \
                libc/src/stdio.c \
                libc/src/stdlib.c \
                libc/src/string.c \
                libc/src/sys_resource.c \
                libc/src/sys_time.c \
                libc/src/termios.c \
                libc/src/time.c \
                libc/src/unistd.c

uart.o: uart.cpp include/term.h include/general.h include/uart.h include/asm.h
