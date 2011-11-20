/* resume.h - common struct defn between PAL and OS code
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

#ifndef _RESUME_H_
#define _RESUME_H_

#include "resumeoffsets.h"

/* image of the saved processor state, common between kmod and PAL */
#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32
struct cpu_state {
    unsigned long eax, ebx, ecx, edx, edi, esi;
    uint16_t cs, ds, es, fs, gs, ss;
    unsigned long efer, eflags, esp, ebp;
    unsigned long cr0, cr2, cr3, cr4;

    /* idt, gdt actually 48 bits each */
    uint16_t gdt_pad;
    uint16_t gdt_limit;
    unsigned long gdt_base;
    uint16_t idt_pad;
    uint16_t idt_limit;
    unsigned long idt_base;
    uint16_t ldt;
    uint16_t tss;
    unsigned long tr;
    unsigned long safety;
    unsigned long return_address;
    /* used for storing timing information across PAL invocations */
    unsigned long startl, starth, endl, endh;
    /* add this to physical base addr to get same addr in kernel virt space */
    unsigned long p2v_offset; 
#ifndef _WIN32
} __attribute__((packed));
#else  // _WIN32
} ;
#pragma pack(pop) // Pop the packed attribute
#endif // _WIN32

typedef struct cpu_state cpu_t;


/* awesome trick from http://www.jaggersoft.com/pubs/CVu11_3.html */
#define COMPILE_TIME_ASSERT(pred)            \
    switch(0){case 0:case pred:;}

#ifndef _WIN32
#define myoffsetof(st, m) __builtin_offsetof(st, m)
#else // _WIN32
#include <stddef.h>
#define myoffsetof(st, m) offsetof(st, m)
#endif // _WIN32

/* It's hackish, but it works.  Please forgive me. */
static inline void compile_time_asserts(void) {
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, cs)             == CPU_STATE_OFFSET_CS);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, ds)             == CPU_STATE_OFFSET_DS);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, ss)             == CPU_STATE_OFFSET_SS);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, cr3)            == CPU_STATE_OFFSET_CR3);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, gdt_limit)      == CPU_STATE_OFFSET_GDT);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, idt_limit)      == CPU_STATE_OFFSET_IDT);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, return_address) == CPU_STATE_OFFSET_RETURN_ADDRESS);
    COMPILE_TIME_ASSERT(myoffsetof(cpu_t, p2v_offset)     == CPU_STATE_OFFSET_P2V_OFFSET);
}

#endif /* _RESUME_H_ */
