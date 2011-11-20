/*
 * mtrrs.h: Intel(r) TXT MTRR-related definitions
 *
 * Copyright (c) 2003-2007, Intel Corporation
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
 *
 */

/*
 *  mtrrs.h: Modified for Flicker
 */

#ifndef __TXT_MTRRS_H__
#define __TXT_MTRRS_H__

#ifndef _WIN32
#include <asm/mtrr.h>
#else  // _WIN32
#include "wintypes.h"

#define MSR_MTRRdefType MSR_IA32_MTRR_DEF_TYPE
#define MSR_MTRRcap MSR_IA32_MTRRCAP

/*  These are the region types  */
 #define MTRR_TYPE_UNCACHABLE 0
 #define MTRR_TYPE_WRCOMB     1
 #define MTRR_TYPE_WRTHROUGH  4
 #define MTRR_TYPE_WRPROT     5
 #define MTRR_TYPE_WRBACK     6
 #define MTRR_NUM_TYPES       7

#endif // _WIN32


#include "acmod.h"

#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32

enum fix_mtrr_t {
    MTRR_FIX64K_00000 = 0x250,
    MTRR_FIX16K_80000 = 0x258,
    MTRR_FIX16K_A0000 = 0x259,
    MTRR_FIX4K_C0000  = 0x268,
    MTRR_FIX4K_C8000  = 0x269,
    MTRR_FIX4K_D0000  = 0x26A,
    MTRR_FIX4K_D8000  = 0x26B,
    MTRR_FIX4K_E0000  = 0x26C,
    MTRR_FIX4K_E8000  = 0x26D,
    MTRR_FIX4K_F0000  = 0x26E,
    MTRR_FIX4K_F8000  = 0x26F
};

typedef union {
    uint64_t raw;
    uint8_t  type[8];
} mtrr_fix_types_t;

enum var_mtrr_t {
    MTRR_PHYS_BASE0_MSR = 0x200,
    MTRR_PHYS_MASK0_MSR = 0x201,
    MTRR_PHYS_BASE1_MSR = 0x202,
    MTRR_PHYS_MASK1_MSR = 0x203,
    MTRR_PHYS_BASE2_MSR = 0x204,
    MTRR_PHYS_MASK2_MSR = 0x205,
    MTRR_PHYS_BASE3_MSR = 0x206,
    MTRR_PHYS_MASK3_MSR = 0x207,
    MTRR_PHYS_BASE4_MSR = 0x208,
    MTRR_PHYS_MASK4_MSR = 0x209,
    MTRR_PHYS_BASE5_MSR = 0x20A,
    MTRR_PHYS_MASK5_MSR = 0x20B,
    MTRR_PHYS_BASE6_MSR = 0x20C,
    MTRR_PHYS_MASK6_MSR = 0x20D,
    MTRR_PHYS_BASE7_MSR = 0x20E,
    MTRR_PHYS_MASK7_MSR = 0x20F
};

typedef union {
    uint64_t    raw;
    struct {
        uint64_t vcnt        : 8;    /* num variable MTRR pairs */
        uint64_t fix         : 1;    /* fixed range MTRRs are supported */
        uint64_t reserved1   : 1;
        uint64_t wc          : 1;    /* write-combining mem type supported */
        uint64_t reserved2   : 53;
    };
} mtrr_cap_t;

typedef union {
    uint64_t    raw;
    struct {
        uint64_t type        : 8;
        uint64_t reserved1   : 2;
        uint64_t fe          : 1;    /* fixed MTRR enable */
        uint64_t e           : 1;    /* (all) MTRR enable */
        uint64_t reserved2   : 52;
    };
} mtrr_def_type_t;

typedef union {
    uint64_t    raw;
    struct {
        uint64_t type      : 8;
        uint64_t reserved1 : 4;
        uint64_t base      : 24;
        uint64_t reserved2 : 28;
    };
} mtrr_physbase_t;

typedef union {
    uint64_t    raw;
    struct {
        uint64_t reserved1 : 11;
        uint64_t v         : 1;      /* valid */
        uint64_t mask      : 24;
        uint64_t reserved2 : 28;
    };
} mtrr_physmask_t;

/* current procs only have 8, so this should hold us for a while */
#define MAX_VARIABLE_MTRRS      16

typedef struct {
    mtrr_def_type_t        mtrr_def_type;
    int                    num_var_mtrrs;
    mtrr_physbase_t     mtrr_physbases[MAX_VARIABLE_MTRRS];
    mtrr_physmask_t     mtrr_physmasks[MAX_VARIABLE_MTRRS];
} mtrr_state_t;

/*
 * Structure for holding saved CPU state that can be restored in
 * kernel module.  (cpu_state is for state that is primarily restored
 * while running the PAL).
 */
struct kcpu_state {
  mtrr_state_t    	saved_mtrr_state;
  uint32_t              saved_misc_enable_msr;
};

#ifdef _WIN32
#pragma pack(pop) // Pop the packed attribute
#endif // _WIN32

typedef struct kcpu_state kcpu_state_t;

extern void save_mtrrs(mtrr_state_t *saved_state);
extern void set_all_mtrrs(bool enable);
extern bool set_mem_type(void *base, uint32_t size, uint32_t mem_type);
extern void restore_mtrrs(mtrr_state_t *saved_state);
extern bool validate_mtrrs(const mtrr_state_t *saved_state);

#ifdef _WIN32
static int fls(int x)
{
    int r = 32;

    if (!x)
        return 0;
    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}
#endif // _WIN32

#endif /*__TXT_MTRRS_H__ */


/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
