/*
 * printk.c:  printk() output fn and helpers
 *
 * Copyright (c) 2006-2010, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * printk.c: Modified for Flicker. NOTE: mutex removed.  NOT
 * multicore-safe.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "string.h"
#include "printk.h"
#include "com.h"

#ifndef PERFCRIT

void printk_init(void)
{
    serial_init();
}

#define WRITE_LOGS(s, n) serial_write(s, n)

void printk(const char *fmt, ...)
{
    char buf[256];
    int n;
    va_list ap;
    static bool last_line_cr = true;

    memset(buf, '\0', sizeof(buf));
    va_start(ap, fmt);
    n = vscnprintf(buf, sizeof(buf), fmt, ap);
    //mtx_enter(&print_lock);
    /* prepend "FLI: " if the last line that was printed ended with a '\n' */
    if ( last_line_cr ) {
        WRITE_LOGS("FLI: ", 8);
    }

    last_line_cr = (n > 0 && buf[n-1] == '\n');
    WRITE_LOGS(buf, n);
    //mtx_leave(&print_lock);
    va_end(ap);
}


void dump_bytes(unsigned char *bytes, int len)
{
    int i;
    if(!bytes) return;

    for (i=0; i<len; i++) {
        printk("%02x", bytes[i]);
        if(i>0 && !((i+1)%16)) {
            printk("\n");
        } else {
            printk(" ");
        }
    }
    if(len%16) {
        printk("\n");
    }
}

/*
 * if 'prefix' != NULL, print it before each line of hex string
 */
void print_hex(const char *prefix, const void *prtptr, size_t size)
{
    size_t i;
    for ( i = 0; i < size; i++ ) {
        if ( i % 16 == 0 && prefix != NULL )
            printk("\n%s", prefix);
        printk("%02x ", *(const uint8_t *)prtptr++);
    }
    printk("\n");
}

#endif /* PERFCRIT */
