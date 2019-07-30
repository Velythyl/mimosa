#include "fat32.h"
#include "fs.h"
#include "disk.h"
#include "general.h"
#include "rtlib.h"

error_code __attribute__((optimize("O0")))
fat_32_create_empty_file(file_system* fs, native_string name, native_string ext,
                         file** result) {
  FAT_directory_entry de;
  error_code err;
  native_char normalized_path[NAME_MAX + 1];
  native_string p = normalized_path;
  file* f;

  if (ERROR(err = open_root_dir(fs, &f))) {
#ifdef SHOW_DISK_INFO
    term_write(cout, "Error loading the root dir: ");
    term_write(cout, err);
    term_writeline(cout);
#endif
    return err;
  }

  uint32 entry_cluster = f->current_cluster;
  // Section start is the LBA
  uint32 entry_section_start = f->current_section_start;
  uint32 entry_section_pos = f->current_section_pos;
  uint32 entry_pos = f->current_pos;
  uint32 section_length = f->current_section_length;

  term_write(cout, "The current root dir section start is:");
  term_write(cout, entry_section_start);

  while ((err = read_file(f, &de, sizeof(de))) == sizeof(de)) {
    // This means the entry is available
    if (de.DIR_Name[0] == 0) break;
    // Update the position with the information before
    entry_cluster = f->current_cluster;
    entry_section_start = f->current_section_start;
    entry_section_pos = f->current_section_pos;
    entry_pos = f->current_pos;
    section_length = f->current_section_length;
  }

  // Set the file to the last position so we can easily write there
  // It is also the position of the directory entry so we set that at 
  // the same time
  f->entry.cluster = f->current_cluster = entry_cluster;
  f->entry.section_start = f->current_section_start = entry_section_start;
  f->entry.section_pos = f->current_section_pos = entry_section_pos;
  f->entry.current_pos = f->current_pos = entry_pos;
  f->entry.section_length = f->current_section_length = section_length;

  // We got a position for the root entry, we need to find an available FAT
  uint32 cluster = FAT32_FIRST_CLUSTER;

  if (ERROR(err = fat_32_find_first_empty_cluster(fs, &cluster))) {
    return err;
  }

  // Fill with spaces
  for (int i = 0; i < FAT_NAME_LENGTH; ++i) {
    de.DIR_Name[i] = ' ';
  }

  // TODO get the right length to avoid an overrun

  // Copy the name
  memcpy(de.DIR_Name, name, 7);
  memcpy(de.DIR_Name + 8, ext, 3);

  {  // Set the cluster in the descriptor
    uint16 cluster_hi = (cluster & 0xFFFF0000) >> 16;
    uint16 cluster_lo = (cluster & 0x0000FFFF);

    for (int i = 0; i < 2; ++i) {
      de.DIR_FstClusHI[i] = as_uint8(cluster_hi, i);
      de.DIR_FstClusLO[i] = as_uint8(cluster_lo, i);
    }
  }

  if(ERROR(err = write_file(f, &de, sizeof(de), TRUE))) {
    return err;
  }

  if(ERROR(err = fat_32_set_fat_link_value(fs, cluster, FAT_32_EOF))) {
    return err;
  }

  // Correctly set to the right coordinates in the FAT
  // so we are at the beginning of the file
  f->current_cluster = cluster;
  f->current_section_length = 1 << (fs->_.FAT121632.log2_bps + fs->_.FAT121632.log2_spc);
  f->current_section_start = ((cluster - 2) << fs->_.FAT121632.log2_spc) + fs->_.FAT121632.first_data_sector;
  f->current_section_pos = 0;
  f->current_pos = 0;
  f->length = 0;
  f->mode = S_IFREG;

  *result = f;

  return err;
}

error_code __attribute__((optimize("O0"))) fat_32_open_root_dir(file_system* fs, file* f) {
#ifdef SHOW_DISK_INFO
  term_write(cout, "Loading FAT32 root dir\n\r");
#endif

  BIOS_Parameter_Block* p;
  cache_block* cb;
  disk* d = fs->_.FAT121632.d;
  error_code err;

  if (ERROR(err = disk_cache_block_acquire(d, 0, &cb))) return err;

  p = CAST(BIOS_Parameter_Block*, cb->buf);

  if(ERROR(err = disk_cache_block_release(cb))) return err;

  // debug_write("In open root dir, the FS kind is: ");
  // debug_write(fs->kind);

  f->fs = fs;
  f->current_cluster = as_uint32(p->_.FAT32.BPB_RootClus);

#ifdef SHOW_DISK_INFO
  term_write(cout, "Root cluster is: ");
  term_write(cout, as_uint32(p->_.FAT32.BPB_RootClus));
  term_writeline(cout);
#endif

  f->current_section_start = fs->_.FAT121632.first_data_sector;

  // Length is not there
  f->current_section_length = as_uint16(p->BPB_BytsPerSec) * p->BPB_SecPerClus;

#ifdef SHOW_DISK_INFO
  term_write(cout, "FAT32 ROOT DIR [sector] start=");
  term_write(cout, fs->_.FAT121632.first_data_sector);
#endif
  f->current_section_pos = 0;
  f->current_pos = 0;
  // Since the FAT32 root directory has no fixed size, we don't specify a length
  // (it would be slow to calculate it everytime...). On a directory, the length
  // is not used anyways when reading the file. We simply read until EOF.
  f->length = 0; 
  f->mode = S_IFDIR;
}

error_code fat_32_find_first_empty_cluster(file_system* fs, uint32* result) {
  error_code err = NO_ERROR;
  BIOS_Parameter_Block* p;
  uint32 offset = 0;
  disk* d = fs->_.FAT121632.d;
  cache_block* cb;

  // Fetch the BPB
  if (ERROR(err = disk_cache_block_acquire(d, 0, &cb))) return err;
  p = CAST(BIOS_Parameter_Block*, cb->buf);
  if (ERROR(err = disk_cache_block_release(cb))) return err;

  uint16 entries_per_sector = (1 << fs->_.FAT121632.log2_bps) >> 2;

  uint32 lba = fs->_.FAT121632.reserved_sectors;
  uint32 max_lba = as_uint32(p->_.FAT32.BPB_FATSz32) + fs->_.FAT121632.reserved_sectors;

  bool found = FALSE;
  // Inspect all sectors starting from *cluster
  // It is faster to use this method than repeated calls to
  // fat_32_get_fat_link_value since the latter will get a 
  // cache block per request.
  uint32 clus;
  uint16 i;
  clus = i = 2;
  // We start from the first cluster because we have no clue where data has been overridden
  while ((lba < max_lba) && !found) {
    if (ERROR(err = disk_cache_block_acquire(fs->_.FAT121632.d, lba, &cb))) {
      return err;
    }

    for (; i < entries_per_sector; ++i, ++clus) {
      uint32 entry = CAST(uint32*, cb->buf)[i];
      if(found = (entry == 0)) {
        break;
      }
    }

    if (ERROR(err = disk_cache_block_release(cb))) return err;
    ++lba;
    i = 0;
  }

  // Could not find an entry: disk out of space
  if (!found) err = DISK_OUT_OF_SPACE;
  *result = clus;
  return err;
}

error_code fat_32_get_fat_link_value(file_system* fs, uint32 cluster, uint32* value) {
  BIOS_Parameter_Block* p;
  disk* d = fs->_.FAT121632.d;
  error_code err;
  cache_block* cb;
  // entries per sector = Bytes Per sector / 4 (an entry is 4 bytes)
  uint16 entries_per_sector = (1 << fs->_.FAT121632.log2_bps) >> 2;

  if (cluster < 2) {
    fatal_error("Cannot inspect lower than the second cluster entry");
  }

  if (ERROR(err = disk_cache_block_acquire(d, 0, &cb))) return err;
  p = CAST(BIOS_Parameter_Block*, cb->buf);
  if (ERROR(err = disk_cache_block_release(cb))) return err;

  uint32 lba = (cluster / entries_per_sector) + as_uint16(p->BPB_RsvdSecCnt);
  uint32 offset = cluster % entries_per_sector;

  if (ERROR(err = disk_cache_block_acquire(d, lba, &cb))) return err;
  *value= *(CAST(uint32*, cb->buf) + offset);
  if (ERROR(err = disk_cache_block_release(cb))) return err;

  return err;
}

error_code fat_32_set_fat_link_value(file_system* fs, uint32 cluster, uint32 value) {
  cache_block* cb;
  disk* d = fs->_.FAT121632.d;
  ide_device* dev = d->_.ide.dev;
  uint16 entries_per_sector = (1 << fs->_.FAT121632.log2_bps) >> 2;
  error_code err;
  BIOS_Parameter_Block* p;
  uint8 wrt_buff[4];

  if (cluster < 2) {
    fatal_error("Cannot inspect lower than the second cluster entry");
  }

  if (ERROR(err = disk_cache_block_acquire(d, 0, &cb))) return err;
  p = CAST(BIOS_Parameter_Block*, cb->buf);
  if (ERROR(err = disk_cache_block_release(cb))) return err;

  uint32 lba = (cluster / entries_per_sector) + as_uint16(p->BPB_RsvdSecCnt);
  uint32 offset_in_bytes = (cluster % entries_per_sector) << 2;

  // Read the cache in order to update it
  if (ERROR(err = disk_cache_block_acquire(d, lba, &cb))) return err;

  for(int i = 0; i < 4; ++i)
    cb->buf[i + offset_in_bytes] = wrt_buff[i] = as_uint8(value, i);

  ide_write(dev, lba, offset_in_bytes, sizeof(uint32), wrt_buff);

  if(ERROR(err = disk_cache_block_release(cb))) return err;
  
  return err;
}
