/*
 * txt.c: Intel(r) TXT support functions, including initiating measured
 *        launch, post-launch, AP wakeup, etc.
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
 *  txt.c: Modified for use with Flicker
 */

#ifndef _WIN32
#include <linux/module.h>
#include <asm/io.h> /* for virt_to_phys() */
#include <asm/processor-flags.h> /* for X86_CR* */
#include <linux/delay.h> /* udelay() mdelay() */
#else  // _WIN32
#include "wintypes.h"
#include "txt_processor.h"
#include "msr.h"
#endif // _WIN32

#include "txt.h"
#include "tpm.h"
#include "mle.h"
#include "config_regs.h"
#include "heap.h"
#include "smx.h"
#include "verify.h"
#include "log.h"
#include "flicker.h"
#include "io.h"



/* Memory address of mle_hdr. Somewhere in MLE binary */
/* MLE header structure needs to be within MLE pages */
/* TXT heap hold the address of MLE header */
static mle_hdr_t *g_mle_hdr_p;

/* region of memory to hold saved mtrrs and other kernel-restored cpu
 * state */
kcpu_state_t kcpu_region;

/*
 * Support function to find the offset (in bytes) of the mle header
 * inside the PAL.
 *
 * TODO: Since we are creating the MLEs, we really should control
 * where the header is, and should not have to search for
 * it. (However, at the moment it comes right after the AMD-specific
 * header, i.e., 4 bytes in, so the search should be quick) */
int search_byte(const unsigned char *target, int len){
    uint8_t *ptr;
    int i;

    ptr = g_pal->pal;

    dbg("begin@%p(g_pal->pal=%p) len=%d", ptr, g_pal->pal, len);
    dump_bytes(ptr, 128);
    for(i=0; i<sizeof(g_pal->pal); i++){
        if(!memcmp(ptr+i, target, len)) {
            dbg("found@%p (offset %08x)", ptr+i, i);
            return i;
        }
    }
    dbg("ERROR: Reached end@%p(%08x) without finding MLE header", ptr+i, i);
    return -1;
}

static void print_mle_hdr(const mle_hdr_t *mle_hdr)
{
    dbg("MLE header:");
    dbg("\t uuid="); print_uuid(&mle_hdr->uuid); dbg("");
    dbg("\t length=%x", mle_hdr->length);
    dbg("\t version=%08x", mle_hdr->version);
    dbg("\t entry_point=%08x", mle_hdr->entry_point);
    dbg("\t first_valid_page=%08x", mle_hdr->first_valid_page);
    dbg("\t mle_start_off=%x", mle_hdr->mle_start_off);
    dbg("\t mle_end_off=%x", mle_hdr->mle_end_off);
    print_txt_caps("\t ", mle_hdr->capabilities);
}


/* Volume3A_SystemProgrammingGuild.pdf Vol3 3-35 3.8.2 shows the
   Linear Address Translation with PAE Enabled (4-KByte Pages */
/* The format of each entry of Page Directory Pointer Table, Page Directory, Page Table
   is described in Figure3-20 */
/* page dir/table entry is phys addr + P + R/W + PWT */
/* Page Directory Pointer Table Entry does not have R/W bit */
#define MAKE_PDTE(addr)  (((uint64_t)(unsigned long)(addr) & PAGE_MASK) | 0x01)
#define MAKE_PDE(addr)  (((uint64_t)(unsigned long)(addr) & PAGE_MASK) | 0x03)
#define MAKE_PTE(addr)  (((uint64_t)(unsigned long)(addr) & PAGE_MASK) | 0x03)

/* We assume/know that our image is <2MB and thus fits w/in a single
 * page table page (512*4KB = 2MB). Thus, we need only 1 page
 * directory pointer, 1 page directory, and 1 page table page, for a
 * total of 3 pages. Requires just 1 for loop for ptable
 * creation. Note: MLE page table can only contain 4k pages. */

/* SENTER also requires a set of page tables to access MLE.  This
 * particular set of paging structures are enabled during SENTER
 * execution only.  This means that after SENTER ends, paging is
 * disabled.  The virtual address of the first content of MLE is
 * 0x00000000.  In other words, when we access virtual address
 * 0x00000000 with paging enabled, we need to access the first page of
 * the MLE resides. */

/* mle_start is the mle_start value in the MLE header, e.g., where
 * stuff starts getting mapped to memory in the MLE image. */
bool build_mle_pagetable(mle_pt_t *p, uint32_t mle_size, uint32_t mle_start){
    uint32_t mle_off;
    void *pg_dir_ptr_tab, *pg_dir, *pg_tab;
    uint64_t *pte;

    dbg("MLE start=%x, end=%x, size=%x", mle_start, mle_start+mle_size,
           mle_size);
    if ( mle_size > 512*PAGE_SIZE ) {
        dbg("MLE size too big for single page table");
        return false;
    }

    /* should start on page boundary */
    if ( mle_start & ~PAGE_MASK ) {
        dbg("MLE start is not page-aligned");
        return false;
    }

    memset(p, 0, sizeof(mle_pt_t));
    dbg("ptab_base=%p", p);

    pg_dir_ptr_tab = (void*)(p->pdpt);
    pg_dir         = (void*)(p->pd);
    pg_tab         = (void*)(p->pt);

    dbg("pg_dir_ptr_tab = %08x", (unsigned int)pg_dir_ptr_tab);
    dbg("pg_dir         = %08x", (unsigned int)pg_dir);
    dbg("pg_tab         = %08x", (unsigned int)pg_tab);
    dbg("");

    /* Note that senter requires PAE addressing mode.
       Size of each entry must be 8 bytes (64 bit) */
    *(uint64_t *)(pg_dir_ptr_tab) = MAKE_PDTE(virt_to_phys(pg_dir));

    /* "PAGE_OFFSET" MLE entry in PDT.
       In the same manner, each entry is 8 bytes */
    /* TODO: Remove assumption that PAGE_OFFSET = 0xc0000000 */
    //*(uint64_t *)(pg_dir) = MAKE_PDE((char*)pg_tab - 0xc0000000);
    *(uint64_t *)(pg_dir) = MAKE_PDE(virt_to_phys(pg_tab));

    pte = pg_tab;
    mle_off = 0;
    do {
    	//*pte = MAKE_PTE((uint32_t)p + sizeof(*p) - 0xc0000000 + mle_off + mle_start);
        *pte = MAKE_PTE(virt_to_phys(p) + sizeof(*p) + mle_off + mle_start);
    	//*pte = MAKE_PTE(virt_to_phys((uint32_t)p + sizeof(*p) + mle_off + mle_start));

        pte++;
        mle_off += PAGE_SIZE;
    } while ( mle_off < mle_size );

    return true;
}

/*
 * sets up TXT heap. Caller MUST free returned txt_heap_t pointer
 * using f_iounmap().
 */
static txt_heap_t *init_txt_heap(void *ptab_base, acm_hdr_t *sinit)
{
    txt_heap_t *txt_heap;
    txt_heap_t *txt_heap_io;
    os_sinit_data_t *os_sinit_data;
    os_mle_data_t   *os_mle_data;
    uint64_t *size;
    uint64_t heap_size;

    uuid_t mle_header = MLE_HDR_UUID;

    //get the address of txt_heap which is stored in the register
    txt_heap = get_txt_heap();
    if(!txt_heap) {
        dbg("txt_heap NULL");
        return NULL;
    }

    heap_size = get_txt_heap_size();
    txt_heap_io = f_ioremap((unsigned)txt_heap, (uint32_t)heap_size);
    if(!txt_heap_io) {
        dbg("f_ioremap NULL");
        return NULL;
    }

    /*
     * BIOS to OS/loader data already setup by BIOS
     */
    if ( !verify_txt_heap(txt_heap_io, true) ) {
        dbg("verify_txt_heap returned NULL!");
        return NULL;
    }

    /*
     * OS/loader to MLE data
     */
    os_mle_data = get_os_mle_data_start(txt_heap_io);
    size = (uint64_t *)((uint32_t)os_mle_data - sizeof(uint64_t));
    *size = sizeof(*os_mle_data) + sizeof(uint64_t);
    memset(os_mle_data, 0, sizeof(*os_mle_data));
    rdmsrl(MSR_IA32_MISC_ENABLE, os_mle_data->saved_misc_enable_msr);

    dbg("txt_heap@%p, txt_heap_io@%p:", txt_heap, txt_heap_io);

    if(!verify_os_mle_data(txt_heap_io)) {
      error("verify_os_mle_data FAILED");
      return NULL;
    }

    /*
     * OS/loader to SINIT data
     */
    os_sinit_data = get_os_sinit_data_start(txt_heap_io);
    size = (uint64_t *)((uint32_t)os_sinit_data - sizeof(uint64_t));
    *size = sizeof(*os_sinit_data) + sizeof(uint64_t);
    memset(os_sinit_data, 0, sizeof(*os_sinit_data));
    /* we only support version 4 (verify_acmod() will ensure SINIT supports */
    /* at least this) even if SINIT supports newer version */
    os_sinit_data->version = 0x04;
    /* this is phys addr */
    os_sinit_data->mle_ptab = (uint64_t)(unsigned long)(virt_to_phys(ptab_base));
    os_sinit_data->mle_size = g_mle_hdr_p->mle_end_off - g_mle_hdr_p->mle_start_off;
    /* g_mle_hdr.mle_end_off - g_mle_hdr.mle_start_off; */
    /* this is linear addr (offset from MLE base) of mle header */
    os_sinit_data->mle_hdr_base = search_byte((unsigned char *)&mle_header, 16);
    /* (uint64_t)&g_mle_hdr - (uint64_t)&_mle_start; */
    /* VT-d PMRs; we do not use VT-d for all of ram; we don't care about max_ram */
    //os_sinit_data->vtd_pmr_lo_base =
    //    (((uint32_t)g_pal->pal) & ~0x1fffff); /* 2MB aligned */
    /* TODO: Remove assumptions in these next two; determine during build process. */
    //os_sinit_data->vtd_pmr_lo_base -= 0xC0000000; /* XXX */
    //os_sinit_data->vtd_pmr_lo_base = virt_to_phys((void*)(((uint32_t)g_pal->pal) & ~0x1fffff));  /* 2MB aligned */
    os_sinit_data->vtd_pmr_lo_base = virt_to_phys(g_pal->pal) & ~0x1fffff;  /* 2MB aligned */
    os_sinit_data->vtd_pmr_lo_size = 0x200000; /* XXX MLE SIZE HERE XXX */

    /* LCP manifest: Flicker doesn't care */
    os_sinit_data->lcp_po_base = (unsigned long)0;
    os_sinit_data->lcp_po_size = 0;
    /* TODO: Can we scrap this capabilities stuff? */
    {
        /* capabilities : choose monitor wake mechanism first */
        txt_caps_t sinit_caps = get_sinit_capabilities(sinit);
        txt_caps_t caps_mask = { 0 };
        caps_mask.rlp_wake_getsec = caps_mask.rlp_wake_monitor = 1;
        os_sinit_data->capabilities._raw = MLE_HDR_CAPS & ~caps_mask._raw;
        if ( sinit_caps.rlp_wake_monitor )
            os_sinit_data->capabilities.rlp_wake_monitor = 1;
        else if ( sinit_caps.rlp_wake_getsec )
            os_sinit_data->capabilities.rlp_wake_getsec = 1;
        else {     /* should have been detected in verify_acmod() */
            dbg("SINIT capabilities are incompatible (0x%x)", sinit_caps._raw);
            return NULL;
        }
    }
    print_os_sinit_data(os_sinit_data);

    return txt_heap_io;
}

#ifndef _WIN32
static inline unsigned long get_eip(void){
    unsigned long eip;
    __asm__ __volatile__ ("call 1f; 1: pop %0" : "=r" (eip) : );
    return eip;
}
#else  // _WIN32
static inline unsigned long get_eip(void){
    unsigned long eip_reg;
    __asm {
    	__emit 0xe8
    	__emit 0x00
    	__emit 0x00
    	__emit 0x00
    	__emit 0x00
    	pop eip_reg
    }
    return eip_reg;
}
#endif // _WIN32

bool txt_is_launched(void)
{
    txt_sts_t sts;

    sts._raw = read_pub_config_reg(TXTCR_STS);

    return sts.senter_done_sts;
}

bool txt_launch_environment(acm_hdr_t *sinit, cpu_t *isk_state)
{
    acm_hdr_t *new_sinit = NULL;
    os_mle_data_t *os_mle_data;
    txt_heap_t *txt_heap_io;
    /* Fingerprint to search for MLE header */
    uuid_t mle_header = MLE_HDR_UUID; /* {0x5a, 0xac, 0x82, 0x90}; */
    /* Offset of MLE header from beginning of MLE binary */
    int g_mle_hdr_offset;
#ifdef _WIN32
    uint32_t tmpEbp, tmpEsp;
#endif // _WIN32

    unsigned long cr0, cr3, eip;
//    unsigned long long int curtime;

    dbg("sinit@%p, isk_state@%p", sinit, isk_state);
    assert(NULL != sinit);
    assert(NULL != isk_state);

    /* check if SINIT matches chipset */
    if ( !does_acmod_match_chipset(sinit) ) {
        dbg("SINIT does not match chipset");
        sinit = NULL;
    }

    /* Copy SINIT to BIOS reserved region. */
    if(!copy_sinit(sinit)) {
        error("copy_sinit FAILED!");
        return false;
    }

    /* do some checks on it */
    if ( !verify_acmod(sinit) ) {
        dbg("verify_acmod FAILED!");
        return false;
    }

    /* Search for the mle_header in the text section of the loaded
     * binary.  The variable "mle_header" currently contains a unique
     * byte sequence to look for in the of MLE header. */
    dbg("find mle_header (search_byte)");
    g_mle_hdr_offset = search_byte((unsigned char *)&mle_header, 16);
    if(g_mle_hdr_offset == -1)
        return false;
    g_mle_hdr_p = (mle_hdr_t *) ((char*)g_pal + g_mle_hdr_offset);


    /* Set the "Starting linear address of (first valid page of) MLE"
     * (see Table 1 of the MLE software development guide).  Since 0
     * works, this does not actually seem to be a linear address, but
     * rather a virtual address. */

    g_mle_hdr_p->first_valid_page = 0;

    /* STUDENTS: Interesting test. If the following line is
     * uncommented, we expect to see progress 0bh error 3 */
    //g_mle_hdr_p->first_valid_page = 0x10;

    print_mle_hdr(g_mle_hdr_p);

    /* create MLE page table */
    assert(NULL != g_mle_ptab);
    if(!build_mle_pagetable(g_mle_ptab,
                            g_mle_hdr_p->mle_end_off -
                            g_mle_hdr_p->mle_start_off,
                            g_mle_hdr_p->mle_start_off)) {
        error("build_mle_pagetable FAILED");
        return false;
    }

    dbg("sinit@%p", sinit);
    dbg("g_mle_ptab @ %p", g_mle_ptab);

    /* initialize TXT heap */
    txt_heap_io = init_txt_heap(g_mle_ptab, sinit);

    /* save MTRRs before we alter them for SINIT launch */
    /* These are for the pal, so cache enabled */
    os_mle_data = get_os_mle_data_start(txt_heap_io);
    dbg("Saving OS MTRRs");
    save_mtrrs(&(os_mle_data->saved_mtrr_state));
    /* These are for the kernel module, to restore things "just in case" */
    dbg("Saving kernel's MTRRs");
    save_mtrrs(&(kcpu_region.saved_mtrr_state));

    /* Save MISC ENABLE register */
    rdmsrl(MSR_IA32_MISC_ENABLE, kcpu_region.saved_misc_enable_msr);
    dbg("Read misc enable register: %04x", kcpu_region.saved_misc_enable_msr);

    f_iounmap(txt_heap_io, (size_t)get_txt_heap_size());

    /* Set MTRRs properly for AC module (SINIT).
     * NOTE: An untested alternative may be to explicitly set the sinit
     * address, instead of copying it to the existing value, e.g.,
     * write_pub_config_reg(TXTCR_SINIT_BASE, ...); */
    new_sinit = (acm_hdr_t *)((uint32_t)read_pub_config_reg(TXTCR_SINIT_BASE));
    dbg("new_sinit@%p", new_sinit);
    if(!set_mtrrs_for_acmod(new_sinit)) {
        dbg("set_mtrrs_for_acmod FAILED! Aborting launch!");
        return false;
    }

    /*
     * STUDENTS: Interesting test.  Overwrite the ACMod header.  If
     * this is uncommented, processor error 6 (Unrecognized AC module
     * format) will appear.
     */
    /*     io_mem = ioremap((unsigned int)sinit - 0x1000000, 0x2000000); */
    /*     memset_io(io_mem + 0x1000000, 0, 100); */
    /*     f_iounmap(io_mem); */

    /* Debugging: sanity checks */
    cr0 = read_cr0();
    cr3 = read_cr3();
    eip = get_eip();
    dbg("cr0=%lx, cr3=%lx, eip=%lx", cr0, cr3, eip);

    dbg("sinit@%p, size=%x", sinit, sinit->size);

    //serial_out_string("Executing senter\n\r");
    dbg("executing GETSEC[SENTER]...");

/*
    curtime = rdtsc();
    printk(KERN_ALERT "About to GETSEC[SENTER] at %llu\n", curtime);
*/
    serial_out_string("Getting ready to GETSEC[SENTER]");

    save_cpu_state(); /* flickermod.c */

    // Clear the TS bit of CR0 so that floating-point can work inside the MLE.
    // We do this after saving CPU state so that restoration does not restore
    // this modified CR0.
#ifndef _WIN32
    __asm__ __volatile__ ( "clts" );
#else  // _WIN32
    //__asm {clts}
#endif // _WIN32

    cr0 = read_cr0();
    dbg("after clts, cr0=%lx", cr0);

    /* TODO: Are we just getting lucky by omitting this? */
    //asm volatile("cli":);

    /* Save the general-purpose registers */
#ifndef _WIN32
    asm volatile ("pushal");
#else  // _WIN32
    __asm { pushad }
#endif // _WIN32

    /* Clear general-purpose registers, since AMD version does */
#ifdef _WIN32
    __asm {
    	xor ebx, ebx
    	xor ecx, ecx
    	xor edx, edx
    	xor edi, edi
    	xor esi, esi
    }
#endif // _WIN32

    /* push post-launch return address on stack */
#ifndef _WIN32
    asm volatile ("pushl $resume_target":);
#else  // _WIN32
    __asm { push resume_target }
#endif // _WIN32

    /* save stack registers at last second */
#ifndef _WIN32
    asm volatile ("movl %%ebp, %0" : "=m" (isk_state->ebp));
    asm volatile ("movl %%esp, %0" : "=m" (isk_state->esp));
#else  // _WIN32
    __asm {
    	mov tmpEbp, ebp
    	mov tmpEsp, esp
    }
    isk_state->ebp = tmpEbp;
    isk_state->esp = tmpEsp;
#endif // _WIN32

    __getsec_senter((uint32_t)new_sinit, (sinit->size)*4);

#ifndef _WIN32
    asm volatile ("resume_target:":);
#else  // _WIN32
    __asm { resume_target: }
#endif // _WIN32

    /* Restore the general-purpose registers we saved earlier */
#ifndef _WIN32
    asm volatile("popal");
#else  // _WIN32
    __asm { popad }
#endif // _WIN32

    //    dbg("txt_launch_environment RESUMED!\n\r");
#ifndef _WIN32
    asm volatile("sti":);
#else  // _WIN32
    __asm { sti }
#endif // _WIN32

    return true;
}

bool txt_prepare_platform(void)
{
    if ( is_tpm_ready(0) )
        return true;
    else
        return false;
}


bool txt_prepare_cpu(void)
{
    unsigned long eflags, cr0;
    uint64_t mcg_cap, mcg_stat;
    int i;

    /* must be running at CPL 0 => this is implicit in even getting this far */
    /* since our bootstrap code loads a GDT, etc. */

    cr0 = read_cr0();

    /* must be in protected mode */
    if ( !(cr0 & X86_CR0_PE) ) {
        dbg("ERR: not in protected mode");
        return false;
    }

    /* cache must be enabled (CR0.CD = CR0.NW = 0) */
    if ( cr0 & X86_CR0_CD ) {
        dbg("CR0.CD set");
        cr0 &= ~X86_CR0_CD;
    }
    if ( cr0 & X86_CR0_NW ) {
        dbg("CR0.NW set");
        cr0 &= ~X86_CR0_NW;
    }

    /* native FPU error reporting must be enabled for proper */
    /* interaction behavior */
    if ( !(cr0 & X86_CR0_NE) ) {
        dbg("CR0.NE not set");
        cr0 |= X86_CR0_NE;
    }

    write_cr0(cr0);

    /* cannot be in virtual-8086 mode (EFLAGS.VM=1) */
    eflags = read_eflags();
    if ( eflags & X86_EFLAGS_VM ) {
        dbg("EFLAGS.VM set");
        write_eflags(eflags | ~X86_EFLAGS_VM);
    }

    dbg("CR0 and EFLAGS OK");

    /*
     * verify all machine check status registers are clear
     */

    /* no machine check in progress (IA32_MCG_STATUS.MCIP=1) */
    rdmsrl(MSR_IA32_MCG_STATUS, mcg_stat);
    if ( mcg_stat & 0x04 ) {
        dbg("machine check in progress");
        return false;
    }

    /* all machine check regs are clear */
    rdmsrl(MSR_IA32_MCG_CAP, mcg_cap);
    for ( i = 0; i < (mcg_cap & 0xff); i++ ) {
        rdmsrl(MSR_IA32_MC0_STATUS + 4*i, mcg_stat);
        if ( mcg_stat & (1ULL << 63) ) {
            dbg("MCG[%d] = %Lx ERROR", i, mcg_stat);
            return false;
        }
    }

    dbg("no machine check errors");

    /* all is well with the processor state */
    dbg("CPU is ready for SENTER");

    return true;
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
