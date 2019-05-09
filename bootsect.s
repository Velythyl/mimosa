# file: "bootsect.s"

# Copyright (c) 2019 by Marc Feeley and Universit� de Montr�al, All
# Rights Reserved.
#
# Revision History
# 22 Sep 01  initial version (Marc Feeley)

# ------------------------------------------------------------------------------
INT13_READ_SECTOR_FN = 2        # BIOS int 0x13 function for "read sector"
INT10_TTY_OUTPUT_FN = 0xE       # BIOS int 0x10 function for "teletype output"
INT16_READ_KEYBOARD_FN = 0      # BIOS int 0x16 function for "read keyboard"
STACK_TOP = 0x10000             # location of stack top
SCRATCH = 0x1000                # location of scratch area
ROOT_DIR_ENTRY_SIZE = 32        # the size for a root directory entry size
# ------------------------------------------------------------------------------

  .globl bootsect_entry

bootsect_entry:

# Note: the BIOS always loads the boot sector of the disk at address 0000:7c00.

  .code16  # at this point the processor is in 16 bit real mode

# ------------------------------------------------------------------------------

code_start:

# This header will make the boot sector look like the one for an MSDOS floppy.

  .byte 0xeb,0x3c,0x90 # this is a jump instruction to "after_header"
  .byte 0x2a,0x26,0x41,0x66,0x3c, 0x49,0x48,0x43 # OEM name, number
nb_bytes_per_sector:
  .word 0x0200 # bytes per sector (512 bytes)
  .byte 0x01      # sector per allocation unit -> sector/cluster
nb_reserved_sectors:  
  .word 0x0001 # reserved sectors for booting 1
nb_of_fats:
  .byte 0x02      # number of FATs (usually 2)
nb_root_dir_entries:
  .word 0x00e8    # number of root dir entries
nb_logical_sectors:
  .word 0x0b40    # number of logical sectors
  .byte 0xf8      # media descriptor byte (f0h: floppy, f8h: disk drive)
nb_sectors_per_fat: 
  .word 0x0009    # sectors per fat
nb_sectors_per_track:
  .word 0x009 # sectors per track
nb_heads:
  .word 0x0000 # number of heads
nb_hidden_sectors:
  .word 0x0000 # number of hidden sectors
  .byte 0x00,0x00 # number of hidden sectors (high word)
  .byte 0x00,0x00,0x00,0x00 # total number of sectors in file system
drive:            # Extended block, supposed to be only for FAT 16
  .byte 0x80      # logical drive number
  .byte 0x00      # reserved
  .byte 0x29      # extended signature
  .byte 0xd1,0x07,0x22,0x27 # serial number
  .byte 0x4f,0x53,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20 # drive label
  .byte 0x46,0x41,0x54,0x31,0x32,0x20,0x20,0x20 # file system type (FAT 12)
# ------------------------------------------------------------------------------

after_header:

nb_root_sectors:
  .word 0x00000 # number of roots sectors
root_dir_sectors:
  .long 0x00    # default value on floppy is 33; should be read correctly



# Setup segments.
  cli

  xorw  %cx,%cx
  movw  %cx,%ds
  movw  %cx,%ss
  movw  $(STACK_TOP & 0xffff),%sp
  sti

# Print banner.
  movw  $banner,%si
  call  print_string

# Setup drive parameters, especially important for hard drive

  movb $0x08, %ah
  movb drive, %dl
  int $0x13

  jc cannot_load

  incb %dh                # dh is maximum head index, we increment by one to get head count
  
  shrw $8, %dx            # place dh into dl (effictively, also keeps dh to 0x00)
  movw %dx, nb_heads      # put the number of heads in the header
  andb $0x3f,%cl          # cl: 00s5......s1 (max sector)
  
  xorb %ch, %ch
  movw %cx, nb_sectors_per_track # the number of cylinder is useless for the LDA to CHS conversion

  # Prepare values for stage two loading

  movw $ROOT_DIR_ENTRY_SIZE, %ax # size in bytes of an entry in the root table
  xor %dx, %dx 
  mulw nb_root_dir_entries  # DX contains the high part, AX contains the low part of (number of entries * size of entries)
  # = directory size
  # Now we want the number of sectors occupied by the table
  divw nb_bytes_per_sector # ax now contains the number of sectors taken up by the table  
                           # dx contains the number of bytes extra
  

  # Calculating the start of the root dir
  movw nb_of_fats, %ax
  mulw nb_sectors_per_fat
  shll $16, %edx             # set the high part
  movw %ax, %dx
  addw nb_hidden_sectors, %dx
  addw nb_reserved_sectors, %dx
  # edx now contains the sector of the root directory

1: jmp 1b

  # Configure the reading

  movl  %edx,%eax
  movl  $KERNEL_START,%ebx
  movl  $KERNEL_SIZE,%ecx

  call reset_drive

next_sector:

  call  read_sector
  jnc   sector_was_read
  call reset_drive

  call  read_sector
  jnc   sector_was_read

  call reset_drive
  call  read_sector

  # Failure: give up
  jc    cannot_load

# ------------------------------

sector_was_read:

  incl  %eax
  xorl  %edx,%edx
  movw  nb_bytes_per_sector,%dx
  addl  %edx,%ebx
  subl  %edx,%ecx

  ja    next_sector

# Turn off floppy disk's motor.

 movw  $0x3f2,%dx
 xorb  %al,%al
 outb  %al,%dx

# Jump to kernel.
  ljmp  $(KERNEL_START>>4),$0  # jump to "KERNEL_START" (which must be < 1MB)

cannot_load:

  movw  $load_error,%si

print_message_and_reboot:

  call  print_string

  movb  $INT16_READ_KEYBOARD_FN,%ah
  int   $0x16 # read keyboard

# Reboot.

  ljmp  $0xf000,$0xfff0  # jump to 0xffff0 (the CPU starts there when reset)

reset_drive:

  pushl %eax
  pushl %edx

  movb  drive, %dl
  xor %ax, %ax
  int $0x13

  popl %edx
  popl %eax

  ret  


read_sector:

# Read one sector from relative sector offset %eax (with bootsector =
# 0) to %ebx.
# CF = 1 if an error reading the sector occured

  pushl %eax
  pushl %ebx
  pushl %ecx

  pushl %eax
  pushl %ebx
  movw  $progress,%si
  call  print_string
  popl  %ebx
  popl  %eax

  movl  %eax,%edx               # edx contains LDA
  shrl  $16,%edx                # dx contains LDA
  divw  nb_sectors_per_track    # %ax = cylinder, %dx = sector in track
  incw  %dx                     # increment sector
  movb  %dl,%cl                 # move the sector per track to cl
  xorw  %dx,%dx                 # erase dx
  divw  nb_heads                # %ax = track, %dx = head
  shlb  $6,%ah                  # keep the top 2 bits of track in ah
  orb   %ah,%cl                 # cl is top two bits of ah and sectors per track
  movb  %al,%ch                 # ch is the bottom part of the track
  movb  %dl,%dh                 # head is now in dh
  movb  drive,%dl               # dl is now the drive
  movl  %ebx,%eax               # put the target address in eax
  shrl  $4,%eax                 # div the address by 2^4
  movw  %ax,%es                 # insert the address in es
  andw  $0x0f,%bx
  movw  $0x0201,%ax # in AH, write 0x02 (command read) and in AL write 0x01 (1 sector to read)
  int   $0x13       # Call the read

  popl  %ecx
  popl  %ebx
  popl  %eax
  ret

#------------------------------------------------------------------------------

# Print string utility.

print_string_loop:
  movb  $INT10_TTY_OUTPUT_FN,%ah
  movw  $(0<<8)+7,%bx   # page = 0, foreground color = 7
  int   $0x10
  incw  %si
print_string:
  movb  (%si),%al
  test  %al,%al
  jnz   print_string_loop
  ret

stage_2_name:
  .ascii "boot.sys"
  .byte 0

banner:
  .ascii "Loading"
  .byte 0

progress:
  .ascii "."
  .byte 0

load_error:
  .byte 10,13
  .ascii "Could not load OS.  Press any key to reboot."
  .byte 10,13,0


#------------------------------------------------------------------------------

code_end:

  .space (1<<9)-(2 + 0)-(code_end-code_start)  # Skip to the end. The signature and the bootsector need to be written

  # Signature
  .byte 0x55
  .byte 0xaa
#------------------------------------------------------------------------------
