// file: "intr.h"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

#ifndef INTR_H
#define INTR_H

//-----------------------------------------------------------------------------

#include "general.h"
#include "asm.h"
#include "pic.h"
#include "apic.h"

//-----------------------------------------------------------------------------

#define CPU_EX_DIV_BY_ZERO 0x00
#define CPU_EX_DEBUG 0x01
#define CPU_EX_NMI 0x02
#define CPU_EX_BREAKPOINT 0x03
#define CPU_EX_OVERFLOW 0x04
#define CPU_EX_BOUND_RANGE_EXCEEDED 0x05
#define CPU_EX_INVALID_OPCODE 0x06
#define CPU_EX_DEV_NOT_AVAIL 0x07
#define CPU_EX_DOUBLE_FAULT 0x08
#define CPU_EX_COPROC_SEG_OVERRUN 0x09
#define CPU_EX_INVALID_TSS 0x0A
#define CPU_EX_SEGMENT_NO_PRESENT 0x0B
#define CPU_EX_STACK_SEGMENT_FAULT 0x0C
#define CPU_EX_GENERAL_PROTECTION_FAULT 0x0D
#define CPU_EX_PAGE_FAULT 0x0E
#define CPU_EX_RESERVED 0x0F

typedef struct interrupt_data {
  uint32 ds;
  uint32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32 int_no, error_code;
  uint32 eip, cs, eflags, useresp, ss;
} interrupt_data;


// Initialization of interrupt manager.

void setup_intr ();

// Enabling, disabling and acknowledging IRQs.

#define ENABLE_IRQ(n) \
do { \
     if ((n) < 8) \
       outb (inb (PIC_PORT_MASTER_IMR) & ~PIC_OCW1_MASK(n), \
             PIC_PORT_MASTER_OCW1); \
     else \
       outb (inb (PIC_PORT_SLAVE_IMR) & ~PIC_OCW1_MASK((n)-8), \
             PIC_PORT_SLAVE_OCW1); \
   } while (0)

#define DISABLE_IRQ(n) \
do { \
     if ((n) < 8) \
       outb (inb (PIC_PORT_MASTER_IMR) | PIC_OCW1_MASK(n), \
             PIC_PORT_MASTER_OCW1); \
     else \
       outb (inb (PIC_PORT_SLAVE_IMR) | PIC_OCW1_MASK((n)-8), \
             PIC_PORT_SLAVE_OCW1); \
   } while (0)

#define ACKNOWLEDGE_IRQ(n) \
do { \
     if ((n) < 8) \
       outb (PIC_OCW2_SPECIFIC_EOI(n), PIC_PORT_MASTER_OCW2); \
     else \
       { \
         outb (PIC_OCW2_SPECIFIC_EOI((n)-8), PIC_PORT_SLAVE_OCW2); \
         outb (PIC_OCW2_SPECIFIC_EOI(PIC_MASTER_IRQ2), PIC_PORT_MASTER_OCW2); \
       } \
   } while (0)

//-----------------------------------------------------------------------------

// Interrupt handlers must use C linkage.

extern void irq0(void* esp);
extern void irq1 ();
extern void irq2 ();
extern void irq3 ();
extern void irq4 ();
extern void irq5 ();
extern void irq6 ();
extern void irq7 ();
extern void irq8 ();
extern void irq9 ();
extern void irq10 ();
extern void irq11 ();
extern void irq12 ();
extern void irq13 ();
extern void irq14 ();
extern void irq15 ();
extern void APIC_timer_irq ();
extern void APIC_spurious_irq ();
extern void unhandled_interrupt (int num);
extern void interrupt_handle(interrupt_data data);
extern void sys_irq (void* esp);

//-----------------------------------------------------------------------------

#endif

// Local Variables: //
// mode: C     //
// End: //
