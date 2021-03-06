<img style="float:left" src="res/logo.png" width="80" >
<h1 style="float:right; display:inline-block">Mimosa</h1>

<div style="clear:both"></div>

<img src="res/mimosa-screenshot.png" width="600">

# Introduction

The Mimosa operating system consists of a minimal kernel built on C++ and Scheme. It contains a Scheme implementation of a hard drive (ATA) driver, keyboard (PS2), serial (8250 UART), FAT32 filesystem and a small real time clock manager. The project was built to experiment with developement of operating system using a high level functional language to study the developement process and the use of Scheme to build a fairly complex system.

# Organisation

The projet is divided in the following folder structure:

- `scheme` contains the Scheme sources for the drivers, utilitary functions and the `gambini.scm` file that loads up the Gambit system
  - The `interpreted` folder contains Scheme sources that are meant to be intepreted by the runtime when the system runs
  - The `compiled` folder contains Scheme sources that are meant to be compiled and executed as a compiled Scheme module
- `archive-items` contains files folders that are placed into the built archive
- `attic` contains deprected files kept for comparison or quick-access
- `drivers` contains C++ drivers
- `fonts` contains the system fonts
- `include` contains header files
- `libc` contain an implementation of the C standard library provided to the Gambit runtime
- `res` contains external ressources (like images included to this repo)
- `utils` contain small tools built to aid developement
- The root folder contains various kernel files and build system files.

# Required tooling

Currently, Mimosa builds with GCC 9.2.1. You will need the 32 bit tools in order to build the system (`g++-multilib`) as well as GNU make. Right now, the system only builds on Linux. If you want to run the OS on an emulator, the makefile provides support for `qemu`, in particular `qemu-system-i386`.

# Quick-install and run instructions

Assuming you have the correct compiler setup, the following steps will
build and run mimosa:
    
```Shell
    make clean
    make single-archive # creates a booting archive in floppy.img
    make run  # requires qemu
```

Multiple debugging `make` commands are also available:
- `make debug` waits for GDB connection on port :1234
- `make run-with-serial` passes through a serial port from the VM to the host on port 44555. It can be used in conjonction with the `telnet` or `netcat` utility to control a REPL from your host system.

The createimg.sh script is used to create a FAT32 image that can be mounted and add necessary Scheme driver files to the archive. However, the folder `archive-items` will be entirely replicated on the image and so you can add other files to be accessible at boot.

You will need to have a special Gambit program compiled for the Mimosa operating system. See the next section for instructions on how to prepare the Gambit executable.

## Compiled v.s interpreted Scheme drivers

Mimosa provides support for both interpreted and compiled Scheme drivers and a mix and match as you desired. The `scheme` folder is divided in two subfolders, where files can be moved, to determine wether the drivers (or sources of various types) are to be compiled or interpreted. 

Compiled files are loaded as Gambit-provided modules and thus a new compiled version of the Gambit runtime is required everytime a change to the method of execution of a file is changed. The `./build-and-copy-gambit-to-vm.sh` script will correctly build the runtime. Creating the runtime can take some time, as it requires building Gambit, compiling the Scheme sources to file with the built Gambit, and then rebuild (completely) the Gambit runtime, this time with the compiled `C` sources as part of the runtime. 

Including a compiled file is done differently than an interpreted one. 
- Inclusion of an interpreted file is done through `(import (my-lib))`
- Inclusion of a compiled file is done with `(##load-module 'my-lib)`

You also **need** to execute `make clean` before starting Mimosa, as otherwise you **will** end up with duplicated drivers.

# Dependencies

The kernel requires a compatible Gambit runtime. Currently, the modified Gambit runtime is located [here](https://github.com/SamuelYvon/gambit). In order to build a compatible runtime, you will need the Ubuntu VM provided in [this repository](https://github.com/udem-dlteam/ubuntu-6). Once running, you can execute the following command to create a working Gambit environement.

```Shell
./build-and-copy-gambit-to-vm.sh build
```

This will automatically place the built executable in the right folder, so a compile cycle will then execute properly.

## `modifiedgambit.h`

This header file is issued from the `include/gambit.h.in` from the custom Gambit implementation. It will require an update if the header file from the Gambit fork changes.

# Developement

The project has seen many codestyles and so some parts of the code are not formatted the same. The C++ code tries to use the "llvm" format (mainly in driver and kernel files), using the `clang-format` utility. The Scheme code does not follow a style that has a name that I know of. 

# Papers & Presentations

- In 2019, Mimosa was presented at the "Gambit at 30" event. You will find related presentations [here](https://github.com/gambit/gambit-at-30)
- In 2020, we presented the Mimosa system in the context of running Scheme on bare-metal at the 2020 Scheme workshop. You will find
  the associated article [here](https://icfp20.sigplan.org/details/scheme-2020-papers/3/Running-Scheme-On-Bare-Metal-Experience-Report-)

