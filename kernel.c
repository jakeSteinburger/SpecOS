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

__attribute__((noreturn))
void __stack_chk_fail(void) {
    writestring("\nStack smashing detected. Halting.\n");
    for (;;) asm("cli; hlt");
}

Kernel kernel = {0};

// get stuff from limine so that other kernel modules can use it
__attribute__((used, section(".requests")))
static volatile struct limine_kernel_file_request kernelElfRequest = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0
};

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

__attribute__((used, section(".requests")))
static volatile struct limine_kernel_address_request kernelAddressRequest = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
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
    kernel.kernelFile = *kernelElfRequest.response;
    kernel.kernelAddress = *kernelAddressRequest.response;
}

void _start() {
    initKernelData();
    init_serial();
    initVGA();
    printf("Test binary: %b\n", 0b11001100);
    printf("HHDM: 0x%x\n", kernel.hhdm);
    writeserial("\nStarting physical memory manager...\n");
    initPMM();
    // Just send output to a serial port to test
    writestring("Trying to initialise GDT...\n");
    initGDT();
    writestring("\n\nTrying to initialise IDT & everything related...\n");
    initIDT();
    // this is commented out cos paging doesn't work yet and it's still in progress.
    writeserial("\nInitiating paging...\n");
    uint64_t* pml4Address = initPaging();
    // allocate & map a couple page frames for the new stack
    uint64_t stackStart = 0xFFFFFFFFFFFFF000LL - (2LL*4096);
    allocPages(kernel.pml4, stackStart, KERNEL_PFLAG_PRESENT | KERNEL_PFLAG_WRITE, 2);
    writeserial("Pages mapped, trying to reload cr3 (and change stack pointer)...\n");
    // load a pointer to pml4 into cr3 and change the stack to point elsewhere
    __asm__ volatile (
        "mov %0, %%cr3\r\n"
        "mov %1, %%rsp"
            : : "r" ((uint64_t) pml4Address),
                "r" (0xFFFFFFFFFFFFF000LL)
    );
    writeserial("\nPaging successfully enabled!\n");
    test_userspace();
    for (;;);
}
