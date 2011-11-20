/** util.h - misc.
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h> /* uintXX_t */
#include <limits.h> /* INT_MAX, etc */
#include <stdarg.h>
#ifdef _WIN32
#include "wintypes.h"
#endif

#include "perf.h" /* for performance measuring structs */

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define BUG() /**/
#define BUG_ON(_p) do { if (_p) BUG(); } while ( 0 )

#define ASSERT(X) {if (!(X)) { printk("Assertion failed"); }}

/* contants for Inter-Processor Interrupts (IPI) */
enum
{
    MSR_APIC_BASE    = 0x1B,
    APIC_BASE_ENABLE = 0x800,
    APIC_BASE_BSP    = 0x100,
    APIC_ICR_LOW_OFFSET  = 0x300,
    APIC_ICR_DST_ALL_EX  = 0x3 << 18,
    APIC_ICR_LEVEL_EDGE  = 0x0 << 15,
    APIC_ICR_ASSERT      = 0x1 << 14,
    APIC_ICR_PENDING     = 0x1 << 12,
    APIC_ICR_INIT        = 0x5 << 8,
    APIC_ICR_STARTUP     = 0x6 << 8,
};

/**
 * Log stuff
 */
#define LOG_LEVEL_VERBOSE        1
#define LOG_LEVEL_INFORMATION    2
#define LOG_LEVEL_WARNING        3
#define LOG_LEVEL_ERROR          4

#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL 4
#endif

unsigned int slb_base_phys();

/* Routines for execution timing in CPU cycles */
#ifndef _WIN32
#define rdtsc(low,high) \
      __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

static inline uint64_t rdtsc64(void) {
        uint64_t rv;
        __asm__ __volatile__ ("rdtsc" : "=A" (rv));
        return (rv);
}
#else  // _WIN32
#define rdtsc(low,high) _rdtsc(&(low), &(high))

static inline uint64_t rdtsc64(void) {
        __asm {
                ; Flush the pipeline
                XOR eax, eax
                CPUID
                ; Get the RDTSC counter into edx:eax
                RDTSC
        }
}

static inline void _rdtsc(uint32_t* low, uint32_t* high) {
  uint64_t time = rdtsc64();
  *low = (uint32_t) time;
  *high = (uint32_t) (time >> 32);
}
#endif // _WIN32

extern struct slb_perf_vals g_perf; /* actual declaration in util.c */

static inline void start_timer(struct st_timer_vars *vs) {
    rdtsc(vs->startlow, vs->starthigh);
}

static inline void stop_timer(struct st_timer_vars *ve) {
    rdtsc(ve->endlow, ve->endhigh);
}

void update_max(struct st_timer_vars *old, struct st_timer_vars *fresh);
void update_min(struct st_timer_vars *old, struct st_timer_vars *fresh);
void update_sum(unsigned long long *sum, struct st_timer_vars *fresh);

static inline unsigned long ntohl(unsigned long in) {
    unsigned char *s = (unsigned char *)&in;
    return (unsigned long)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}


static inline unsigned short ntohs(unsigned short in) {
    unsigned char *s = (unsigned char *)&in;
    return (unsigned short)(s[0] << 8 | s[1]);
}

/**
 * lowlevel output functions
 */

void slb_outchar(char c);
void slb_outlong(unsigned long val);
void slb_outbyte(unsigned char b);
void slb_serial_init();

#ifndef PERFCRIT

void slb_out_string(const char *value);
void record_timestamp (const char *name);
void log_event (int level, const char *fmt, ...);

#else

#define slb_out_string(value) {}
#define record_timestamp(name) {}
#define log_event(format, arg...) {}

#endif /* PERFCRIT */

#ifndef _WIN32
static inline void
outb(const unsigned short port, unsigned char value)
{
  asm volatile("outb %0,%1" :: "a"(value),"Nd"(port));
}

static inline
unsigned char
inb(const unsigned short port)
{
  unsigned char res;
  asm volatile("inb %1, %0" : "=a"(res): "Nd"(port));
  return res;
}

#else  // _WIN32

static __inline unsigned char inB(unsigned short port)
{
  unsigned char val;

  __asm {
    mov dx, port
    in al, dx
    mov val, al
  }

  return val;
}

static __inline void outB(const unsigned short port, unsigned char value){
  __asm {
    mov al, value
    mov dx, port
    out dx, al
  }
}

#endif // _WIN32



/**
 * Memory-management functions.
 */
extern unsigned int malloc_base_addr;


/**
 * processor-specific functions.
 */
static inline void cpu_relax(void)
{
#ifndef _WIN32
    __asm__ __volatile__ ("pause");
#else  // _WIN32
    __asm { pause }
#endif // _WIN32
}


#endif
