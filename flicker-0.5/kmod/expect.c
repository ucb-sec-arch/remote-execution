/*
 * expect.c: Pre-compute expected PCR values as useful reference
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
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
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

#include "flicker.h"
#include "log.h"

/**
 * Compute a sha-1 over the SLB; attempting to identically mimic the
 * hashing behavior performed by the TPM with a correctly-executed
 * SKINIT.
 */
void compute_expected_amd(void) {
    uint8_t digest[20];
    uint8_t pcr_preimage[40];
    uint32_t slb_len;

    assert(NULL != g_pal);

    // Upper 16 bits of first 32 bits in the SLB
    slb_len = ((*(uint32_t*) g_pal->pal) >> 16) & 0x0000ffff;

    memset(pcr_preimage, 0, 40);

    sha1(g_pal->pal, slb_len, pcr_preimage+20);

    dump_bytes(pcr_preimage, 40);

    sha1(pcr_preimage, 40, digest);

    dbg("Expected PCR-17 contents:");
    dump_bytes(digest, 20);
}


void compute_expected_intel(void) {
    dbg("Not implemented.");
}

/**
 * The goal here is to predict the contents of the Platform
 * Configuration Register that contains a hash of the PAL. On AMD
 * systems, this is PCR 17.  On Intel systems, this is PCR 18.
 */
void compute_expected_pcrs(void) {
    if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
        compute_expected_intel();
    } else {
        compute_expected_amd();
    }
}
