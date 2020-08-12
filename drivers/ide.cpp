// file: "ide.cpp"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 22 Sep 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "asm.h"
#include "disk.h"
#include "ide.h"
#include "include/pci.h"
#include "intr.h"
#include "rtlib.h"
#include "term.h"
#include "thread.h"

//-----------------------------------------------------------------------------

// IDE controller ports and IRQs.

uint8 controller_count = 0;
ide_controller ide_controller_map[IDE_CONTROLLERS];
//-----------------------------------------------------------------------------

typedef struct ide_module_struct {
  ide_controller ide[IDE_CONTROLLERS];
} ide_module;

static ide_module ide_mod;

static void ide_delay(uint16 port) {
  for (int i = 0; i < 4; i++)
    inb(port + IDE_ALT_STATUS_REG);
}

ide_cmd_queue_entry *ide_cmd_queue_alloc(ide_device *dev) {
  int32 i;
  ide_controller *ctrl;
  ide_cmd_queue_entry *entry;

  ASSERT_INTERRUPTS_DISABLED(); // Interrupts should be disabled at this point

  ctrl = dev->ctrl;

  while ((i = ctrl->cmd_queue_freelist) < 0)
    condvar_mutexless_wait(ctrl->cmd_queue_condvar);

  entry = &ctrl->cmd_queue[i];

  entry->dev = dev;
  entry->refcount = 2; // the interrupt handler and the client have to free me

  i = entry->next;

  ctrl->cmd_queue_freelist = i;

  if (i >= 0)
    condvar_mutexless_signal(ctrl->cmd_queue_condvar);

  return entry;
}

void ide_cmd_queue_free(ide_cmd_queue_entry *entry) {
  ide_device *dev;
  ide_controller *ctrl;

  ASSERT_INTERRUPTS_DISABLED(); // Interrupts should be disabled at this point

  dev = entry->dev;
  ctrl = dev->ctrl;

  if (--entry->refcount == 0) {
    entry->next = ctrl->cmd_queue_freelist;
    ctrl->cmd_queue_freelist = entry->id;
    condvar_mutexless_signal(ctrl->cmd_queue_condvar);
  }
}

void ide_irq(ide_controller *ctrl) {
  uint8 s;
  uint32 i;
  ide_cmd_queue_entry *entry;
  uint16 base;
  uint16 *p = NULL;

  entry = &ctrl->cmd_queue[0]; // We only handle one operation at a time
  base = ide_controller_map[ctrl->id].base;

  cmd_type type = entry->cmd;

  if (type == cmd_read_sectors) {
    p = CAST(uint16 *, entry->_.read_sectors.buf);
  } else if (type == cmd_write_sectors) {
    p = CAST(uint16 *, entry->_.write_sectors.buf);
  } else if (type == cmd_flush_cache) {
    p = NULL;
  } else {
    panic(L"[IDE.CPP] Unknown command type...");
  }

  s = inb(base + IDE_STATUS_REG);

  if (s & IDE_STATUS_ERR) {
    // #ifdef SHOW_DISK_INFO
    uint8 err = inb(base + IDE_ERROR_REG);
    term_write(cout, "***IDE ERROR***\n");

    if (err & IDE_ERROR_BBK)
      term_write(cout, "Bad block mark detected in sector's ID field\n");
    if (err & IDE_ERROR_UNC)
      term_write(cout, "Uncorrectable data error encountered\n");
    if (err & IDE_ERROR_IDNF)
      term_write(cout, "Requested sector's ID field not found\n");
    if (err & IDE_ERROR_ABRT)
      term_write(cout, "Command aborted (status error or invalid cmd)\n");
    if (err & IDE_ERROR_TK0NF)
      term_write(cout, "Track 0 not found during recalibrate command\n");
    if (err & IDE_ERROR_AMNF)
      term_write(cout, "Data address mark not found after ID field\n");
    // #endif

    if (type == cmd_read_sectors) {
      entry->_.read_sectors.err = UNKNOWN_ERROR;
    } else if (type == cmd_write_sectors) {
      entry->_.write_sectors.err = UNKNOWN_ERROR;
    }
    condvar_mutexless_signal(entry->done);
    ide_cmd_queue_free(entry);
  } else if (type == cmd_read_sectors) {
    for (i = entry->_.read_sectors.count << (IDE_LOG2_SECTOR_SIZE - 1); i > 0;
         i--)
      *p++ = inw(base + IDE_DATA_REG);

    if (inb(base + IDE_ALT_STATUS_REG) & IDE_STATUS_DRQ) {
      entry->_.read_sectors.err = UNKNOWN_ERROR;
    } else {
      entry->_.read_sectors.err = NO_ERROR;
    }
    condvar_mutexless_signal(entry->done);
    ide_cmd_queue_free(entry);
  } else if (type == cmd_write_sectors) {
    if (entry->_.write_sectors.written < entry->_.write_sectors.count) {
      // Write the next sector to write
      for (uint16 i = 1 << (IDE_LOG2_SECTOR_SIZE - 1); i > 0; i--) {
        outw(*p++, base + IDE_DATA_REG);
      }
      entry->_.write_sectors.buf = p;
      entry->_.write_sectors.written++;
    } else {
      // This is the status interrupt
      if (inb(base + IDE_ALT_STATUS_REG) & IDE_STATUS_DRQ) {
        entry->_.write_sectors.err = UNKNOWN_ERROR;
      } else {
        entry->_.write_sectors.err = NO_ERROR;
      }
      condvar_mutexless_signal(entry->done);
      ide_cmd_queue_free(entry);
    }
  } else if (type == cmd_flush_cache) {
    condvar_mutexless_signal(entry->done);
    ide_cmd_queue_free(entry);
  }
}

#ifdef USE_IRQ14_FOR_IDE0

extern "C" void irq14() {
#ifdef SHOW_INTERRUPTS
  term_write(cout, "\033[41m irq14 \033[0m");
#endif

  if (has_cut_ide_support()) {
    uint8 params[1] = {0};
    send_gambit_int(GAMBIT_IDE_INT, params, 1);
  } else {
    ACKNOWLEDGE_IRQ(14);
    ide_irq(&ide_mod.ide[0]);
  }
}

#endif

#ifdef USE_IRQ15_FOR_IDE1

extern "C" void irq15() {
#ifdef SHOW_INTERRUPTS
  term_write(cout, "\033[41m irq15 \033[0m");
#endif

  if (has_cut_ide_support()) {
    uint8 params[1] = {1};
    send_gambit_int(GAMBIT_IDE_INT, params, 1);
  } else {
    ACKNOWLEDGE_IRQ(15);
    ide_irq(&ide_mod.ide[1]);
  }
}

#endif

error_code ide_read_sectors(ide_device *dev, uint32 lba, void *buf,
                            uint32 count) {
  error_code err = NO_ERROR;

  ASSERT_INTERRUPTS_ENABLED(); // Interrupts should be enabled at this point

  if (count > 0) {
    ide_controller *ctrl = dev->ctrl;
    uint16 base = ide_controller_map[ctrl->id].base;
    ide_cmd_queue_entry *entry;

    disable_interrupts();
    entry = ide_cmd_queue_alloc(dev);

    if (count > 256)
      count = 256;

    entry->cmd = cmd_read_sectors;
    entry->_.read_sectors.buf = buf;
    entry->_.read_sectors.count = count;

    outb(IDE_DEV_HEAD_LBA | IDE_DEV_HEAD_DEV(dev->id) | (lba >> 24),
         base + IDE_DEV_HEAD_REG);
    outb(count, base + IDE_SECT_COUNT_REG);
    outb(lba, base + IDE_SECT_NUM_REG);
    outb((lba >> 8), base + IDE_CYL_LO_REG);
    outb((lba >> 16), base + IDE_CYL_HI_REG);
    outb(IDE_READ_SECTORS_CMD, base + IDE_COMMAND_REG);

    condvar_mutexless_wait(entry->done);

    err = entry->_.read_sectors.err;

    ide_cmd_queue_free(entry);

    enable_interrupts();
  }

  return err;
}

error_code ide_write_sectors(ide_device *dev, uint32 lba, void *buf,
                             uint32 count) {
  error_code err = NO_ERROR;

  ASSERT_INTERRUPTS_ENABLED(); // Interrupts should be enabled at this point

  if (count > 0) {
    ide_controller *ctrl = dev->ctrl;
    uint16 base = ide_controller_map[ctrl->id].base;
    ide_cmd_queue_entry *entry;

    disable_interrupts();

    if (count != 1)
      panic(L"Only one sector supported...");
    if (count > 256)
      count = 256;

    entry = ide_cmd_queue_alloc(dev);
    entry->cmd = cmd_write_sectors;
    entry->_.write_sectors.buf = buf;
    entry->_.write_sectors.count = count;
    entry->_.write_sectors.written = 1; // We write a sector right now

    outb(IDE_DEV_HEAD_LBA | IDE_DEV_HEAD_DEV(dev->id) | (lba >> 24),
         base + IDE_DEV_HEAD_REG);
    outb(count, base + IDE_SECT_COUNT_REG);
    outb(lba, base + IDE_SECT_NUM_REG);
    outb((lba >> 8), base + IDE_CYL_LO_REG);
    outb((lba >> 16), base + IDE_CYL_HI_REG);
    outb(IDE_WRITE_SECTORS_CMD, base + IDE_COMMAND_REG);

    while ((inb(base + IDE_STATUS_REG) & IDE_STATUS_BSY) ||
           !(inb(base + IDE_STATUS_REG) & IDE_STATUS_DRQ))
      ide_delay(base);

    uint16 *p = CAST(uint16 *, entry->_.write_sectors.buf);

    // Write the first sector immediately
    for (uint16 i = (1 << (IDE_LOG2_SECTOR_SIZE - 1)); i > 0; i--) {
      outw(*p++, base + IDE_DATA_REG);
    }

    entry->_.write_sectors.buf = p; // So we can write from there

    condvar_mutexless_wait(entry->done);
    err = entry->_.write_sectors.err;
    ide_cmd_queue_free(entry);

    // Flush the command buffer
    entry = ide_cmd_queue_alloc(dev);
    entry->cmd = cmd_flush_cache;

    outb(IDE_FLUSH_CACHE_CMD, base + IDE_COMMAND_REG);
    condvar_mutexless_wait(entry->done);

    ide_cmd_queue_free(entry);
    enable_interrupts();
  };

  return err;
}

static void swap_and_trim(native_string dst, uint16 *src, uint32 len) {
  uint32 i;
  uint32 end = 0;

  for (i = 0; i < len; i++) {
    dst[i] = CAST(native_char, (i & 1) ? src[i >> 1] : (src[i >> 1] >> 8));
    if (dst[i] != ' ')
      end = i + 1;
  }

  dst[end] = '\0';
}

static void setup_ide_device(ide_controller *ctrl, ide_device *dev, uint8 id) {
  uint32 i;
  uint32 j;
  uint16 ident[1 << (IDE_LOG2_SECTOR_SIZE - 1)];
  uint16 base;

  dev->id = id;
  dev->ctrl = ctrl;

  if (dev->kind == IDE_DEVICE_ABSENT)
    return;

  base = ide_controller_map[ctrl->id].base;

  // perform an IDENTIFY DEVICE or IDENTIFY PACKET DEVICE command

  outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(dev->id), base + IDE_DEV_HEAD_REG);

  if (dev->kind == IDE_DEVICE_ATA) {
    outb(IDE_IDENTIFY_DEVICE_CMD, base + IDE_COMMAND_REG);
  } else {
    outb(IDE_IDENTIFY_PACKET_DEVICE_CMD, base + IDE_COMMAND_REG);
  }

  for (j = 1000000; j > 0; j--) // wait up to 1 second for a response
  {
    uint8 stat = inb(base + IDE_STATUS_REG);

    if (!(stat & IDE_STATUS_BSY)) {
      if (stat & IDE_STATUS_ERR)
        j = 0;
      break;
    }

    thread_sleep(1000); // 1 usec
  }

  if (j == 0) {
    dev->kind = IDE_DEVICE_ABSENT;
    return;
  }

  for (i = 0; i < (1 << (IDE_LOG2_SECTOR_SIZE - 1)); i++) {
    ident[i] = inw(base + IDE_DATA_REG);
  }

  swap_and_trim(dev->serial_num, ident + 10, 20);
  swap_and_trim(dev->firmware_rev, ident + 23, 8);
  swap_and_trim(dev->model_num, ident + 27, 40);

  dev->cylinders_per_disk = 0;
  dev->heads_per_cylinder = 0;
  dev->sectors_per_track = 0;
  dev->total_sectors_when_using_CHS = 0;
  dev->total_sectors = 0;

  if (dev->kind == IDE_DEVICE_ATA) {
    dev->cylinders_per_disk = ident[1];
    dev->heads_per_cylinder = ident[3];
    dev->sectors_per_track = ident[6];
    dev->total_sectors = (CAST(uint32, ident[61]) << 16) + ident[60];

    if (ident[53] & (1 << 0)) {
      dev->cylinders_per_disk = ident[54];
      dev->heads_per_cylinder = ident[55];
      dev->sectors_per_track = ident[56];
      dev->total_sectors_when_using_CHS =
          (CAST(uint32, ident[58]) << 16) + ident[57];
    }
  }

#if 0
  debug_write("dev->serial_num");
  debug_write(dev->serial_num);
  debug_write("dev->firmware_rev");
  debug_write(dev->firmware_rev);
  debug_write("dev->model_num");
  debug_write(dev->model_num);
  debug_write("dev->cylinders_per_disk");
  debug_write(dev->cylinders_per_disk);
  debug_write("dev->heads_per_cylinder");
  debug_write(dev->heads_per_cylinder);
  debug_write("dev->sectors_per_track");
  debug_write(dev->sectors_per_track);
  debug_write("dev->total_sectors_when_using_CHS");
  debug_write(dev->total_sectors_when_using_CHS);
  debug_write("dev->total_sectors");
  debug_write(dev->total_sectors);
#endif

  term_write(cout, "  word 47 hi (should be 128) = ");
  term_write(cout, (ident[47] >> 8));
  term_writeline(cout);
  term_write(cout, "  Maximum number of sectors that shall be transferred per "
                   "interrupt on READ/WRITE MULTIPLE commands = ");
  term_write(cout, (ident[47] & 0xff));
  term_writeline(cout);

#ifdef SHOW_IDE_INFO

  if (dev->kind == IDE_DEVICE_ATA) {
    if ((ident[0] & (1 << 15)) == 0)
      term_write(cout, "  ATA device\n");
  } else {
    if ((ident[0] & (3 << 14)) == (2 << 14))
      term_write(cout, "  ATAPI device\n");
  }

  if ((ident[0] & (1 << 7)) == 1)
    term_write(cout, "  removable media device\n");

#if 0

  if ((ident[0] & (1<<6)) == 1)
    term_write(cout, "  not removable controller and/or device\n");

#endif

  if ((ident[0] & (1 << 2)) == 1)
    term_write(cout, "  response incomplete\n");

  if (dev->kind == IDE_DEVICE_ATA) {
    term_write(cout, "  Number of logical cylinders = ");
    term_write(cout, ident[1]);
    term_write(cout, "\n");
    term_write(cout, "  Number of logical heads = ");
    term_write(cout, ident[3]);
    term_write(cout, "\n");
    term_write(cout, "  Number of logical sectors per logical track = ");
    term_write(cout, ident[6]);
    term_write(cout, "\n");
  }

  term_write(cout, "  Serial number = ");
  term_write(cout, dev->serial_num);
  term_write(cout, "\n");
  term_write(cout, "  Firmware revision = ");
  term_write(cout, dev->firmware_rev);
  term_write(cout, "\n");
  term_write(cout, "  Model number = ");
  term_write(cout, dev->model_num);
  term_write(cout, "\n");

#if 0

  term_write(cout, "  word 47 hi (should be 128) = ") << (ident[47] >> 8) << "\n";
  term_write(cout, "  Maximum number of sectors that shall be transferred per interrupt on READ/WRITE MULTIPLE commands = ") << (ident[47] & 0xff) << "\n";

#endif

  if (dev->kind == IDE_DEVICE_ATA) {
    if (ident[53] & (1 << 0)) {
      term_write(cout, "  Number of current logical cylinders = ");
      term_write(cout, ident[54]);
      term_write(cout, "\n");
      term_write(cout, "  Number of current logical heads = ");
      term_write(cout, ident[55]);
      term_write(cout, "\n");
      term_write(cout, "  Number of current logical sectors per track = ");
      term_write(cout, ident[56]);
      term_write(cout, "\n");
      term_write(cout, "  Current capacity in sectors = ");
      term_write(cout, (CAST(uint32, ident[58]) << 16) + ident[57]);
      term_write(cout, "\n");
    }

    term_write(cout,
               "  Total number of user addressable sectors (LBA mode only) = ");
    term_write(cout, (CAST(uint32, ident[61]) << 16) + ident[60]);
    term_write(cout, "\n");
  }

  if (ident[63] & (1 << 10))
    term_write(cout, "  Multiword DMA mode 2 is selected\n");

  if (ident[63] & (1 << 9))
    term_write(cout, "  Multiword DMA mode 1 is selected\n");

  if (ident[63] & (1 << 8))
    term_write(cout, "  Multiword DMA mode 0 is selected\n");

  if (ident[63] & (1 << 2))
    term_write(cout, "  Multiword DMA mode 2 and below are supported\n");

  if (ident[63] & (1 << 1))
    term_write(cout, "  Multiword DMA mode 1 and below are supported\n");

  if (ident[63] & (1 << 0))
    term_write(cout, "  Multiword DMA mode 0 is supported\n");

  term_write(cout, "  Maximum queue depth � 1 = ");
  term_write(cout, (ident[75] & 31));
  term_write(cout, "\n");

  if (ident[80] & (1 << 5))
    term_write(cout, "  supports ATA/ATAPI-5\n");

  if (ident[80] & (1 << 4))
    term_write(cout, "  supports ATA/ATAPI-4\n");

  if (ident[80] & (1 << 3))
    term_write(cout, "  supports ATA-3\n");

  if (ident[80] & (1 << 2))
    term_write(cout, "  supports ATA-2\n");

  term_write(cout, "  Minor version number = ");
  term_write(cout, ident[81]);
  term_write(cout, "\n");

  term_write(cout, "  supports:");

  if (ident[82] & (1 << 14))
    term_write(cout, " NOP command,");

  if (ident[82] & (1 << 13))
    term_write(cout, " READ BUFFER command,");

  if (ident[82] & (1 << 12))
    term_write(cout, " WRITE BUFFER command,");

  if (ident[82] & (1 << 10))
    term_write(cout, " Host Protected Area feature set,");

  if (ident[82] & (1 << 9))
    term_write(cout, " DEVICE RESET command,");

  if (ident[82] & (1 << 8))
    term_write(cout, " SERVICE interrupt,");

  if (ident[82] & (1 << 7))
    term_write(cout, " release interrupt,");

  if (ident[82] & (1 << 6))
    term_write(cout, " look-ahead,");

  if (ident[82] & (1 << 5))
    term_write(cout, " write cache,");

  if (ident[82] & (1 << 3))
    term_write(cout, " Power Management feature set,");

  if (ident[82] & (1 << 2))
    term_write(cout, " Removable Media feature set,");

  if (ident[82] & (1 << 1))
    term_write(cout, " Security Mode feature set,");

  if (ident[82] & (1 << 0))
    term_write(cout, " SMART feature set,");

  if (ident[83] & (1 << 8))
    term_write(cout, " SET MAX security extension,");

  if (ident[83] & (1 << 6))
    term_write(cout,
               " SET FEATURES subcommand required to spinup after power-up,");

  if (ident[83] & (1 << 5))
    term_write(cout, " Power-Up In Standby feature set,");

  if (ident[83] & (1 << 4))
    term_write(cout, " Removable Media Status Notification feature set,");

  if (ident[83] & (1 << 3))
    term_write(cout, " Advanced Power Management feature set,");

  if (ident[83] & (1 << 2))
    term_write(cout, " CFA feature set,");

  if (ident[83] & (1 << 1))
    term_write(cout, " READ/WRITE DMA QUEUED,");

  if (ident[83] & (1 << 0))
    term_write(cout, " DOWNLOAD MICROCODE command,");

  term_write(cout, "\n");

  term_write(cout, "  enabled:");

  if (ident[85] & (1 << 14))
    term_write(cout, " NOP command,");

  if (ident[85] & (1 << 13))
    term_write(cout, " READ BUFFER command,");

  if (ident[85] & (1 << 12))
    term_write(cout, " WRITE BUFFER command,");

  if (ident[85] & (1 << 10))
    term_write(cout, " Host Protected Area feature set,");

  if (ident[85] & (1 << 9))
    term_write(cout, " DEVICE RESET command,");

  if (ident[85] & (1 << 8))
    term_write(cout, " SERVICE interrupt,");

  if (ident[85] & (1 << 7))
    term_write(cout, " release interrupt,");

  if (ident[85] & (1 << 6))
    term_write(cout, " look-ahead,");

  if (ident[85] & (1 << 5))
    term_write(cout, " write cache,");

  if (ident[85] & (1 << 3))
    term_write(cout, " Power Management feature set,");

  if (ident[85] & (1 << 2))
    term_write(cout, " Removable Media feature set,");

  if (ident[85] & (1 << 1))
    term_write(cout, " Security Mode feature set,");

  if (ident[85] & (1 << 0))
    term_write(cout, " SMART feature set,");

  if (ident[86] & (1 << 8))
    term_write(cout, " SET MAX security extension,");

  if (ident[86] & (1 << 6))
    term_write(cout,
               " SET FEATURES subcommand required to spin-up after power-up,");

  if (ident[86] & (1 << 5))
    term_write(cout, " Power-Up In Standby feature set,");

  if (ident[86] & (1 << 4))
    term_write(cout, " Removable Media Status Notification feature set,");

  if (ident[86] & (1 << 3))
    term_write(cout, " Advanced Power Management feature set,");

  if (ident[86] & (1 << 2))
    term_write(cout, " CFA feature set,");

  if (ident[86] & (1 << 1))
    term_write(cout, " READ/WRITE DMA QUEUED command,");

  if (ident[86] & (1 << 0))
    term_write(cout, " DOWNLOAD MICROCODE command,");

  term_write(cout, "\n");

  if (ident[53] & (1 << 2)) {
    if (ident[88] & (1 << 12))
      term_write(cout, "  Ultra DMA mode 4 is selected\n");

    if (ident[88] & (1 << 11))
      term_write(cout, "  Ultra DMA mode 3 is selected\n");

    if (ident[88] & (1 << 10))
      term_write(cout, "  Ultra DMA mode 2 is selected\n");

    if (ident[88] & (1 << 9))
      term_write(cout, "  Ultra DMA mode 1 is selected\n");

    if (ident[88] & (1 << 8))
      term_write(cout, "  Ultra DMA mode 0 is selected\n");

    if (ident[88] & (1 << 4))
      term_write(cout, "  Ultra DMA mode 4 and below are supported\n");

    if (ident[88] & (1 << 3))
      term_write(cout, "  Ultra DMA mode 3 and below are supported\n");

    if (ident[88] & (1 << 2))
      term_write(cout, "  Ultra DMA mode 2 and below are supported\n");

    if (ident[88] & (1 << 1))
      term_write(cout, "  Ultra DMA mode 1 and below are supported\n");

    if (ident[88] & (1 << 0))
      term_write(cout, "  Ultra DMA mode 0 is supported\n");
  }

  if (ident[88] & (1 << 0))
    term_write(cout, "  Ultra DMA mode 0 is supported\n");

#endif
}

static void setup_ide_controller(ide_controller *ctrl, uint8 id) {
  term_write(cout, "Setting up IDE controller no: ");
  term_write(cout, id);
  term_writeline(cout);

  uint32 i;
  uint32 j;
  uint8 stat[IDE_DEVICES_PER_CONTROLLER];
  uint8 candidates;
  uint16 base = ide_controller_map[id].base;

  ctrl->id = id;

  // check each device to see if it is present

  candidates = 0;

  for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
#ifdef SHOW_IDE_INFO
    term_write(cout, "Device of controller no: ");
    term_write(cout, i);
    term_writeline(cout);
#endif

    outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(i), base + IDE_DEV_HEAD_REG);
#ifdef SHOW_IDE_INFO
    term_write(cout, "[START] Sleeping 400 nsecs\n");
#endif
    ide_delay(base); // 400 nsecs
#ifdef SHOW_IDE_INFO
    term_write(cout, "[STOP ] Sleeping 400 nsecs\n");
#endif

    stat[i] = inb(base + IDE_STATUS_REG);

    if ((stat[i] & (IDE_STATUS_BSY | IDE_STATUS_RDY | IDE_STATUS_DF |
                    IDE_STATUS_DSC | IDE_STATUS_DRQ)) !=
        (IDE_STATUS_BSY | IDE_STATUS_RDY | IDE_STATUS_DF | IDE_STATUS_DSC |
         IDE_STATUS_DRQ)) {
      ctrl->device[i].kind = IDE_DEVICE_ATAPI; // for now means the device
      candidates++;                            // is possibly present
    } else {
      ctrl->device[i].kind = IDE_DEVICE_ABSENT;
    }
  }

  if (candidates > 0) {
    // perform a software RESET of the IDE controller
#ifdef SHOW_IDE_INFO
    term_write(cout, "Resetting the IDE: ");
    term_write(cout, candidates);
    term_writeline(cout);
#endif

#ifdef SHOW_IDE_INFO
    term_write(cout, "SELECT DISK 0\n");
#endif

    outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(0), base + IDE_DEV_HEAD_REG);
    ide_delay(base); // 400 nsecs
    inb(base + IDE_STATUS_REG);

#ifdef SHOW_IDE_INFO
    term_write(cout, "READ STT\n");
#endif
    thread_sleep(5000); // 5 usecs
    outb(IDE_DEV_CTRL_nIEN, base + IDE_DEV_CTRL_REG);
#ifdef SHOW_IDE_INFO
    term_write(cout, "A\n");
#endif
    thread_sleep(5000); // 5 usecs
    outb(IDE_DEV_CTRL_nIEN | IDE_DEV_CTRL_SRST, base + IDE_DEV_CTRL_REG);
#ifdef SHOW_IDE_INFO
    term_write(cout, "B\n");
#endif
    thread_sleep(5000); // 5 usecs
    outb(IDE_DEV_CTRL_nIEN, base + IDE_DEV_CTRL_REG);
#ifdef SHOW_IDE_INFO
    term_write(cout, "C\n");
#endif
    /* thread_sleep(2000000); // 2 msecs */
    /* inb(base + IDE_ERROR_REG); */
#ifdef SHOW_IDE_INFO
    term_write(cout, "D\n");
    term_writeline(cout);
    term_writeline(cout);
    term_writeline(cout);
    term_writeline(cout);
#endif
    /* thread_sleep(5000); // 5 usecs */

#ifdef SHOW_IDE_INFO
    term_write(cout, "Before LOOP\n");
#endif

    for (j = 30000; j > 0; j--) // wait up to 30 seconds for a response
    {
      for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
        term_write(cout, "Reset of device ");
        term_write(cout, i);
        term_writeline(cout);

        outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(i), base + IDE_DEV_HEAD_REG);
        ide_delay(base); // 400 nsecs
        if (((inb(base + IDE_STATUS_REG) & IDE_STATUS_BSY) == 0) &&
            ctrl->device[i].kind == IDE_DEVICE_ATAPI) {
          ctrl->device[i].kind = IDE_DEVICE_ATA;
          candidates--;
        }
      }

      if (candidates == 0)
        break;

      thread_sleep(1000000); // 1 msec
    }

    candidates = 0;

    for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
      if (ctrl->device[i].kind == IDE_DEVICE_ATA)
        candidates++;
      else
        ctrl->device[i].kind = IDE_DEVICE_ABSENT;
    }

    if (candidates > 0) {
      for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
        outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(i), base + IDE_DEV_HEAD_REG);
        ide_delay(base); // 400 nsecs

        if (inb(base + IDE_CYL_LO_REG) == 0x14 &&
            inb(base + IDE_CYL_HI_REG) == 0xeb &&
            ctrl->device[i].kind == IDE_DEVICE_ATA)
          ctrl->device[i].kind = IDE_DEVICE_ATAPI;
      }

      for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
        if (ctrl->device[i].kind == IDE_DEVICE_ATA) {
          if (stat[i] == 0)
            ctrl->device[i].kind = IDE_DEVICE_ABSENT;
          else {
            outb(IDE_DEV_HEAD_IBM | IDE_DEV_HEAD_DEV(i),
                 base + IDE_DEV_HEAD_REG);
            ide_delay(base); // 400 nsecs

            outb(0x58, base + IDE_ERROR_REG);
            outb(0xa5, base + IDE_CYL_LO_REG);
            if (inb(base + IDE_ERROR_REG) == 0x58 ||
                inb(base + IDE_CYL_LO_REG) != 0xa5)
              ctrl->device[i].kind = IDE_DEVICE_ABSENT;
          }
        }
      }
    }
  }

  // setup each device

  candidates = 0;

  for (i = 0; i < IDE_DEVICES_PER_CONTROLLER; i++) {
#ifdef SHOW_IDE_INFO

    if (ctrl->device[i].kind != IDE_DEVICE_ABSENT) {
      term_write(cout, "ide");
      term_write(cout, ctrl->id);
      term_write(cout, ".");
      term_write(cout, i);

      if (ctrl->device[i].kind == IDE_DEVICE_ATA)
        term_write(cout, " is ATA\n");
      else
        term_write(cout, " is ATAPI\n");
    }

#endif

    setup_ide_device(ctrl, &ctrl->device[i], i);

    if (ctrl->device[i].kind != IDE_DEVICE_ABSENT)
      candidates++;
  }

  // setup command queue

  for (i = 0; i < MAX_NB_IDE_CMD_QUEUE_ENTRIES; i++) {
    ide_cmd_queue_entry *entry = &ctrl->cmd_queue[i];
    entry->id = i;
    entry->next = i + 1;
    entry->done = new_condvar(CAST(condvar *, kmalloc(sizeof(condvar))));
  }

  ctrl->cmd_queue[MAX_NB_IDE_CMD_QUEUE_ENTRIES - 1].next = -1;

  ctrl->cmd_queue_freelist = 0;
  ctrl->cmd_queue_condvar =
      new_condvar(CAST(condvar *, kmalloc(sizeof(condvar))));

  if (candidates > 0) {
    // enable interrupts

    outb(0, base + IDE_DEV_CTRL_REG);
    thread_sleep(2000000); // 2 msecs

    ENABLE_IRQ(ide_controller_map[id].irq);
  }
}

/**
 * This function is called when an IDE controller is detected
 * on a pci bus. The bus, device and function parameter
 * corresponds to the device's PCI address.
 *
 */
void ide_found_controller(uint16 bus, uint8 device, uint8 function,
                          uint32 info) {

#ifdef SHOW_IDE_INFO
  term_write(cout, (native_string) "Found an IDE controller at ");
  term_write(cout, bus);
  term_write(cout, (native_string) " ");
  term_write(cout, device);
  term_write(cout, (native_string) " ");
  term_write(cout, function);
  term_writeline(cout);
#endif

  if (controller_count + 2 > IDE_CONTROLLERS) {
#ifdef SHOW_IDE_INFO
    term_write(cout, (native_string) "Discarding, full capacity\n");
#endif
    return; // Full capacity
  }

  uint32 pci_info =
      pci_read_conf(bus, device, function, PCI_HEADER_PCI_INFO_OFFSET);

  uint32 header_type = (pci_info >> 16) & 0xFF;

  if (0x00 != header_type) {
#ifdef SHOW_IDE_INFO
    term_write(cout, (native_string) "Discarding, unknown header type ");
    term_write(cout, header_type);
    term_writeline(cout);
#endif
    // We expect PCI header type 0x00 for IDE controllers
    return;
  }

  // Determine if this is IDE
  uint8 prog_interface = (info >> 8) & 0xFF;

#ifdef SHOW_IDE_INFO
  term_write(cout, (native_string) "PROG-IF: ");
  term_write(cout, prog_interface);
  term_writeline(cout);
#endif

  uint32 manuf_info =
      pci_read_conf(bus, device, function, PCI_HEADER_MANUFACTURER_OFFSET);

  uint32 bars[6] = {
      PCI_HEADER_0_BAR0, PCI_HEADER_0_BAR1, PCI_HEADER_0_BAR2,
      PCI_HEADER_0_BAR3, PCI_HEADER_0_BAR4, PCI_HEADER_0_BAR5,
  };

  // Select IRQ
  uint32 irq_line =
      pci_read_conf(bus, device, function, PCI_HEADER_0_INT_OFFSET);
  uint8 irq_no = irq_line & 0xFF; // if serial, they will have the same IRQ

#ifdef SHOW_IDE_INFO
  term_write(cout, (native_string) "IRQ: ");
  term_write(cout, irq_no);
  term_writeline(cout);
#endif

  bool pata = (!prog_interface) || (prog_interface & IDE_PCI_PATA_PROG_IF_A) ||
              (prog_interface & IDE_PCI_PATA_PROG_IF_B);

#ifdef SHOW_IDE_INFO
  term_write(cout, (native_string) "Is PATA? ");
  term_write(cout, pata);
  term_writeline(cout);
#endif

  // Setup bars
  for (uint8 bar = 0; bar < 6; ++bar) {
    uint8 bar_offset = bars[bar];
    bars[bar] = pci_read_conf(bus, device, function, bar_offset) & IDE_BAR_MASK;
  }

  // Set bar's value correctly, revert to default is 0
  bars[0] = (bars[0]) + (IDE_PATA_FIRST_CONTROLLER_BASE * (!bars[0]));
  bars[1] = (bars[1]) + (IDE_PATA_FIRST_CONTROLLER * (!bars[1]));
  bars[2] = (bars[2]) + (IDE_PATA_SECOND_CONTROLLER_BASE * (!bars[2]));
  bars[3] = (bars[3]) + (IDE_PATA_SECOND_CONTROLLER * (!bars[3]));

#ifdef SHOW_IDE_INFO
  term_write(cout, (native_string) "BARS: \n");
  for (uint8 bar = 0; bar < 6; ++bar) {
    term_write(cout, (native_string) "BAR");
    term_write(cout, bar);
    term_write(cout, (native_string) " : ");
    term_writeline(cout);
    term_write(cout, (void *)bars[bar]);
    term_writeline(cout);
  }
#endif

  // Primary
  ide_controller_map[controller_count].enabled = TRUE;
  ide_controller_map[controller_count].id = 0;
  ide_controller_map[controller_count].base_port = bars[0];
  ide_controller_map[controller_count].controller_port = bars[1];
  ide_controller_map[controller_count].bus_master_port = bars[4];
  ide_controller_map[controller_count].serial = !pata;
  ide_controller_map[controller_count].irq =
      (pata ? IDE_PATA_PRIMARY_IRQ : irq_no);

  // Secondary
  ide_controller_map[controller_count + 1].enabled = TRUE;
  ide_controller_map[controller_count + 1].id = 1;
  ide_controller_map[controller_count + 1].base_port = bars[2];
  ide_controller_map[controller_count + 1].controller_port = bars[3];
  ide_controller_map[controller_count + 1].bus_master_port = bars[4];
  ide_controller_map[controller_count + 1].serial = !pata;
  ide_controller_map[controller_count + 1].irq =
      (pata ? IDE_PATA_SECONDARY_IRQ : irq_no);

  controller_count += 2;
}

void ide_detect_controllers() {
  // Scan PCI devices
  for (uint16 bus = 0; bus < PCI_BUS_COUNT; ++bus) {
    for (uint8 device = 0; device < PCI_DEV_PER_BUS; ++device) {
      for (uint8 function = 0; function < PCI_FUNC_PER_DEVICE; ++function) {
        if (pci_device_at(bus, device, function)) {
          uint32 info =
              pci_read_conf(bus, device, function, PCI_HEADER_INFO_OFFSET);
          uint8 class_code = (info >> 24) && 0xFF;
          uint8 subclass = (info >> 16) && 0xFF;
          if (class_code == PCI_CLASS_MASS_STORAGE &&
              subclass == PCI_SUBCLASS_IDE) {
            ide_found_controller(bus, device, function, info);
          }
        }
      }
    }
  }
}

void setup_ide() {
  uint32 i;
  uint32 j;

  for (i = 0; i < IDE_CONTROLLERS; i++) {
    ide_controller_map[i].enabled = FALSE;
  }

  ide_detect_controllers();

  while (1) {
    NOP();
  }

  for (i = 0; i < IDE_CONTROLLERS; i++) {
    if (ide_controller_map[i].enabled) {
      setup_ide_controller(&ide_mod.ide[i], i);
    }
  }

  /* Disk setup */
  /* for (i = 0; i < IDE_CONTROLLERS; i++) */
  /*   for (j = 0; j < IDE_DEVICES_PER_CONTROLLER; j++) */
  /*     if (ide_mod.ide[i].device[j].kind != IDE_DEVICE_ABSENT) { */
  /*       disk *d = disk_alloc(); */
  /*       if (d != NULL) { */
  /*         d->kind = DISK_IDE; */
  /*         d->log2_sector_size = IDE_LOG2_SECTOR_SIZE; */
  /*         d->partition_type = 0; */
  /*         d->partition_path = 0; */
  /*         d->partition_start = 0; */
  /*         d->partition_length = ide_mod.ide[i].device[j].total_sectors; */
  /*         d->_.ide.dev = &ide_mod.ide[i].device[j]; */
  /*       } */
  /*     } */
}

//-----------------------------------------------------------------------------

// Local Variables: //
// mode: C++ //
// End: //
