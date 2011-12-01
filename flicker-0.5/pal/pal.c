/*
 * pal.c: Put code for Flicker PAL here in pal_main()
 *
 * Copyright (C) 2006-2011 Jonathan M. McCune
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 */

#include <stdarg.h>
#include "malloc.h"
#include "printk.h"
#include "params.h"
#include "tpm.h"
#include "sha1.h"
#include "util.h"

/**
 * Special global variables used to convey the size of LOW and HIGH
 * memory regions to facilitate measurement.  Used for
 * hash_trick. This is how we learn where the linker placed various
 * parts of the PAL
 */
extern uint8_t g_pal_zero;
uint8_t g_end_of_low __attribute__((section (".slb.end_of_low"), aligned(4)));
uint8_t g_aligned_end_of_low __attribute__((section (".slb.aligned_end_of_low"), aligned(4096)));
uint8_t g_start_of_high __attribute__((section (".slb.start_of_high"), aligned(4)));
uint8_t g_end_of_high __attribute__((section (".slb.end_of_high"), aligned(4)));

extern uint8_t g_mle_header;
extern uint8_t g_mle_header_end;



/**
 * This next group of functions exist solely for debug reasons.
 * Understanding what they print out is very useful in understanding
 * exactly what is happening when a PAL executes.  None of these are
 * actually necessary in a stable, mature PAL.
 */

/* debug code to dump populated regions of page tables */
void dump_populated_areas(uint8_t *pts) {
    int i;
    uint8_t* ptr;
    int ever_print = 0;
    uint32_t val;
    printk("dump_populated_areas @ %p ENTER\n", pts);
    for(i=0; i<12*1024/4; i++) { /* 12KB of memory to scan, prints 4 bytes/iter. */
        ptr = pts + i*4;
        val = *((uint32_t*)ptr);
        if(0 != val) { /*non-zero*/
            printk("PT addr %p, contents %08x\n", ptr, val);
            ever_print++;
        }
    }
    printk("Printed %d lines of PT\n", ever_print);
    printk("dump_populated_areas EXIT\n");
}

void dump_cpu_t(cpu_t* s) {
    if(s == NULL) { return; }

    printk("eax %08lx, ebx %08lx, ecx %08lx, edx %08lx, edi %08lx, esi %08lx\n",
        s->eax, s->ebx, s->ecx, s->edx, s->edi, s->esi);

    printk("cs %04x, ds %04x, es %04x, fs %04x, gs %04x, ss %04x\n",
        s->cs, s->ds, s->es, s->fs, s->gs, s->ss);

    printk("eflags %08lx, efer %08lx, esp %08lx, ebp %08lx\n",
        s->eflags, s->efer, s->esp, s->ebp);

    printk("cr0 %08lx, cr2 %08lx, cr3 %08lx, cr4 %08lx\n",
        s->cr0, s->cr2, s->cr3, s->cr4);

    printk("gdt_base:gdt_limit %08lx:%04x, idt_base:idt_limit %08lx:%04x\n",
        s->gdt_base, s->gdt_limit, s->idt_base, s->idt_limit);

    printk("ldt %04x, tss %04x, tr %08lx, safety %08lx, return_address %08lx\n",
        s->ldt, s->tss, s->tr, s->safety, s->return_address);

    printk("skinit timer sh:sl eh:el 0x%08lx:0x%08lx 0x%08lx:0x%08lx\n",
        s->starth, s->startl, s->endh, s->endl);

    printk("p2v_offset 0x%08lx\n", s->p2v_offset);
}

void get_pcr17(void) {
    unsigned char v[SHA_DIGEST_LENGTH];
    int result;
    int locality = 2;

    result = tpm_pcr_read(locality, 17, (tpm_pcr_value_t*)v);
    if(TPM_SUCCESS != result) {
        log_event(LOG_LEVEL_VERBOSE, "ERROR: Failed to read PCR-17");
    }
    log_event(LOG_LEVEL_VERBOSE,
              "PCR-17: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
              "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
              v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9],
              v[10], v[11], v[12], v[13], v[14], v[15], v[16], v[17], v[18], v[19]);
}

void gdt_debug(void) {
    uint32_t base;
    uint8_t * gdt_ptr;

    base = slb_base_phys();

    printk("slb_base_phys(): 0x%08x\n", base);
    printk("GDT:\n");

    gdt_ptr = (uint8_t*)52; /* 4-byte AMD header + 44-byte Intel header; hackish */

    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;

    printk("TSS:\n");
    dump_bytes(gdt_ptr, 4); gdt_ptr += 4;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;
    dump_bytes(gdt_ptr, 8); gdt_ptr += 8;

    printk("SLB/MLE size: %08x\n", *((unsigned short*)(2))); /* grabs from SLB header */
}


/**
 * Code below here is in section .text.slb.  It is part of the actual
 * SLB / MLE that is measured directly by SKINIT /
 * GETSEC[SENTER]. Code not in section .text.slb is measured in
 * software by function hash_trick().
 */

#define FIB /* Fibonacci program */

int pal_main(void) __attribute__ ((section (".text.slb")));
int pal_main(void)
{
    printk("Hello from pal_main()\n");

#ifdef FIB
    int i = 0;
    int* test;
    int len = pm_get_addr(1, (char**)&test);
    int arr[test[0]];

    printk("At %s num fib: %d len: %d\n", __func__, test[0], len);

    arr[0] = 1;
    arr[1] = 1;

    for (i = 2; i < test[0];i++ ) {
        arr[i] = arr[i-1] + arr[i-2];
    }

    printk("Fibonacci: ");
    for (i = 0;i < test[0];i++) {
        printk("%d ", arr[i]);
    }
    printk("\n");
#endif /* FIB */

    return 0;
}


/* The "hash trick" measures and then extends a PCR with the
 * measurement of the "upper" portion of the PAL.  When the PAL is too
 * large to be measured directly by SKINIT / GETSEC[SENTER], this
 * architecture becomes necessary.
 */
void hash_trick(uint8_t* high_region_start, uint32_t high_region_size) __attribute__ ((section (".text.slb")));
void hash_trick(uint8_t* high_region_start, uint32_t high_region_size) {
    SHA1_CTX ctx;
    uint8_t high_hash[SHA_DIGEST_LENGTH];

    int result;
    int locality = 2;

    printk("hash_trick: Hashing %d bytes at address 0x%08x\n",
           high_region_size, (uint32_t)high_region_start);

    record_timestamp("hash_trick hash - begin");
    SHA1_init(&ctx);
    SHA1_update(&ctx, high_region_start, high_region_size);
    SHA1_final(&ctx);
    SHA1_digest(&ctx, high_hash);
    record_timestamp("hash_trick hash - end");
    dump_bytes(high_hash, SHA_DIGEST_LENGTH);

    /* Now extend this measurement into PCR 19 (locality 2 PCR) */
    record_timestamp("hash_trick extend - begin");
    result = tpm_pcr_extend(locality, 19, (tpm_digest_t*)high_hash, NULL);
    record_timestamp("hash_trick extend - end");

    if (result != TPM_SUCCESS) {
       printk("Failed to extend measurement PCR (error %d)\n", result);
    }
    else {
        log_event(LOG_LEVEL_VERBOSE, "hash_trick: Successfully extended measurement PCR 19 with high_hash.\n");
    }
}

int pal_init (void *parambase) __attribute__ ((section (".text.slb")));
int pal_init (void *parambase)
{
    int param_result;
    void* inputBuffer = NULL;

    // Initialize malloc
    static_malloc_init();

    // Setup the parameters - Dependent on layout of 'struct pal_descriptor'
    // in ../kmod/flicker.h
    inputBuffer = parambase + sizeof(cpu_t);
    param_result = pm_init(inputBuffer,
                           MAX_INPUT_PARAM_SIZE,
                           inputBuffer + MAX_INPUT_PARAM_SIZE,
                           MAX_OUTPUT_PARAM_SIZE);

    if (!param_result) {
        printk("ERROR - Failed to initialize the parameters\n");
    }

#ifndef PERF_CRIT
    gdt_debug();
#endif

    log_event(LOG_LEVEL_VERBOSE, "&g_pal_zero           = 0x%08x\n", (uint32_t)&g_pal_zero);
    log_event(LOG_LEVEL_VERBOSE, "&g_end_of_low         = 0x%08x\n", (uint32_t)&g_end_of_low);
    log_event(LOG_LEVEL_VERBOSE, "&g_aligned_end_of_low = 0x%08x\n", (uint32_t)&g_aligned_end_of_low);
    log_event(LOG_LEVEL_VERBOSE, "&g_start_of_high      = 0x%08x\n", (uint32_t)&g_start_of_high);
    log_event(LOG_LEVEL_VERBOSE, "&g_end_of_high        = 0x%08x\n", (uint32_t)&g_end_of_high);

    log_event(LOG_LEVEL_VERBOSE, "&g_mle_header         = 0x%08x\n", (uint32_t)&g_mle_header);
    log_event(LOG_LEVEL_VERBOSE, "&g_mle_header_end     = 0x%08x\n", (uint32_t)&g_mle_header_end);

    log_event(LOG_LEVEL_VERBOSE, "uuid0                 = 0x%08x\n", *((uint32_t*)(&g_mle_header   )));
    log_event(LOG_LEVEL_VERBOSE, "uuid1                 = 0x%08x\n", *((uint32_t*)(&g_mle_header+4 )));
    log_event(LOG_LEVEL_VERBOSE, "uuid2                 = 0x%08x\n", *((uint32_t*)(&g_mle_header+8 )));
    log_event(LOG_LEVEL_VERBOSE, "uuid3                 = 0x%08x\n", *((uint32_t*)(&g_mle_header+12)));
    log_event(LOG_LEVEL_VERBOSE, "HeaderLen             = 0x%08x\n", *((uint32_t*)(&g_mle_header+16)));
    log_event(LOG_LEVEL_VERBOSE, "Version               = 0x%08x\n", *((uint32_t*)(&g_mle_header+20)));
    log_event(LOG_LEVEL_VERBOSE, "EntryPoint            = 0x%08x\n", *((uint32_t*)(&g_mle_header+24)));
    log_event(LOG_LEVEL_VERBOSE, "FirstValidPage        = 0x%08x\n", *((uint32_t*)(&g_mle_header+28)));
    log_event(LOG_LEVEL_VERBOSE, "MleStart              = 0x%08x\n", *((uint32_t*)(&g_mle_header+32)));
    log_event(LOG_LEVEL_VERBOSE, "MleEnd                = 0x%08x\n", *((uint32_t*)(&g_mle_header+36)));
    log_event(LOG_LEVEL_VERBOSE, "Capabilities          = 0x%08x\n", *((uint32_t*)(&g_mle_header+40)));


    hash_trick((uint8_t*)&g_start_of_high,
               (uint32_t)&g_end_of_high - (uint32_t)&g_start_of_high);

    log_event(LOG_LEVEL_VERBOSE, "Successfully initialized PAL\n");

    return 0;
}

/* Note: This function Assumes a 4KB stack.  A more elegant solution
 * would probably define some symbols and let the linker script
 * determine the stack size.
 */
void zero_stack (void) __attribute__ ((section (".text.slb")));
void zero_stack (void) {
    uint32_t esp;
    uint32_t stack_base;
    int ptr;

    __asm__ __volatile__("movl %%esp, %0 "
                         : "=m" (esp) );

    printk("esp: 0x%08x\n", esp);

    stack_base = (0xFFFFFFFF << 12) & esp;  // 2^12 = 4k

    if (stack_base <= 0) {
      printk("Stack base too low!  Not clearing it.\n");
      return;
    }

    // Zero out the stack 4 bytes at a time
    for (ptr = stack_base; ptr < esp; ptr+=4) {
      *((long*) ptr) = 0;
    }

    // Make sure we get the 0-3 bytes that may remain unzeroed
    for (ptr = ptr - 4; ptr < esp; ptr++) {
      *((char*) ptr) = 0;
    }
}

/* CPU vendor-specific magic numbers. Must be consistent with asm.S */
#define CPU_VENDOR_INTEL     0xAB
#define CPU_VENDOR_AMD     	0xCD

#define AMD_STRING_DWORD1 0x68747541
#define AMD_STRING_DWORD2 0x69746E65
#define AMD_STRING_DWORD3 0x444D4163

#define INTEL_STRING_DWORD1    0x756E6547
#define INTEL_STRING_DWORD2    0x49656E69
#define INTEL_STRING_DWORD3    0x6C65746E

#define cpuid(op, eax, ebx, ecx, edx)           \
({                                              \
  __asm__ __volatile__("cpuid"                          \
          :"=a"(*(eax)), "=b"(*(ebx)), "=c"(*(ecx)), "=d"(*(edx))       \
          :"0"(op), "2" (0));                   \
})

/* called from asm.S */
uint32_t get_cpu_vendor(void) __attribute__ ((section (".text.slb")));
uint32_t get_cpu_vendor(void) {
    uint32_t dummy;
    uint32_t vendor_dword1, vendor_dword2, vendor_dword3;

    cpuid(0, &dummy, &vendor_dword1, &vendor_dword3, &vendor_dword2);
    if(vendor_dword1 == AMD_STRING_DWORD1 && vendor_dword2 == AMD_STRING_DWORD2
       && vendor_dword3 == AMD_STRING_DWORD3)
        return CPU_VENDOR_AMD;
    else if(vendor_dword1 == INTEL_STRING_DWORD1 && vendor_dword2 == INTEL_STRING_DWORD2
            && vendor_dword3 == INTEL_STRING_DWORD3)
        return CPU_VENDOR_INTEL;
    else
        __asm__ __volatile__("1: jmp 1b\n":); /* HALT */

    return 0; /* never reached */
}

int cap_measurement_pcr(void)  __attribute__ ((section (".text.slb")));
int cap_measurement_pcr(void)
{
    unsigned char zeroHash[SHA_DIGEST_LENGTH];
    int result = 0;
    int locality = 2;
    int i;

    for(i=0; i<SHA_DIGEST_LENGTH; i++) zeroHash[i] = 0; /* no memset in .text.slb */
    result = tpm_pcr_extend(locality, 18, (tpm_digest_t*)zeroHash, NULL);

    if (TPM_SUCCESS != result) {
       printk("Failed to extend measurement PCR (error %d)\n", result);
    }
    else {
        printk("Successfully extended measurement PCR with zero.\n");
    }
    return result;
}


int slb_dowork(uint32_t params) __attribute__ ((section (".text.slb")));
int slb_dowork(uint32_t params) {
    //int ret, err;
    //int *command;
    unsigned int base = slb_base_phys();
    int locality = 2;
    void *resume_pts = NULL;

    slb_out_string("entered slb_dowork");

    printk("entered slb_dowork(), base @ 0x%08x, params @ 0x%08x\n",
           base, (unsigned int)params);
    base++;

    dump_cpu_t((cpu_t*)params);

    /* Init TPM */

    //dump_locality_access_regs();
    deactivate_all_localities();
    //dump_locality_access_regs();

    if(TPM_SUCCESS == tpm_wait_cmd_ready(locality)) {
        printk("TPM successfully opened in Locality %d.\n", locality);
        if(is_tpm_ready(locality)) {
            printk("TPM confirmed ready at locality %d.\n", locality);
        } else {
            printk("TPM: FAILED to open TPM locality %d\n", locality);
            /* return false; */ // TODO: Fail gracefully.
        }
    } else {
        printk("TPM ERROR: Locality %d could not be opened.\n", locality);
        /* return false; */ // TODO: Fail gracefully.
    }

    pal_init((void *)params);

    //log_skinit_time((cpu_t*)params); /* TODO: re-implement DRTM timing */

#ifndef PERFCRIT
    get_pcr17();
#endif

    /* Make sure we get the right address for the resume page tables */
    resume_pts = (void*)params + sizeof(cpu_t) +
        MAX_INPUT_PARAM_SIZE + MAX_OUTPUT_PARAM_SIZE;
    log_event(LOG_LEVEL_VERBOSE, "resume_pts: 0x%p\n", resume_pts);

    if(((uint32_t)resume_pts & 0xfffff000) != (uint32_t)resume_pts) {
        log_event(LOG_LEVEL_ERROR, "resume_pts NOT 4KB-ALIGNED; "
                  "we will likely die trying to resume: 0x%p\n", resume_pts);
    }

    //dump_populated_areas(resume_pts);

    pal_main();

    //record_timestamp("cap pcr - begin");
    cap_measurement_pcr();
    //record_timestamp("cap pcr - end");

    /* Deactivate all localities so that the legacy OS can regain
     * access at the desired locality. */
    //record_timestamp("deactivate - begin");
    deactivate_all_localities(); // new tboot TPM code
    //record_timestamp("deactivate - end");

    //record_timestamp("zero - begin");
    zero_stack();
    //record_timestamp("zero - end");

    return 0;
}

