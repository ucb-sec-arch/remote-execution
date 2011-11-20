/** util.c - misc.
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

#include "util.h"
#include "params.h" /* for log* stuff */
#include "string.h" /* vscnprintf */
#include "printk.h"
#ifdef _WIN32
#include "io.h"
#endif

#define SERIAL_BASE 0x3f8

struct slb_perf_vals g_perf;

#ifndef TRUSTSIM

extern uint32_t g_phys_base_addr; /* Declared and populated in asm.S */

unsigned int slb_base_phys() {
    return g_phys_base_addr;
}

#endif

static unsigned long long get_runtime(struct st_timer_vars *v) {
    unsigned long long start, end;
    if(!v) { return 0; }
    end = v->endlow & 0x00000000ffffffff;
    end |= ((unsigned long long)v->endhigh) << 32;
    start = v->startlow & 0x00000000ffffffff;
    start |= ((unsigned long long)v->starthigh) << 32;
    return end-start;
}

/* Check if 'fresh' is lower than 'old', potentially resulting in a fresh
 * min. */
void update_min(struct st_timer_vars *old, struct st_timer_vars *fresh) {
    unsigned long long oldr, freshr;
    if(!old || !fresh) { return; }

    oldr = get_runtime(old);
    freshr = get_runtime(fresh);

    /* found a fresh min. !oldr covers uninitialized min */
    if((freshr < oldr) || !oldr) {
        old->startlow = fresh->startlow;
        old->starthigh = fresh->starthigh;
        old->endlow = fresh->endlow;
        old->endhigh = fresh->endhigh;
    }
}

void update_max(struct st_timer_vars *old, struct st_timer_vars *fresh) {
    unsigned long long oldr, freshr;
    if(!old || !fresh) { return; }

    oldr = get_runtime(old);
    freshr = get_runtime(fresh);

    /* found a fresh max */
    if(freshr > oldr) {
        old->startlow = fresh->startlow;
        old->starthigh = fresh->starthigh;
        old->endlow = fresh->endlow;
        old->endhigh = fresh->endhigh;
    }
}

void update_sum(unsigned long long *sum, struct st_timer_vars *fresh) {
    unsigned long long freshr;
    if(!sum || !fresh) { return; }

    freshr = get_runtime(fresh);
    *sum += freshr;
}



#ifndef PERFCRIT

/* output a char string */
void
slb_out_string(const char *value)
{
    for(; (*value) != '\0'; value++)
        slb_outchar(*value);
}

/**
 * Output a string, followed by a newline
 */
void
slb_out_info(const char *msg)
{
    //  out_string(message_label);
  slb_out_string(msg);
  slb_outchar('\r');
  slb_outchar('\n');
}


void record_timestamp (const char *name)
{
    uint64_t timestamp;
    int nameLength;
    char *paramData;

    timestamp = rdtsc64();
    nameLength = strlen(name);
    paramData = pm_reserve(PARAMETER_TYPE_TIMING_INFO, nameLength + sizeof(uint64_t));
    if (paramData == 0) {
        printk("ERROR: Can't append timing parameter\n");
        return;
    }
    memcpy(paramData, &timestamp, sizeof(uint64_t));
    memcpy(paramData + sizeof(uint64_t), name, nameLength);
}

void log_event (int level, const char *fmt, ...)
{
#define MAX_LOG_ENTRY_SIZE  256
    char logEntry[MAX_LOG_ENTRY_SIZE];
    int logEntrySize;
    va_list params;

    if (level < MIN_LOG_LEVEL) {
        return;
    }

    va_start(params, fmt);
#ifndef _WIN32
    vscnprintf(logEntry, MAX_LOG_ENTRY_SIZE, fmt, params);
#else  // _WIN32
    vsnprintf(logEntry, MAX_LOG_ENTRY_SIZE, fmt, params);
#endif // _WIN32
    va_end(params);

    logEntry[MAX_LOG_ENTRY_SIZE - 1] = '\0';
    logEntrySize = strnlen(logEntry, MAX_LOG_ENTRY_SIZE);

    pm_append(PARAMETER_TYPE_LOG_ENTRY, logEntry, logEntrySize);
    printk("%s", logEntry);
}

#endif /* PERFCRIT */

/**
 * These debug functions are redundant with respect to printk()
 * support.  However, they are significantly less complex and can
 * prove to be useful when trying to track down unexpected failures.
 * Thus, they are still here.
 */

/* ------------------------------------------
   output the 32-bit value in %eax to ttyS0
   pay attention to endian-ness
   ------------------------------------------ */
void slb_outlong(unsigned long val) {
    int i;
    char c;

    for(i=3; i>=0; i--) {
        c = (char) (val >> (8*i));
        slb_outbyte(c);
    }

    slb_outchar('\n');
    slb_outchar('\r');
}

/* ------------------------------------------
   output a single byte in %al hex as two 8-bit characters
   ------------------------------------------ */
void slb_outbyte(unsigned char b) {
    char c = '0';

    if((b >> 4) <= 9)
        c = (b >> 4) + '0';
    else
        c = (b >> 4) - 0xa + 'a';

    slb_outchar(c);

    if((b & 0xf) <= 9)
        c = (b & 0xf) + '0';
    else
        c = (b & 0xf) - 0xa + 'a';

    slb_outchar(c);
}


/* ------------------------------------------
   output a character to ttyS0
   ------------------------------------------ */
void slb_outchar(char c) {
/*     if(!serial_initialized) { return; } */

    while (!(inb(SERIAL_BASE+0x5) & 0x20))
        ;
    outb(SERIAL_BASE, c);
}

static unsigned int serial_initialized;

void
slb_serial_init()
{
    serial_initialized = 1;
    // enable DLAB and set baudrate 115200
    outb(SERIAL_BASE+0x3, 0x80);
    outb(SERIAL_BASE+0x0, 0x01);
    outb(SERIAL_BASE+0x1, 0x00);
    // disable DLAB and set 8N1
    outb(SERIAL_BASE+0x3, 0x03);
    // reset IRQ register
    outb(SERIAL_BASE+0x1, 0x00);
    // enable fifo, flush buffer, enable fifo
    outb(SERIAL_BASE+0x2, 0x01);
    outb(SERIAL_BASE+0x2, 0x07);
    outb(SERIAL_BASE+0x2, 0x01);
    // set RTS,DTR
    outb(SERIAL_BASE+0x4, 0x03);
}



