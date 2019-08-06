// file: "main.cpp"

// Copyright (c) 2001 by Marc Feeley and Universit� de Montr�al, All
// Rights Reserved.
//
// Revision History
// 23 Oct 01  initial version (Marc Feeley)

//-----------------------------------------------------------------------------

#include "drivers/filesystem/include/vfs.h"
#include "chrono.h"
#include "disk.h"
#include "general.h"
#include "ps2.h"
#include "rtlib.h"
#include "term.h"
#include "thread.h"

int main() {
  term tty;

  term_init(&tty, 0, 366, 80, 13, &font_mono_5x7, &font_mono_5x7, L"tty", TRUE);
  file_open(NULL, NULL, NULL);


#ifdef MIMOSA_REPL
  term_run(&tty);
#endif

#ifdef GAMBIT_REPL
  {
    native_string file_name = "GSC.EXE";

    file* prog;
    if (NO_ERROR == open_file(file_name, "r", &prog)) {
      term_write(&tty, "\r\n Starting program ");
      term_write(&tty, file_name);
      term_writeline(&tty);

      // TODO:
      // The program thread needs to be aware of what its doing
      uint32 len = prog->length;
      uint8* code = (uint8*)GAMBIT_START;

      error_code err;
      if (ERROR(err = read_file(prog, code, len))) {
        panic(L"ERR");
      }

      term_write(cout, "File loaded. Starting program at: ");
      term_write(cout, code);

      thread_sleep(1000);

      for (int i = 0; i < 5; ++i) {
        term_writeline(cout);
      }

      program_thread* task =
          CAST(program_thread*, kmalloc(sizeof(program_thread)));
      new_program_thread(task, CAST(libc_startup_fn, code), "Gambit");
      thread_start(CAST(thread*, task));

    } else {
      term_write(&tty, "\r\n Failed to open the program.\r\n");
    }
  }
#endif

  do {
    thread_yield();
  } while(1);

  return 0;
}

//-----------------------------------------------------------------------------