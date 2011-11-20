/*
 * perf.h
 * This file contains constants, structs, and enums used to facilitate
 * performance data exchange between the untrusted OS and the PAL.
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

#ifndef __PERF_H__
#define __PERF_H__

struct st_timer_vars {
    unsigned long startlow;
    unsigned long starthigh;
    unsigned long endlow;
    unsigned long endhigh;
};


struct slb_perf_vals {
    struct st_timer_vars seal;
    struct st_timer_vars unseal;
    struct st_timer_vars tpm_seal;
    struct st_timer_vars tpm_unseal;
    struct st_timer_vars tpm_getrand;
    struct st_timer_vars tpm_getrand_min;
    struct st_timer_vars tpm_getrand_max;
    struct st_timer_vars rsa_keygen;
    struct st_timer_vars rsa_encrypt;
    struct st_timer_vars rsa_decrypt;
    struct st_timer_vars primep;
    struct st_timer_vars primeq;
    struct st_timer_vars skinit;
    struct st_timer_vars hashself;
    struct st_timer_vars pcrextend;
    unsigned long long sum_getrand;
    int num_getrand_calls;
    int num_random_byte_calls;
    unsigned long long sum_rsag_malloc;
    unsigned char pcr17[20]; /* for verifying seal is working correctly */
    struct st_timer_vars ctr_create;
    struct st_timer_vars ctr_release;
    struct st_timer_vars ctr_incr;
    struct st_timer_vars ctr_read;
    unsigned int countID;
};


#endif /* __PERF_H__ */
