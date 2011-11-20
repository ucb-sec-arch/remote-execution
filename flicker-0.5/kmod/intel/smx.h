/*
 * smx.h: Intel(r) TXT SMX architecture-related definitions
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
 *  smx.h: Modified for Flicker
 */

#ifndef __TXT_SMX_H__
#define __TXT_SMX_H__

#define X86_CR4_SMXE        0x4000  /* enable SMX */

/*
 * GETSEC[] instructions
 */

/* GETSEC instruction opcode */
#define IA32_GETSEC_OPCODE    	".byte 0x0f,0x37"

/* GETSEC leaf function codes */
#define IA32_GETSEC_CAPABILITIES    0
#define IA32_GETSEC_SENTER    	    4
#define IA32_GETSEC_SEXIT    	    5
#define IA32_GETSEC_PARAMETERS    	6
#define IA32_GETSEC_SMCTRL          7
#define IA32_GETSEC_WAKEUP    	    8

/*
 * GETSEC[] leaf functions
 */

typedef union {
    uint32_t _raw;
    struct {
        uint32_t chipset_present  : 1;
        uint32_t undefined1          : 1;
        uint32_t enteraccs          : 1;
        uint32_t exitac              : 1;
        uint32_t senter              : 1;
        uint32_t sexit              : 1;
        uint32_t parameters          : 1;
        uint32_t smctrl              : 1;
        uint32_t wakeup              : 1;
        uint32_t undefined9          : 22;
        uint32_t extended_leafs   : 1;
    };
} capabilities_t;

#ifndef _WIN32
static inline capabilities_t __getsec_capabilities(uint32_t index)
{
    uint32_t cap;
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
              : "=a"(cap)
              : "a"(IA32_GETSEC_CAPABILITIES), "b"(index));
    return (capabilities_t)cap;
}
#else  // _WIN32
static capabilities_t __getsec_capabilities(uint32_t index)
{
    uint32_t cap;
    capabilities_t capt;

    __asm {
    	mov ebx, index
        mov eax, IA32_GETSEC_CAPABILITIES
    	__emit 0x0f
    	__emit 0x37
    	mov cap, eax
    }

    capt._raw = cap;
    return capt;
}
#endif //_WIN

static inline void __getsec_senter(uint32_t sinit_base, uint32_t sinit_size)
{
#ifndef _WIN32
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
    		  :
    		  : "a"(IA32_GETSEC_SENTER),
    		    "b"(sinit_base),
    		    "c"(sinit_size),
    		    "d"(0x0));
#else  // _WIN32
    __asm {
    	mov eax, IA32_GETSEC_SENTER
    	mov ebx, sinit_base
    	mov ecx, sinit_size
    	mov edx, 0
    	__emit 0x0f
    	__emit 0x37
    }
#endif // _WIN32
}

static inline void __getsec_sexit(void)
{
#ifndef _WIN32
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
                          : : "a"(IA32_GETSEC_SEXIT));
#else  // _WIN32
    __asm {
    	mov eax, IA32_GETSEC_SEXIT
    	__emit 0x0f
    	__emit 0x37
    }
#endif // _WIN32
}

static inline void __getsec_wakeup(void)
{
#ifndef _WIN32
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
                          : : "a"(IA32_GETSEC_WAKEUP));
#else  // _WIN32
    __asm {
    	mov eax, IA32_GETSEC_WAKEUP
    	__emit 0x0f
    	__emit 0x37
    }
#endif // _WIN32
}

static inline void __getsec_smctrl(void)
{
#ifndef _WIN32
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
                          : : "a"(IA32_GETSEC_SMCTRL), "b"(0x0));
#else  // _WIN32
    __asm {
    	mov eax, IA32_GETSEC_SMCTRL
    	__emit 0x0f
    	__emit 0x37
    }
#endif // _WIN32
}

static inline void __getsec_parameters(uint32_t index, int* param_type,
                                       uint32_t* peax, uint32_t* pebx,
                                       uint32_t* pecx)
{
    uint32_t eax_reg=0, ebx_reg=0, ecx_reg=0;

#ifndef _WIN32
    __asm__ __volatile__ (IA32_GETSEC_OPCODE "\n"
                          : "=a"(eax_reg), "=b"(ebx_reg), "=c"(ecx_reg)
                          : "a"(IA32_GETSEC_PARAMETERS), "b"(index));
#else  // _WIN32
    __asm {
    	mov eax, IA32_GETSEC_PARAMETERS
    	mov ebx, index
    	__emit 0x0f
    	__emit 0x37
    	mov eax_reg, eax
    	mov ebx_reg, ebx
    	mov ecx_reg, ecx
    }
#endif // _WIN32

    if ( param_type != NULL )   *param_type = eax_reg & 0x1f;
    if ( peax != NULL )         *peax = eax_reg;
    if ( pebx != NULL )         *pebx = ebx_reg;
    if ( pecx != NULL )         *pecx = ecx_reg;
}

static inline uint32_t read_eflags(void)
{
    uint32_t ef;

#ifndef _WIN32
    __asm__ __volatile__ ("pushfl; popl %0" : "=r" (ef));
#else  // _WIN32
    ef = __readeflags();
#endif // _WIN32

    return (ef);
}

static inline void write_eflags(uint32_t ef)
{
#ifndef _WIN32
    __asm__ __volatile__ ("pushl %0; popfl" : : "r" (ef));
#else  // _WIN32
    __writeeflags(ef);
#endif // _WIN32
}


#endif /* __TXT_SMX_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
