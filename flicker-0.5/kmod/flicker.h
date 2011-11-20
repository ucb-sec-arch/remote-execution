/*
 *  flicker.h: Definitions for Flicker kernel module
 */

#ifndef _FLICKER_H_
#define _FLICKER_H_

#ifndef _WIN32
    #include <linux/device.h>
    #include <linux/input.h>
    #include <linux/sysfs.h>
    #include <linux/types.h>
    #include <linux/slab.h>
#else // _WiN32
    #include "ntddk.h"
    #include "wintypes.h"
#endif // _WiN32

#include "acmod.h"
#include "../common/resume.h" /* cpu_t */

#define MAX_INPUT_SIZE 0x1d*PAGE_SIZE-sizeof(cpu_t)
#define MAX_OUTPUT_SIZE 0x1c*PAGE_SIZE
#define MAX_PAL_SIZE 0x80*PAGE_SIZE

#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32
struct pal_descriptor {
    uint8_t pal[MAX_PAL_SIZE];
    cpu_t reload;
    uint8_t inputs[MAX_INPUT_SIZE];
    uint8_t outputs[MAX_OUTPUT_SIZE];
    uint8_t resume_pagetabs[3*PAGE_SIZE]; /* must be 4K-aligned */
#ifndef _WIN32
} __attribute__((packed));
#else  // _WIN32
} ;
#pragma pack(pop) // Pop the packed attribute
#endif // _WIN32

typedef struct pal_descriptor pal_t;

#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32
struct mle_page_tables {
    uint8_t pdpt[PAGE_SIZE];
    uint8_t pd[PAGE_SIZE];
    uint8_t pt[PAGE_SIZE];
#ifndef _WIN32
} __attribute__((packed));
#else  // _WIN32
} ;
#pragma pack(pop) // Pop the packed attribute
#endif // _WIN32

typedef struct mle_page_tables mle_pt_t;

/* declared in latelaunch.c */
extern mle_pt_t *g_mle_ptab;
extern pal_t *g_pal;

#define ALIGN_128K        (0xfffe0000)

/* MSR Registers */
#define EFER_MSR          (0xc0000080)

/**
 * Determine CPU vendor.
 */

#define CPU_VENDOR_INTEL     0xAB
#define CPU_VENDOR_AMD     	0xCD

#define AMD_STRING_DWORD1 0x68747541
#define AMD_STRING_DWORD2 0x69746E65
#define AMD_STRING_DWORD3 0x444D4163

#define INTEL_STRING_DWORD1    0x756E6547
#define INTEL_STRING_DWORD2    0x49656E69
#define INTEL_STRING_DWORD3    0x6C65746E

#ifndef _WIN32
static inline int get_cpu_vendor(void) {
    uint32_t dummy;
    uint32_t vendor_dword1, vendor_dword2, vendor_dword3;

    cpuid(0, &dummy, &vendor_dword1, &vendor_dword3, &vendor_dword2);

    if(vendor_dword1 == AMD_STRING_DWORD1 && vendor_dword2 == AMD_STRING_DWORD2
       && vendor_dword3 == AMD_STRING_DWORD3)
        return CPU_VENDOR_AMD;

    if(vendor_dword1 == INTEL_STRING_DWORD1 && vendor_dword2 == INTEL_STRING_DWORD2
       && vendor_dword3 == INTEL_STRING_DWORD3)
        return CPU_VENDOR_INTEL;

    return -1;
}
#else  // _WIN32
static inline int get_cpu_vendor(void) {
    uint32_t vendor_dword1, vendor_dword2, vendor_dword3;
    uint32_t myEax, myEbx, myEcx, myEdx;

    __asm {
    	mov eax, 0
    	cpuid
    	mov myEax, eax
    	mov myEbx, ebx
    	mov myEcx, ecx
    	mov myEdx, edx
    }

    // Note that the registers are not alphabetically ordered!
    vendor_dword1 = myEbx;
    vendor_dword2 = myEdx;
    vendor_dword3 = myEcx;

    if(vendor_dword1 == AMD_STRING_DWORD1 && vendor_dword2 == AMD_STRING_DWORD2
       && vendor_dword3 == AMD_STRING_DWORD3)
        return CPU_VENDOR_AMD;

    if(vendor_dword1 == INTEL_STRING_DWORD1 && vendor_dword2 == INTEL_STRING_DWORD2
       && vendor_dword3 == INTEL_STRING_DWORD3)
        return CPU_VENDOR_INTEL;

    return -1;
}
#endif // !_WIN32

/**
 * convert from virtual to physical addresses
 */

#ifndef _WIN32
#define v2p(v) ((uint32_t)(v)-0xc0000000)
#else // _WIN32
#define v2p(v) ((uint32_t)MmGetPhysicalAddress((void *)v).QuadPart)
#endif // _WIN32


/* flicker.c */
int launch_drtm(void);
void save_cpu_state(void);

/* resume.c */
void build_resume_page_tables(void *target, void *buffer);

/* expect.c */
void compute_expected_pcrs(void);

/* {linux,windows}/sha.c */
int sha1(void *data, const unsigned int len, unsigned char digest[20]);

#endif /* _FLICKER_H_ */
