/* 64 bit version of the SpecOS kernel.
 * Copyright (C) 2024 Jake Steinburger under the MIT license. See the GitHub repository for more information.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "limine.h"

#include "drivers/include/serial.h"
#include "drivers/include/vga.h"
#include "sys/include/gdt.h"
#include "sys/include/idt.h"
#include "drivers/include/keyboard.h"
#include "drivers/include/rtc.h"
#include "drivers/include/disk.h"
#include "utils/include/string.h"
#include "include/shell.h"
#include "utils/include/printf.h"
#include "mem/include/pmm.h"
#include "limine.h"
#include "mem/include/paging.h"
#include "sys/include/panic.h"
#include "include/kernel.h"

Kernel kernel = {0};

// get stuff from limine so that other kernel modules can use it
__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmapRequest = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdmRequest = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

void initKernelData() {
    // init info for the terminal & stdio
    kernel.colourOut = 0xFFFFFF;
    kernel.doPush = true;
    kernel.chX = 5;
    kernel.chY = 5;
    // bootloader information
    kernel.hhdm = (hhdmRequest.response)->offset;
    struct limine_memmap_response memmapResponse = *memmapRequest.response;
    kernel.memmapEntryCount = memmapResponse.entry_count;
    kernel.memmapEntries = memmapResponse.entries;
}

void _start() {
    initKernelData();
    init_serial();
    initVGA(); 
    // Just send output to a serial port to test
    writestring("Trying to initialise GDT...\n");
    initGDT();
    writestring("\nGDT successfully initialised! (as far as can be told. All I know is that there isn't a gpf.)");
    writestring("\n\nTrying to initialise IDT & everything related...\n");
    initIDT();
    writestring("\nStarting physical memory manager...");
    initPMM();
    // this is commented out cos paging doesn't work yet and it's still in progress.
    //writestring("\nInitiating paging...");
    //initPaging();
    char buffer[10];
    uint64_t tableSingle;
    for (int i = 0; i < 5; i++) {
        for (int y = 0; y < 10; y++)
            buffer[y] = 0;
        tableSingle = *(uint64_t*)&(kernel.GDT)[i];
        uint64_to_hex_string((uint64_t)tableSingle, buffer);
        writestring("\nGDT contents: 0x");
        writestring(buffer);
    }
    test_userspace();
    for (;;);
}
