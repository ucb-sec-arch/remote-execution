/*
 * verify.c: verify that platform and processor supports Intel(r) TXT
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
 *  verify.c: Modified for Flicker
 */

#ifndef _WIN32
#include <linux/module.h>
#include <asm/msr.h> /* for MSR_IA32_FEATURE_CONTROL and others */
#include <asm/processor-flags.h> /* for X86_CR* */
#else  // _WIN32
#include "wintypes.h"
#include "msr.h"
#include "txt_processor.h"
#endif // _WIN32

/* magic numbers not available in Linux kernel's headers */
#define IA32_FEATURE_CONTROL_MSR_SENTER_PARAM_CTL      0x7f00
#define IA32_FEATURE_CONTROL_MSR_ENABLE_SENTER         0x8000
#define FEATURE_CONTROL_VMXON_ENABLED_INSIDE_SMX  (1<<1)

#include "heap.h"
#include "smx.h"
#include "verify.h"
#include "io.h"

/*
 * CPUID extended feature info
 */
static unsigned int g_cpuid_ext_feat_info;

/*
 * IA32_FEATURE_CONTROL_MSR
 */
static unsigned long g_feat_ctrl_msr;


static void read_processor_info(void)
{
    unsigned long f1, f2;

    /* is CPUID supported? */
    /* (it's supported if ID flag in EFLAGS can be set and cleared) */
#ifndef _WIN32
    asm("pushf\n\t"
        "pushf\n\t"
        "pop %0\n\t"
        "mov %0,%1\n\t"
        "xor %2,%0\n\t"
        "push %0\n\t"
        "popf\n\t"
        "pushf\n\t"
        "pop %0\n\t"
        "popf\n\t"
        : "=&r" (f1), "=&r" (f2)
        : "ir" (X86_EFLAGS_ID));
    if ( ((f1^f2) & X86_EFLAGS_ID) == 0 ) {
        g_cpuid_ext_feat_info = 0;
        return;
    }

    g_cpuid_ext_feat_info = cpuid_ecx(1);
#else  // _WIN32
    {
    unsigned long cpuid_regs[4];
    __cpuid(cpuid_regs, 1);
    g_cpuid_ext_feat_info = cpuid_regs[2]; /* ecx */
    }
#endif // _WIN32

    rdmsrl(MSR_IA32_FEATURE_CONTROL, g_feat_ctrl_msr);
    dbg("MSR_IA32_FEATURE_CONTROL: %08lx", g_feat_ctrl_msr);
}

static bool supports_vmx(void)
{
    /* check that processor supports VMX instructions */
    if ( !(g_cpuid_ext_feat_info & bitmaskof(X86_FEATURE_VMXE)) ) {
        dbg("ERR: CPU does not support VMX");
        return false;
    }
    dbg("CPU is VMX-capable");

    /* and that VMX is enabled in the feature control MSR */
    if ( !(g_feat_ctrl_msr & FEATURE_CONTROL_VMXON_ENABLED_INSIDE_SMX) ) {
        dbg("ERR: VMXON disabled by feature control MSR (%lx)",
               g_feat_ctrl_msr);
        return false;
    }

    return true;
}

static bool supports_smx(void)
{
    /* check that processor supports SMX instructions */
    if ( !(g_cpuid_ext_feat_info & bitmaskof(X86_FEATURE_SMXE)) ) {
        dbg("ERR: CPU does not support SMX");
        return false;
    }
    dbg("CPU is SMX-capable");

    /*
     * and that SMX is enabled in the feature control MSR
     */

    /* check that the MSR is locked -- BIOS should always lock it */
    if ( !(g_feat_ctrl_msr & FEATURE_CONTROL_LOCKED) ) {
        dbg("ERR: FEATURE_CONTROL_LOCKED is not locked");
        /* in general this should not happen, as BIOS is required to lock */
        /* the MSR; but it may be desirable to allow it sometimes */
        return false;
    }

    /* check that SENTER (w/ full params) is enabled */
    if ( !(g_feat_ctrl_msr & (IA32_FEATURE_CONTROL_MSR_ENABLE_SENTER |
                              IA32_FEATURE_CONTROL_MSR_SENTER_PARAM_CTL)) ) {
        dbg("ERR: SENTER disabled by feature control MSR (%lx)",
               g_feat_ctrl_msr);
        return false;
    }

    return true;
}

static bool supports_txt(void)
{
    capabilities_t cap;

    /* processor must support SMX */
    if ( !supports_smx() || !supports_vmx() )
        return false;

    /* testing for chipset support requires enabling SMX on the processor */
    write_cr4(read_cr4() | X86_CR4_SMXE);
    dbg("SMX is enabled");

    /*
     * verify that an TXT-capable chipset is present and
     * check that all needed SMX capabilities are supported
     */

    cap = __getsec_capabilities(0);
    if ( cap.chipset_present ) {
        if ( cap.senter && cap.sexit && cap.parameters && cap.smctrl &&
             cap.wakeup ) {
            dbg("TXT chipset and all needed capabilities present");
            return true;
        }
        else
            dbg("ERR: insufficient SMX capabilities (%x)", cap._raw);
    }
    else
        dbg("ERR: TXT-capable chipset not present");

    /* since we are failing, we should clear the SMX flag */
    write_cr4(read_cr4() & ~X86_CR4_SMXE);

    return false;
}

static void print_bios_data(bios_data_t *bios_data)
{
    dbg("bios_data (@%p, %Lx):", bios_data,
           *((uint64_t *)bios_data - 1));
    dbg("\t version: %u", bios_data->version);
    dbg("\t bios_sinit_size: 0x%x (%u)", bios_data->bios_sinit_size,
           bios_data->bios_sinit_size);
    dbg("\t lcp_pd_base: 0x%Lx", bios_data->lcp_pd_base);
    dbg("\t lcp_pd_size: 0x%Lx (%Lu)", bios_data->lcp_pd_size,
           bios_data->lcp_pd_size);
    dbg("\t num_logical_procs: %u", bios_data->num_logical_procs);
    if ( bios_data->version >= 3 )
        dbg("\t flags: 0x%08Lx", bios_data->flags);
}

bool verify_bios_data(txt_heap_t *txt_heap)
{
    uint64_t size, heap_size;
    bios_data_t *bios_data;

    /* check size */
    heap_size = get_txt_heap_size();
    size = get_bios_data_size(txt_heap);
    if ( size == 0 ) {
        dbg("BIOS data size is 0");
        return false;
    }
    if ( size > heap_size ) {
        dbg("BIOS data size is larger than heap size "
               "(%Lx, heap size=%Lx)", size, heap_size);
        return false;
    }

    bios_data = get_bios_data_start(txt_heap);

    /* check version */
    if ( bios_data->version < 2 ) {
        dbg("unsupported BIOS data version (%u)", bios_data->version);
        return false;
    }
    /* we assume backwards compatibility but print a warning */
    if ( bios_data->version > 3 )
        dbg("unsupported BIOS data version (%u)", bios_data->version);

    /* all TXT-capable CPUs support at least 2 cores */
    if ( bios_data->num_logical_procs < 2 ) {
        dbg("BIOS data has incorrect num_logical_procs (%u)",
               bios_data->num_logical_procs);
        return false;
    }
//    else if ( bios_data->num_logical_procs >= NR_CPUS ) {
//        dbg("BIOS data specifies too many CPUs (%u)",
//               bios_data->num_logical_procs);
//        return false;
//    }

    print_bios_data(bios_data);

    return true;
}

static void print_os_mle_data(os_mle_data_t *os_mle_data)
{
    dbg("os_mle_data (@%p, 0x%Lx):", os_mle_data,
           *((uint64_t *)os_mle_data - 1));
    //printk("\t version: %u", os_mle_data->version);
    /* TBD: perhaps eventually print saved_mtrr_state field */
    //printk("\t mbi: 0x%p", os_mle_data->mbi);
}

bool verify_os_mle_data(txt_heap_t *txt_heap)
{
    uint64_t size, heap_size;
    os_mle_data_t *os_mle_data;

    /* check size */
    heap_size = read_priv_config_reg(TXTCR_HEAP_SIZE);
    size = get_os_mle_data_size(txt_heap);
    if ( size == 0 ) {
        error("OS to MLE data size is 0");
        return false;
    }
    if ( size > heap_size ) {
        error("OS to MLE data size is larger than heap size "
               "(%Lx, heap size=%Lx)", size, heap_size);
        return false;
    }
    if ( size < sizeof(os_mle_data_t) ) {
        error("OS to MLE data size (%Lx) is smaller than "
               "os_mle_data_t size (%x)", size, sizeof(os_mle_data_t));
        return false;
    }

    os_mle_data = get_os_mle_data_start(txt_heap);

    /* check version */
    /* since this data is from our pre-launch to
     * post-launch code only, it */
    /* should always be this */
    //if ( os_mle_data->version != 1 ) {
    //    printk("unsupported OS to MLE data version (%u)",
    //           os_mle_data->version);
    //    return false;
    //}

    /* Flicker doesn't use MBI */
    /*     /\* field checks *\/ */
    /*     if ( os_mle_data->mbi == NULL ) { */
    /*         printk("OS to MLE data mbi field is
     *         NULL"); */
    /*         return false; */
    /*     } */

    print_os_mle_data(os_mle_data);

    return true;
}


void print_os_sinit_data(os_sinit_data_t *os_sinit_data)
{
    dbg("function: os_sinit_data (@%p, 0x%Lx):", os_sinit_data,
           *((uint64_t *)os_sinit_data - 1));
    dbg("\t version: %u", os_sinit_data->version);
    dbg("\t mle_ptab: 0x%Lx", os_sinit_data->mle_ptab);
    dbg("\t mle_size: 0x%Lx (%Lu)", os_sinit_data->mle_size,
           os_sinit_data->mle_size);
    dbg("\t mle_hdr_base: 0x%Lx", os_sinit_data->mle_hdr_base);
    dbg("\t vtd_pmr_lo_base: 0x%Lx", os_sinit_data->vtd_pmr_lo_base);
    dbg("\t vtd_pmr_lo_size: 0x%Lx", os_sinit_data->vtd_pmr_lo_size);
    dbg("\t vtd_pmr_hi_base: 0x%Lx", os_sinit_data->vtd_pmr_hi_base);
    dbg("\t vtd_pmr_hi_size: 0x%Lx", os_sinit_data->vtd_pmr_hi_size);
    dbg("\t lcp_po_base: 0x%Lx", os_sinit_data->lcp_po_base);
    dbg("\t lcp_po_size: 0x%Lx (%Lu)", os_sinit_data->lcp_po_size,
           os_sinit_data->lcp_po_size);
    print_txt_caps("\t ", os_sinit_data->capabilities);
}

static bool verify_os_sinit_data(txt_heap_t *txt_heap)
{
    uint64_t size, heap_size;
    os_sinit_data_t *os_sinit_data;

    /* check size */
    heap_size = get_txt_heap_size();
    size = get_os_sinit_data_size(txt_heap);
    if ( size == 0 ) {
        dbg("OS to SINIT data size is 0");
        return false;
    }
    if ( size > heap_size ) {
        dbg("OS to SINIT data size is larger than heap size "
               "(%Lx, heap size=%Lx)", size, heap_size);
        return false;
    }

    os_sinit_data = get_os_sinit_data_start(txt_heap);

    /* check version (but since we create this, it should always be OK) */
    if ( os_sinit_data->version > 4 ) {
        dbg("unsupported OS to SINIT data version (%u)",
               os_sinit_data->version);
        return false;
    }

    /* only check minimal size */
    if ( size < sizeof(os_sinit_data_t) ) {
        dbg("OS to SINIT data size (%Lx) is smaller than "
               "os_sinit_data_t (%x)", size, sizeof(os_sinit_data_t));
        return false;
    }

    print_os_sinit_data(os_sinit_data);

    return true;
}

static void print_sinit_mdrs(sinit_mdr_t mdrs[], uint32_t num_mdrs)
{
    uint32_t i;
    static char *mem_types[] = {"GOOD", "SMRAM OVERLAY", "SMRAM NON-OVERLAY",
                                "PCIE EXTENDED CONFIG", "PROTECTED"};

    dbg("\t sinit_mdrs:");
    for ( i = 0; i < num_mdrs; i++ ) {
        dbg("\t\t %016Lx - %016Lx ", mdrs[i].base,
               mdrs[i].base + mdrs[i].length);
        if ( mdrs[i].mem_type < sizeof(mem_types)/sizeof(mem_types[0]) )
            dbg("(%s)", mem_types[mdrs[i].mem_type]);
        else
            dbg("(%d)", (int)mdrs[i].mem_type);
    }
}

static void print_hash(sha1_hash_t hash)
{
    int i;
    for ( i = 0; i < SHA1_SIZE; i++ )
        dbg("%02x ", hash[i]);
    dbg("");
}

static void print_sinit_mle_data(sinit_mle_data_t *sinit_mle_data)
{
    dbg("sinit_mle_data (@%p, %Lx):", sinit_mle_data,
           *((uint64_t *)sinit_mle_data - 1));
    dbg("\t version: %u", sinit_mle_data->version);
    dbg("\t bios_acm_id: \n\t");
        print_hash(sinit_mle_data->bios_acm_id);
    dbg("\t edx_senter_flags: 0x%08x",
           sinit_mle_data->edx_senter_flags);
    dbg("\t mseg_valid: 0x%Lx", sinit_mle_data->mseg_valid);
    dbg("\t sinit_hash:\n\t"); print_hash(sinit_mle_data->sinit_hash);
    dbg("\t mle_hash:\n\t"); print_hash(sinit_mle_data->mle_hash);
    dbg("\t stm_hash:\n\t"); print_hash(sinit_mle_data->stm_hash);
    dbg("\t lcp_policy_hash:\n\t");
        print_hash(sinit_mle_data->lcp_policy_hash);
    dbg("\t lcp_policy_control: 0x%08x",
           sinit_mle_data->lcp_policy_control);
    dbg("\t rlp_wakeup_addr: 0x%x", sinit_mle_data->rlp_wakeup_addr);
    dbg("\t num_mdrs: %u", sinit_mle_data->num_mdrs);
    dbg("\t mdrs_off: 0x%x", sinit_mle_data->mdrs_off);
    dbg("\t num_vtd_dmars: %u", sinit_mle_data->num_vtd_dmars);
    dbg("\t vtd_dmars_off: 0x%x", sinit_mle_data->vtd_dmars_off);
    print_sinit_mdrs((sinit_mdr_t *)
                     (((char*)sinit_mle_data - sizeof(uint64_t)) +
                      sinit_mle_data->mdrs_off), sinit_mle_data->num_mdrs);
}

static bool verify_sinit_mle_data(txt_heap_t *txt_heap)
{
    uint64_t size, heap_size;
    sinit_mle_data_t *sinit_mle_data;

    /* check size */
    heap_size = get_txt_heap_size();
    size = get_sinit_mle_data_size(txt_heap);
    if ( size == 0 ) {
        dbg("SINIT to MLE data size is 0");
        return false;
    }
    if ( size > heap_size ) {
        dbg("SINIT to MLE data size is larger than heap size"
               "(%Lx, heap size=%Lx)", size, heap_size);
        return false;
    }

    sinit_mle_data = get_sinit_mle_data_start(txt_heap);

    /* check version */
    sinit_mle_data = get_sinit_mle_data_start(txt_heap);
    if ( sinit_mle_data->version > 0x05 ) {
        dbg("unsupported SINIT to MLE data version (%x)",
               sinit_mle_data->version);
        return false;
    }

    /* this data is generated by SINIT and so is implicitly trustworthy, */
    /* so we don't need to validate it's fields */

    print_sinit_mle_data(sinit_mle_data);

    return true;
}

bool verify_txt_heap(txt_heap_t *txt_heap, bool bios_os_data_only)
{
    uint64_t size1, size2, size3, size4;

    dbg("verify_txt_heap");

    /* verify BIOS to OS data */
    if ( !verify_bios_data(txt_heap) )
        return false;

    if ( bios_os_data_only )
        return true;

    /* NOTE: We never reach here because init_txt_heap sets
     * bios_os_data_only as true. TODO: Ensure that os_sinit_data and
     * sinit_mle_data get verified. */

    /* check that total size is within the heap */
    size1 = get_bios_data_size(txt_heap);
    size2 = get_os_mle_data_size(txt_heap);
    size3 = get_os_sinit_data_size(txt_heap);
    size4 = get_sinit_mle_data_size(txt_heap);
    if ( (size1 + size2 + size3 + size4) >
         get_txt_heap_size() ) {
        dbg("TXT heap data sizes (%Lx, %Lx, %Lx, %Lx) are larger than"
               "heap total size (%Lx)", size1, size2, size3, size4,
               get_txt_heap_size());
        return false;
    }

    /* verify OS to MLE data */
    if ( !verify_os_mle_data(txt_heap) )
        return false;

    /* verify OS to SINIT data */
    if ( !verify_os_sinit_data(txt_heap) )
        return false;

    /* verify SINIT to MLE data */
    if ( !verify_sinit_mle_data(txt_heap) )
        return false;

    return true;
}

bool txt_verify_platform(void)
{
    txt_heap_t *txt_heap;
    txt_heap_t *txt_heap_io;
    uint64_t heap_size;
    uint64_t errorcode, e2sts;
    bool rv;
    read_processor_info();

    /* support Intel(r) TXT (this includes TPM support) */
    if ( !supports_txt() )
        return false;

    dbg("About to do Jay's check of configuration registers");
    e2sts = read_pub_config_reg(TXTCR_E2STS);
    dbg("E2STS register is %Lx", e2sts);
    errorcode = read_pub_config_reg(TXTCR_ERRORCODE);
    dbg("Error code is %Lx", errorcode);

    /* verify BIOS to OS data */
    dbg("Getting txt_heap");
    txt_heap = get_txt_heap();
    /* TODO: Is there more verification we should do here? */
    if(!txt_heap) {
        dbg("txt_heap NULL");
        return false;
    }

    heap_size = get_txt_heap_size();
    dbg("About to ioremap(txt_heap) of size %16llx", heap_size);

    txt_heap_io = f_ioremap((unsigned)txt_heap, (uint32_t)heap_size);
    if(!txt_heap_io) {
        dbg("ERROR: ioremap NULL");
        return false;
    }

    rv = verify_bios_data(txt_heap_io);
    f_iounmap(txt_heap_io, (uint32_t)heap_size);
    return rv;
}

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
