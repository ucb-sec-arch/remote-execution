/*
 * flicker.c: Main file for Flicker functionality
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
#include "txt.h"
#include "mtrrs.h"
#include "svm.h"
#include "io.h"
#include "smx.h"

#ifdef _WIN32
#include "msr.h"
#endif // _WIN32

/* InSecure Kernel state; system state to be restored post-flicker
 * session. */
static cpu_t isk_state;

/* region of memory to hold saved mtrrs and other kernel-restored cpu
 * state. Accessed here and in intel/txt.c. */
extern kcpu_state_t kcpu_region;

/* region of memory to hold ACMOD (aka SINIT), also used in sysfs.c */
acmod_t *g_acmod = NULL;
size_t g_acmod_size = 0;

/* globals declared extern in flicker.h */
mle_pt_t *g_mle_ptab = NULL;
pal_t *g_pal = NULL;

/* Pre-declare these so we can use them in save_cpu_state() */
void amd_kernel_reenter(void); /* XXX */
void intel_kernel_reenter(void); /* XXX */


/**
 * Write current cpu state into memory (presumably before a Flicker
 * session) so that it can be restored later (presumably after the
 * Flicker session).  EBP and ESP will be saved later. Inspired by
 * arch/i386/power/cpu.c in v2.6.24.
 */
#ifndef _WIN32
void save_cpu_state(void) {
  /* EFER */
  rdmsrl(EFER_MSR, isk_state.efer);

  /* eflags */
  asm volatile("pushfl");
  asm volatile("popl %0" : "=m" (isk_state.eflags));

  /* descriptor tables */
  asm volatile ("sgdt %0" : "=m" (isk_state.gdt_limit));
  asm volatile ("sidt %0" : "=m" (isk_state.idt_limit)); /* store 36bit */
  asm volatile ("sldt %0" : "=m" (isk_state.ldt));
  asm volatile ("str %0"  : "=m" (isk_state.tr));

  /* segment registers */
  asm volatile ("movw %%cs, %0" : "=r" (isk_state.cs));
  asm volatile ("movw %%ds, %0" : "=r" (isk_state.ds));
  asm volatile ("movw %%es, %0" : "=r" (isk_state.es));
  asm volatile ("movw %%fs, %0" : "=r" (isk_state.fs));
  asm volatile ("movw %%gs, %0" : "=r" (isk_state.gs));
  asm volatile ("movw %%ss, %0" : "=r" (isk_state.ss));

  /* control registers */
  asm volatile ("movl %%cr0, %0" : "=r" (isk_state.cr0));
  asm volatile ("movl %%cr2, %0" : "=r" (isk_state.cr2));
  asm volatile ("movl %%cr3, %0" : "=r" (isk_state.cr3));
  asm volatile ("movl %%cr4, %0" : "=r" (isk_state.cr4));

  /* Stack registers are saved at the last minute, later */

  /* return address */
  isk_state.return_address =
      (get_cpu_vendor() == CPU_VENDOR_INTEL) ?
      (uint32_t)intel_kernel_reenter : (uint32_t)amd_kernel_reenter;

  /* We want to set isk_state.p2v_offset such that _adding_ it to the
   * PAL's physical address will result in the correct OS virtual
   * address. Even though there is overflow if the virtual address is
   * less than the physical address, we still get the right answer.*/
  isk_state.p2v_offset = (uint32_t)&(g_pal->reload) -
      (uint32_t)virt_to_phys(&(g_pal->reload));

  /* Copy saved state to well-known area in PAL input buffer.
   * Note that assembly code in the PAL will reference the
   * reload region to restore CPU state when resumuing kernel. */
  memcpy(&(g_pal->reload), &isk_state, sizeof(cpu_t));

  logit("g_pal->reload (size: 0x%08x) saved at virt/phys: 0x%08x / 0x%08x",
        sizeof(g_pal->reload), (uint32_t)&(g_pal->reload), (uint32_t)virt_to_phys(&(g_pal->reload)));


  //dump_state(&isk_state);
}

#else // _WIN32

void save_cpu_state(void) {
    /* EFER */
    isk_state.efer = (unsigned long)__readmsr(EFER_MSR);

    /* eflags */
    isk_state.eflags = __readeflags();

    /* descriptor tables */
    __asm {
    	sgdt isk_state.gdt_limit
    	sidt isk_state.idt_limit
    	sldt isk_state.ldt
    	str  isk_state.tr ; causes a warning about size, do not understand why, x-ref w/ binary generated in linux and it is the same
    }

    /* segment registers */
    __asm {
    	mov isk_state.cs, cs
    	mov isk_state.ds, ds
    	mov isk_state.es, es
    	mov isk_state.fs, fs
    	mov isk_state.gs, gs
    	mov isk_state.ss, ss
    }

    /* control registers */
    isk_state.cr0 = __readcr0();
    isk_state.cr2 = __readcr2();
    isk_state.cr3 = __readcr3();
    isk_state.cr4 = __readcr4();

    /* data registers */
    __asm {
    	mov isk_state.esi, esi
        mov isk_state.edi, edi
    }

    /* Stack registers are saved at the last minute, later */


    /* return address */
    isk_state.return_address =
    	(get_cpu_vendor() == CPU_VENDOR_INTEL) ?
    	(uint32_t)intel_kernel_reenter : (uint32_t)amd_kernel_reenter;

    /* stack info - not necessary here, but interesting to get rough idea of stack layout for this driver */
    __asm {
    	mov isk_state.ebp, ebp
    	mov isk_state.esp, esp
    }

    /* We want to set isk_state.p2v_offset such that _adding_ it to the
     * PAL's physical address will result in the correct OS virtual
     * address. Even though there is overflow if the virtual address is
     * less than the physical address, we still get the right answer.*/
    isk_state.p2v_offset = (uint32_t)&(g_pal->reload) -
        (uint32_t)MmGetPhysicalAddress((void *)&(g_pal->reload)).QuadPart;

    /* Copy saved state to well-known area in PAL input buffer.
     * Note that assembly code in the PAL will reference the
     * reload region to restore CPU state when resumuing kernel. */
    memcpy(&(g_pal->reload), &isk_state, sizeof(cpu_t));

    logit("g_pal->reload (size: 0x%08x) saved at virt/phys: 0x%08x / 0x%08x",
    	sizeof(g_pal->reload), (uint32_t)&(g_pal->reload), virt_to_phys(&(g_pal->reload)));

}
#endif // _WIN32

/**
 * Code to restart the kernel after the PAL finishes
 * executing. This function behaves strangely!  At times the stack is
 * not available, so local variables don't work as expected.  You must
 * know what you're doing before edits to this function will produce
 * the desired results.  Lots of debug code and notes that were
 * relevant during development (they may be system-specific and have
 * no meaning on your hardware) left in-place for educational
 * purposes. THIS FUNCTION DOES NOT RETURN LIKE A NORMAL FUNCTION.
 */
#ifndef _WIN32
void intel_kernel_reenter(void) {

#ifdef PERFCRIT
    stop_timer();
#endif

    // This line seems to crash Flicker, so taking it out:
    //    dbg("Hot dog, we're back\n\r");

#ifdef PERFCRIT
    print_timer();
#endif

    /* let's compare CPU special-purpose registers */

    /*
     * control registers
     * bit mapping is described in Intel Architecture 3A 2.5 (Vol.3 2-17)
     */
    //asm volatile ("movl %%cr0, %0" : "=r" (temp));
    //serial_printk("saved cr0=%08lx\n", isk_state.cr0);
    //serial_printk("current cr0=%08lx\n", temp);
    //saved   805003b: 1000 0000 0101 0000 0000 0011 1011
    //current 800003b: 1000 0000 0000 0000 0000 0011 1011
    //WP(bit16)
    //AM(bit18)

    //asm volatile ("movl %%cr4, %0" : "=r" (temp));
    //serial_printk("saved cr4=%08lx\n", isk_state.cr4);
    //serial_printk("current cr4=%08lx\n", temp);
    //saved   4680: 0100 0110 1000 0000
    //current 4000: 0100 0000 0000 0000

    //    printbitdiffs(isk_state.cr0, temp, "cr0");
    //    asm volatile ("movl %%cr2, %0" : "=r" (temp));
    //    printbitdiffs(isk_state.cr2, temp, "cr2");
    //    asm volatile ("movl %%cr3, %0" : "=r" (temp));
    //    printbitdiffs(isk_state.cr3, temp, "cr3");
    //    asm volatile ("movl %%cr4, %0" : "=r" (temp));
    //    printbitdiffs(isk_state.cr4, temp, "cr4");

    /* eflags */
    //    asm volatile("pushfl");
    //    asm volatile("popl %0" : "=m" (temp));
    //    printbitdiffs(isk_state.eflags, temp, "eflags");

    /* EFER */
    //    asm volatile("movl $0xc0000080, %%ecx":); /* specify EFER */
    //    asm volatile("rdmsr"); /* puts value in EAX (and EDX in 64-bit
    //                            * mode) */
    //    asm volatile("movl %%eax, %0" : "=m" (temp));
    //    printbitdiffs(isk_state.efer, temp, "efer");

    /* Now, do the actual reload */

    /* Control registers */
    asm volatile ("movl %0, %%cr0" :: "r" (isk_state.cr0));
    //    dbg("0x%08lx restored into cr0", isk_state.cr0);
    asm volatile ("movl %0, %%cr4" :: "r" (isk_state.cr4));
    //    dbg("0x%08lx restored into cr4", isk_state.cr4);

    /* reload es,fs,gs */
    asm volatile ("movw %0, %%es" :: "r" (isk_state.es));
    asm volatile ("movw %0, %%fs" :: "r" (isk_state.fs));
    asm volatile ("movw %0, %%gs" :: "r" (isk_state.gs));


    /* before reloading the TR, we must set the "busy" flag of the TSS
     * to 0.  Otherwise, switching to a busy task causes a #GP. */
    *((unsigned long *)(isk_state.gdt_base + isk_state.tr + 4)) &= 0xfffffdff;

    /* reload tr */
    asm volatile ("ltr %0" :: "m" (isk_state.tr));
    //serial_printk("task register reloaded %08lx\n", isk_state.tr);

    /* compare eflags */
    //asm volatile("pushfl");
    //asm volatile("popl %0" : "=m" (temp));
    //serial_printk("saved eflags=%08lx\n", isk_state.eflags);
    //0x296 = 10 1001 0110
    // IF(1) TF(0) | SF(1) ZF(0) 0 AF(1)| 0 PF(1) 1 CF(0)
    //serial_printk("current eflags=%08lx\n", temp);
    //0x82 =  00 1000 0010
    // IF(0) TF(0) | SF(1) ZF(0) 0 AF(0)| 0 PF(0) 1 CF(0)

    /* compare EFER */
    //asm volatile("movl $0xc0000080, %%ecx":); /* specify EFER */
    //asm volatile("rdmsr"); /* puts value in EAX (and EDX in 64-bit mode) */
    //asm volatile("movl %%eax, %0" : "=m" (temp));
    //serial_printk("original EFER=%08lx\n", isk_state.efer); //0x0
    //serial_printk("current EFER=%08lx\n", temp); //0x0

    /* compare IDT */
    //serial_printk("original idt_base=%08lx\n", isk_state.idt_base);
    //serial_printk("original idt_limit=%04x\n", isk_state.idt_limit);
    //idt_base = c0505000, limit = 07ff
    //idt_base = (unsigned char *)isk_state.idt_base;
    //dump_bytes(idt_base, 0x40);
    /*
     (here)
      d0 4b 60 00 00 8f 10 c0 c0 c6 60 00 00 8e 3e
      c0 0c c7 60 00 00 8e 3e c0 28 c8 60 00 00 ee
      3e c0 70 4b 60 00 00 ef 10 c0 7c 4b 60 00 00
      8f 10 c0 88 4b 60 00 00 8f 10 c0 2c 4b 60 00
    */

    /*
     (At save_cpu_state)
      d0 4b 60 00 00 8f 10 c0 c0 c6 60 00 00 8e 3e c0
      0c c7 60 00 00 8e 3e c0 28 c8 60 00 00 ee 3e c0
      70 4b 60 00 00 ef 10 c0 7c 4b 60 00 00 8f 10 c0
      88 4b 60 00 00 8f 10 c0 2c 4b 60 00 00 8f 10 c0
    */
    //asm volatile ("sidt %0" : "=m" (tmp_idt));
    //dump_bytes((unsigned char *)&tmp_idt, 0x6);
    /* 00 50 50 c0 00 00 */

    //rdmsrl(MSR_IA32_MISC_ENABLE, misc_msr);
    //serial_printk("misc_msr=%08x\n", misc_msr); //66952488

    /* re-enable EFLAGS.IF MOVED TO txt.c so we know that the stack is
     * properly laid out. */
    //asm volatile("sti":);

    /* Reload Linux kernel's stack.  Just before SENTER (in txt.c) we
     * did a "pushl $resume_target" to push the desired post-Flicker
     * IP on the kernel's stack.  We then saved esp and ebp just prior
     * to calling SENTER.  To reload things, we want to restore ESP
     * and EBP.
     */

    /* Restore the MTRRs here */
    restore_mtrrs(&(kcpu_region.saved_mtrr_state));

    /* restore pre-SENTER IA32_MISC_ENABLE_MSR (no verification needed) */
    wrmsrl(MSR_IA32_MISC_ENABLE, kcpu_region.saved_misc_enable_msr);

    /* OVERRIDE C-Style calling convention return of this function. We
     * want to return to txt_launch_environment() just after the
     * invocation of SENTER.
     * Put ESP back to what it was just prior to invoking SENTER.
     * note that this will make variables stored on the stack in
     * this function unavailable.
     */
    asm volatile ("movl %0, %%esp"::"r"(isk_state.esp));
    asm volatile ("movl %0, %%ebp"::"r"(isk_state.ebp));
    asm volatile ("ret":);
}
#else  // _WIN32


void intel_kernel_reenter(void) {

#ifdef PERFCRIT
    stop_timer();
    print_timer();
#endif

    /* Do the actual reload */

    /* Control registers */
    write_cr0(isk_state.cr0);
    write_cr4(isk_state.cr4);

    /* reload es,fs,gs */
    __asm {
    	mov es, isk_state.es
    	mov fs, isk_state.fs
    	mov gs, isk_state.gs
    }

    /* before reloading the TR, we must set the "busy" flag of the TSS
     * to 0.  Otherwise, switching to a busy task causes a #GP. */
    *((unsigned long *)(isk_state.gdt_base + isk_state.tr + 4)) &= 0xfffffdff;

    /* reload tr */
    __asm {
    	ltr isk_state.tr
    }

    /* reload some data registers */
    __asm {
    	mov esi, isk_state.esi
    	mov edi, isk_state.edi
    }

    /* Restore the MTRRs here */
    restore_mtrrs(&(kcpu_region.saved_mtrr_state));

    /* restore pre-SENTER IA32_MISC_ENABLE_MSR (no verification needed) */
    wrmsrl(MSR_IA32_MISC_ENABLE, kcpu_region.saved_misc_enable_msr);

    /* Reload eflags */
    write_eflags(isk_state.eflags);

    /* OVERRIDE C-Style calling convention return of this function. We
     * want to return to txt_launch_environment() just after the
     * invocation of SENTER.
     * Put ESP back to what it was just prior to invoking SENTER.
     * note that this will make variables stored on the stack in
     * this function unavailable.
     */
    __asm {
    	mov esp, isk_state.esp
    	mov ebp, isk_state.ebp
    	ret
    }
}
#endif // _WIN32

/**
 *  Does pre-launch checks to make sure flicker will work on this
 *  machine.
 *
 *  Returns: 0 on success, EIO on failure
 *
 *  BUG: We should move some of these checks to an earlier spot.  At
 *  this point, it's really too late to do anything.
 */
int prepare_for_launch(void)
{
    unsigned long apicbase;

    dbg("#######################################");
    dbg("prepare_for_launch() in kmod");

    /* we should only be executing on the BSP */
    rdmsrl(MSR_IA32_APICBASE, apicbase);
    if ( !(apicbase & MSR_IA32_APICBASE_BSP) ) {
        dbg("entry processor is not BSP");
        goto failed;
    }

    /* we need to make sure this is a (TXT-) capable platform before using */
    /* any of the features, incl. those required to check if the environment */
    /* has already been launched */

    /* REBOOTS upon SENTER without this!!! */
    /* make TPM ready for measured launch */
    if ( !txt_prepare_platform() )
        goto failed;

    /* Check the MTRRs before launch */
    if ( !validate_mtrrs(&(kcpu_region.saved_mtrr_state)) ) {
       dbg("MTRRs failed validation");
    }

    /* need to verify that platform can perform measured launch */
    if ( !txt_verify_platform() )
        goto failed;

    /* print any errors from last boot */
    txt_get_error();

    /* Make sure we're not already in a measured environment (this can
     * happen when GETSEC[SEXIT] is not called) */
    if ( txt_is_launched() ) {
        dbg("ERROR: txt_is_launched() already!");
        goto failed;
    }

    /* Make sure our SINIT module looks like one */
    if( !is_sinit_acmod(g_acmod->acm.raw, g_acmod_size) )
        goto failed;

    /* make the CPU ready for measured launch */
    if ( !txt_prepare_cpu() )
        goto failed;

    dbg("prepare_for_launch succeeded");
    return 0;

  failed:
    dbg("prepare_for_launch failed");
#ifndef _WIN32
    return -ENXIO;
#else  // _WIN32
    return -1;
#endif // _WIN32
}

int launch_senter(void) {
  int rv;

  save_cpu_state();

  if ((rv = prepare_for_launch())) {
      return rv;
  }

  /* launch the measured environment */
  if (!(rv = txt_launch_environment(&(g_acmod->acm.hdr), &isk_state))) {
      return rv;
  }

  return 0;
}

#ifndef _WIN32
static inline uint64_t rdtsc64(void) {
    uint64_t t;
    __asm__ __volatile__("rdtsc" : "=A"(t));
    return t;
}
#else  // _WIN32
static inline uint64_t rdtsc64(void) {
    __asm {
    	; Flush the pipeline
    	XOR eax, eax
    	CPUID
    	; Get the RDTSC counter into edx:eax
    	RDTSC
    }
}
#endif // _WIN32


/**
 * BEGIN AMD-SPECIFIC FUNCTIONS
 */

/*
 * If you call functions in here (that don't get inlined), or if you
 * use local variables, resume will fail.
 */

#ifndef _WIN32
void amd_kernel_reenter(void) {

    serial_outchar('K');
    serial_outchar('E');
    serial_outchar('R');
    serial_outchar('N');
    serial_outchar('E');
    serial_outchar('L');

    /* Control registers */
    asm volatile ("movl %0, %%cr0" :: "r" (isk_state.cr0));
    asm volatile ("movl %0, %%cr4" :: "r" (isk_state.cr4));

    /* reload es,fs,gs */
    asm volatile ("movw %0, %%es" :: "r" (isk_state.es));
    asm volatile ("movw %0, %%fs" :: "r" (isk_state.fs));
    asm volatile ("movw %0, %%gs" :: "r" (isk_state.gs));

    /* before reloading the TR, we must set the "busy" flag of the TSS
     * to 0.  Otherwise, switching to a busy task causes a #GP. */
    *((unsigned long *)(isk_state.gdt_base + isk_state.tr + 4)) &= 0xfffffdff;

    /* reload tr */
    asm volatile ("ltr %0" :: "m" (isk_state.tr));

    /* OVERRIDE C-Style calling convention return of this function. We
     * want to return to txt_launch_environment() just after the
     * invocation of SENTER.
     * Put ESP back to what it was just prior to invoking SENTER.
     * note that this will make variables stored on the stack in
     * this function unavailable.
     */
    asm volatile ("movl %0, %%esp"::"r"(isk_state.esp));
    asm volatile ("movl %0, %%ebp"::"r"(isk_state.ebp));

    /* Set SVME bit in EFER, so that we can set GIF */
     asm volatile(
         "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
         "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
         "orl $0x00001000, %%eax\n\t" /* set bit 12 */
         "wrmsr\n\t" /* SVME should now be enabled */
         :
         :
         : "%edx");

    /* re-enable GIF */
    asm volatile("stgi":);

    /* Clear SVME bit in EFER */
    asm volatile(
        "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
        "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
        "andl $0xffffefff, %%eax\n\t" /* clear bit 12 */
        "wrmsr\n\t" /* SVME should now be disabled */
        :
        :
        : "%edx");

    /* avoid standard epilogue */
    asm volatile ("ret":);
}


/* We want to time the number of elapsed cycles during an skinit
 * invocation.  Before skinit runs, we grab the start time.  That is
 * the purpose of this function.  The start time is kept in the cpu_t
 * reload_state struct along with the rest of the CPU state, so that
 * it gets passed as a parameter into the SLB.
 * slb_finish_skinit_timer() takes the final measurement, computes the
 * difference, and prints it out from inside the SLB.
 */
void start_skinit_timer(cpu_t *s) {
    assert(NULL != s);
    __asm__ __volatile__("rdtsc" : "=a" (s->startl), "=d" (s->starth));

    logit("rdtsc: %08lx:%08lx", s->starth, s->startl);
}

/**
 * Send an INIT IPI to all APs.
 */
void amd_init_ipi_aps(void) {
    int i=0;

    /*
     * Send INIT IPI
     */
    apic_write(APIC_ICR,
               APIC_DEST_ALLBUT |
               //APIC_INT_LEVELTRIG |
               APIC_INT_ASSERT |
               //(0x110 << 8) /* StartUp -> Should crash? */
               APIC_DM_INIT);

    dbg("apic_read(APIC_ICR): %08lx", (unsigned long)apic_read(APIC_ICR));

    //dbg("Waiting for send to finish...\n");
    while(apic_read(APIC_ICR) & APIC_ICR_BUSY) {
        mdelay(1);
        i++;
    }
    dbg("apic_read(APIC_ICR) & APIC_ICR_BUSY -> %d iterations", i);
}

void amd_do_skinit(void) {
    uint32_t phys;      /* physical addr containing SLB */

    assert(NULL != g_pal);

    enable_svme();
    save_cpu_state();

    phys = virt_to_phys(g_pal->pal);
    dbg("PAL physical address: 0x%08x", phys);

    amd_init_ipi_aps();
    ///---XXX TODO hash_slb();

    start_skinit_timer(&(g_pal->reload));
    dbg("rdtsc is now: %lld", rdtsc64());

    /* put physical address of SLB into eax */
    asm volatile ("movl %0, %%eax" :: "r" (phys));
    /* clear remaining registers */
    asm volatile ("xorl %%ebx, %%ebx":);
    asm volatile ("xorl %%ecx, %%ecx":);
    asm volatile ("xorl %%edx, %%edx":);
    asm volatile ("xorl %%edi, %%edi":);
    asm volatile ("xorl %%esi, %%esi":);

    /* push post-skinit return address on stack */
    asm volatile ("pushl $resume_target\n\t":);

    /* save stack registers at last second */
    asm volatile ("movl %%ebp, %0" : "=m" (isk_state.ebp));
    asm volatile ("movl %%esp, %0" : "=m" (isk_state.esp));

    /* spin for 1000 cycles per AMD manual requirements:
       Publication No. Revision Date
       24593           3.14     September 2007
       "15.26.8 Secure Multiprocessor Initialization"
       "Software Requirements for Secure MP initialization."
       "...a fixed delay of no more than 1000 processor cycles may be
       necessary before executing SKINIT to ensure reliable sensing of
       APIC INIT state by the SKINIT"
     */
    asm volatile("mov $1000, %%ebx \n"
                 "1: subl $1, %%ebx \n"
                 "jnz 1b \n"
                 : );

    /* CALL SKINIT */
    asm volatile("skinit");

    asm volatile("resume_target:":);

    serial_outchar('.');
    serial_outchar('\r');
    serial_outchar('\n');
    dbg("rdtsc is now: %lld", rdtsc64());

    dbg("exit.");
}

#else  // _WIN32

void amd_kernel_reenter(void) {
    serial_outchar('K');
    serial_outchar('E');
    serial_outchar('R');
    serial_outchar('N');
    serial_outchar('E');
    serial_outchar('L');

    /* Control registers */
    __writecr0(isk_state.cr0);
    __writecr4(isk_state.cr4);

    /* reload es,fs,gs */
    __asm {
    	mov es, isk_state.es
    	mov fs, isk_state.fs
    	mov gs, isk_state.gs
    }

    /* before reloading the TR, we must set the "busy" flag of the TSS
     * to 0.  Otherwise, switching to a busy task causes a #GP. */
    *((unsigned long *)(isk_state.gdt_base + isk_state.tr + 4)) &= 0xfffffdff;

    /* reload tr */
    __asm {
    	ltr isk_state.tr  ; causes a warning about size, do not understand why, x-ref w/ binary generated in linux and it is the same
    }

    /* OVERRIDE C-Style calling convention return of this function. We
     * want to return to txt_launch_environment() just after the
     * invocation of SENTER.
     * Put ESP back to what it was just prior to invoking SENTER.
     * note that this will make variables stored on the stack in
     * this function unavailable.
     */
    __asm {
        mov edi, isk_state.edi
    	mov esp, isk_state.esp
    	mov ebp, isk_state.ebp
    }

    /* Set SVME bit in EFER, so that we can set GIF */
     //asm volatile(
     //    "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
     //    "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
     //    "orl $0x00001000, %%eax\n\t" /* set bit 12 */
     //    "wrmsr\n\t" /* SVME should now be enabled */
     //    :
     //    :
     //    : "%edx");
    __asm {
    	mov ecx, 0xc0000080  /* specify EFER */
    	rdmsr				 /* puts value in EAX (and EDX in 64-bit mode) */
    	or eax, 0x00001000	 /* set bit 12 */
    	wrmsr				 /* SVME should now be enabled */
    }

    /* re-enable Global Interrupt Flag (GIF) */
    //asm volatile("stgi":);
    // STGI opcode: 0x0f01dc
    __asm {
    	__emit 0x0f
    	__emit 0x01
    	__emit 0xdc
    }

    /* Clear SVME bit in EFER */
    //asm volatile(
    //    "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
    //    "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
    //    "andl $0xffffefff, %%eax\n\t" /* clear bit 12 */
    //    "wrmsr\n\t" /* SVME should now be disabled */
    //    :
    //    :
    //    : "%edx");
    __asm {
    	mov ecx, 0xc0000080  /* specify EFER */
    	rdmsr				 /* puts value in EAX (and EDX in 64-bit mode) */
    	and eax, 0xffffefff	 /* clear bit 12 */
    	wrmsr				 /* SVME should now be disabled */
    }

    /* avoid standard epilogue */
    //asm volatile ("ret":);
    __asm {
    	ret
    }
}


/* We want to time the number of elapsed cycles during an skinit
 * invocation.  Before skinit runs, we grab the start time.  That is
 * the purpose of this function.  The start time is kept in the cpu_t
 * reload_state struct along with the rest of the CPU state, so that
 * it gets passed as a parameter into the SLB.
 * slb_finish_skinit_timer() takes the final measurement, computes the
 * difference, and prints it out from inside the SLB.
 */
void start_skinit_timer(cpu_t *s) {
    long myEax, myEdx;
    assert(NULL != s);

    __asm {
    	; Flush the pipeline
    	XOR eax, eax
    	CPUID
    	; Get the RDTSC counter into edx:eax
    	rdtsc
    	mov myEax, eax
    	mov myEdx, edx
    }

    s->startl = myEax;
    s->starth = myEdx;

    logit("rdtsc: %08lx:%08lx", s->starth, s->startl);
}

/**
 * Send an INIT IPI to all APs.
 */
void amd_init_ipi_aps(void) {
    int i=0;

    /*
     * Send INIT IPI
     */
    // TODO: Figure out how to do this on Windows!
    /*
    apic_write(APIC_ICR,
               APIC_DEST_ALLBUT |
               //APIC_INT_LEVELTRIG |
               APIC_INT_ASSERT |
               //(0x110 << 8) // StartUp -> Should crash?
               APIC_DM_INIT);

    dbg("apic_read(APIC_ICR): %08lx", (unsigned long)apic_read(APIC_ICR));

    //dbg("Waiting for send to finish...\n");
    while(apic_read(APIC_ICR) & APIC_ICR_BUSY) {
        mdelay(1);
        i++;
    }
    dbg("apic_read(APIC_ICR) & APIC_ICR_BUSY -> %d iterations", i);
    */
}

void amd_do_skinit(void) {
    uint32_t phys;      /* physical addr containing SLB */

    assert(NULL != g_pal);

    serial_printk("Before saving cpu state, cr3 = 0x", read_cr3());

    enable_svme();
    save_cpu_state();

    serial_printk("After saving cpu state, cr3 = 0x", read_cr3());

    phys = virt_to_phys(g_pal->pal);
    dbg("PAL physical address: 0x%08x", phys);

    amd_init_ipi_aps();
    ///---XXX TODO hash_slb();

    start_skinit_timer(&(g_pal->reload));
    dbg("rdtsc is now: %lld", rdtsc64());

    serial_printk("Before assembly, cr3 = 0x", read_cr3());

    /* put physical address of SLB into eax */
    __asm {
    	mov eax, phys
    }

    /* clear remaining registers */
    __asm {
    	xor ebx, ebx
    	xor ecx, ecx
    	xor edx, edx
    	xor edi, edi
    	xor esi, esi
    }

    /* push post-skinit return address on stack */
    __asm {
    	push resume_target
    }

    /* save stack registers at last second */
    __asm {
    	mov isk_state.ebp, ebp
    	mov isk_state.esp, esp
    }

    /* spin for 1000 cycles per AMD manual requirements:
       Publication No. Revision Date
       24593           3.14     September 2007
       "15.26.8 Secure Multiprocessor Initialization"
       "Software Requirements for Secure MP initialization."
       "...a fixed delay of no more than 1000 processor cycles may be
       necessary before executing SKINIT to ensure reliable sensing of
       APIC INIT state by the SKINIT"
     */
    __asm {
    	mov ebx, 0x3eb
    	myloop:
    	sub ebx, 0x1
    	jnz myloop
    }

    //asm volatile("mov $1000, %%ebx \n"
    //             "1: subl $1, %%ebx \n"
    //             "jnz 1b \n"
    //             : );

    /* CALL SKINIT */
    // skinit opcode is 0x0f01de
    __asm {
    	__emit 0x0f
    	__emit 0x01
    	__emit 0xde
    }

    __asm {
    	resume_target:
    }

    serial_outchar('.');
    serial_outchar('\r');
    serial_outchar('\n');
    dbg("rdtsc is now: %lld", rdtsc64());

    dbg("exit.");
}

#endif // _WIN32

/**
 * END AMD-SPECIFIC FUNCTIONS
 */

int launch_drtm(void) {
    if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
        launch_senter();
    } else {
        amd_do_skinit();
    }

    return 0; /* XXX */
}

