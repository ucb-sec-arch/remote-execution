/** asm.S - assembly routines that comprise the Flicker shim and OS
 *  kernel resume code.
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

/* This file contains considerable debug code in the present version.
 * The intention is that it will be easier to figure out where things
 * go wrong if there are problems on hardware that has not previously
 * been tested.
 */

.macro HexPrintAL
#ifndef PERFCRIT
      call  reg2hex
      ror $8, %eax
      push %eax
      call slb_outchar
      pop %eax

      rol $8, %eax
      push  %eax
      call slb_outchar
      pop %eax
#endif
.endm

.macro SayEAX
#ifndef PERFCRIT
    pushal

    push %eax
    rol $8, %eax
    HexPrintAL
    pop %eax

    push %eax
    rol $16, %eax
    HexPrintAL
    pop %eax

    push %eax
    rol $24, %eax
    HexPrintAL
    pop %eax

    push %eax
    HexPrintAL
    pop %eax

    popal
#endif
.endm

.macro SayQ
#ifndef PERFCRIT
    pushal
    mov $0x5f, %eax // underscore, not Q. sue me.
    push %eax
    call slb_outchar
    pop %eax
    popal
#endif
.endm

// this clobbers EAX and EDX
.macro SayAsm
#ifndef PERFCRIT
    mov $0x3fd, %edx // control register for com1
1:  in (%dx), %al    // read status into AL
    test $0x20, %al  // check for ready
    je 1b            // keep trying
    mov $0x3f8, %edx // data register for com1
    mov $0x40, %al   // output an @ character
    out %al, (%dx)
#endif
.endm

.macro SayAsm2
#ifndef PERFCRIT
    mov $0x3fd, %edx // control register for com1
1:  in (%dx), %al    // read status into AL
    test $0x20, %al  // check for ready
    je 1b            // keep trying
    mov $0x3f8, %edx // data register for com1
    mov $0x3f, %al   // output a ? character
    out %al, (%dx)
#endif
.endm

#include "../common/resumeoffsets.h"

/* CPU vendor-specific magic numbers */
#define CPU_VENDOR_INTEL 	0xAB
#define CPU_VENDOR_AMD 		0xCD

#define AMD_STRING_DWORD1 0x68747541
#define AMD_STRING_DWORD2 0x69746E65
#define AMD_STRING_DWORD3 0x444D4163

#define INTEL_STRING_DWORD1	0x756E6547
#define INTEL_STRING_DWORD2	0x49656E69
#define INTEL_STRING_DWORD3	0x6C65746E

#define VM_CR_MSR 0xc0010114
#define VM_CR_DPD 0
#define VM_CR_R_INIT 1
#define VM_CR_DIS_A20M 2

/* various selectors in the GDT */
#define cs_sel                1<<3 /* 0x08 */
#define ds_sel                2<<3 /* 0x10 */
#define ds_offset_sel         3<<3 /* 0x18 */
#define cs_flicker_sel        4<<3 /* 0x20 */
#define ds_flicker_sel        5<<3 /* 0x28 */
#define ss_flicker_sel        6<<3 /* 0x30 */
#define call_gate_flicker_sel 7<<3 /* 0x38 */
#define tss_flicker_sel       8<<3 /* 0x40 */
#define fs_flicker_sel        9<<3 /* 0x48 */

#define PAL_SIZE (0x80000) /* 512 KB - must be consistent with kmod! */
#define ALIGN_4K 0xfffff000

/* misc magic numbers */
#define MLE_HDR_VERSION 0x00020000

#define STACK_TOP 0x10000 /* 64 KB */

    /* Linker script sticks these variables at the end of "low" region.
     * Their purpose is to enable accurate embedding of SLB size. */
    .extern g_end_of_low
    .extern g_aligned_end_of_low

    .section .text.slb
    .global g_pal_zero
g_pal_zero:
PAL_ZERO:
slb_header:   /* AMD-specific 4-byte header */
    .word _pal_start - g_pal_zero   // PAL entry point relative to header (bits 0-15)
    .word g_end_of_low - g_pal_zero // PAL size including this header (bits 31-16)
.globl g_mle_header
g_mle_header:   /* Intel-specific 44-byte header */
  mle_hdr_uuid:
    .long 0x9082AC5A
    .long 0x74A7476F
    .long 0xA2555C0F
    .long 0x42B651CB
  mle_hdr_headerlen:
    .long g_mle_header_end - g_mle_header
  mle_hdr_version:
    .long MLE_HDR_VERSION
  mle_hdr_entry_point:
    .long _pal_start
  mle_hdr_first_valid_page:
    .long 0x0
  mle_hdr_mle_start_off:
    .long 0x0
  mle_hdr_mle_end_off:
    .long g_aligned_end_of_low /* value assigned in linker script! */
  mle_hdr_capabilities:
    .long 0x00000003 /* rlp_wake_{getsec, monitor} = 1 */
.globl g_mle_header_end
g_mle_header_end:

/* enables C-land to have a reliable place to look for base addr */
.globl g_phys_base_addr
g_phys_base_addr:
    .long 0x00000000


    /* lgdt loads the pseudo-descriptor specified by the source
     * operand into the global descriptor table register
     * (GDTR). The pseudo-descriptor is a memory location
     * containing the GDTR base and limit. In legacy and
     * compatibility mode, the pseudodescriptor is 6 bytes.  If
     * the operand size is 32 bits, the lower two bytes specify
     * the 16-bit limit and the upper four bytes specify a 32-bit
     * base address.
     */
pgdt_descr:
           .word end_gdt - gdt - 1 /* pgdt limit */
pgdt_base: .long gdt /* pgdt base; needs FIXUP */
           .word 0 /* word align */

    /* Global Descriptor Table (GDT) */
    /* these need to remain in the same order as the selector #define's */
    gdt:
    _gdt_null:
        .quad 0x0000000000000000
    _cs_sel:
        .quad 0x00cf9b000000ffff
    _ds_sel:
        .quad 0x00cf93000000ffff
    _ds_offset_sel: /* ring 0 offset data segment (FIXUP XXX currently unused) */
        .quad 0x00cf93000000ffff
    _cs_flicker_sel: /* ring 3 code segment (FIXUP) */
        .quad 0x00cffb000000ffff
    _ds_flicker_sel: /* ring 3 data segment (FIXUP) */
        .quad 0x00cff3000000ffff
    _ss_flicker_sel: /* ring 3 stack segment (FIXUP) */
        .quad 0x00cff3000000ffff
    _call_gate_flicker_sel:  /* call gate for ring 3 -> 0 transition */
        .word ring0_reenter-PAL_ZERO /* offset 15-0; (partial FIXUP) */
        .word 0x0008 /* ring 0 CS */
        .word 0xec00 /* P=1,DPL=3,Type=0xc (32-bit call gate), param cnt=0 */
        .word 0x0000 /* offset 31-16, (FIXUP) */
    _tss_flicker_sel: /* TSS to provide SS,ESP for the call gate (FIXUP) */
        .word tss_end-tss_start /* limit 15-0 */
        .word tss_start-PAL_ZERO /* base 15-0 */
        .word 0x8900 /* P=1,DPL=0,Type=0x9 (32-bit available TSS), base 23-16 (FIXUP) */
        .word 0x0010 /* base 31-24 (FIXUP), G=0,AVL=1, limit 19-16 */
    _fs_flicker_sel: /* ring 3 data segment, all of memory */
        .quad 0x00cff3000000ffff
    end_gdt:

    .align
    tss_start: /* 0x48 */
        .word 0x0000 /* Link */
        .word 0x0000 /* IGN */
        .long 0x0000fff8 /* GUESS esp0, needs FIXUP */
        .word 0x0010 /* SS0 */
        .word 0x0000 /* IGN */
        .long 0x0000fff8 /* GUESS esp1, needs FIXUP */
        .word 0x0010 /* SS0 */
        .word 0x0000 /* IGN */
        .long 0x0000fff8 /* GUESS esp2, needs FIXUP */
        .word 0x0010 /* SS0 */
        .word 0x0000 /* IGN */
    tss_end:
_pal_start:
    // SMALL HACK TO TEST CALL 1F-BASED BASE ADDR COLLECTION
    call 1f
1:  popl %eax // put EIP into EAX
    andl $ALIGN_4K, %eax // 4K-align

    movl %eax, %ebp /* SLB base addr */

//    BROKEN IN CROSS-CPU PAL RIGHT NOW
//    movl %edx, %edi /* CPU info */
//    rdtsc
//     /* save base addr of self */
//     pushl %ebp
//     /* save machine info from edx upon entry into SLB */
//     pushl %edi // XXX AMD-specific
//     pushl %eax /* rdtsc low */
//     pushl %edx /* rdtsc high */

    /*********************************************************************
     * Determine processor type to perform some low-level initialization *
     * after DRTM. On AMD CPUs, we need to clear R_INIT, DIS_A20M and    *
     * (optionally) CR_DPD (to enable HDT access) in VM_CR_MSR.          *
     * Store the result in EDX.                                          *
     *********************************************************************/
find_cpu_vendor:
    xor    %eax, %eax
    cpuid
    cmpl $(INTEL_STRING_DWORD1), %ebx
    jne cpu_vendor_notintel
    cmpl $(INTEL_STRING_DWORD2), %edx
    jne cpu_vendor_notintel
    cmpl $(INTEL_STRING_DWORD3), %ecx
    jne cpu_vendor_notintel
    jmp cpu_intel
cpu_vendor_notintel:
    cmpl $(AMD_STRING_DWORD1), %ebx
    jne cpu_vendor_unknown
    cmpl $(AMD_STRING_DWORD2), %edx
    jne cpu_vendor_unknown
    cmpl $(AMD_STRING_DWORD3), %ecx
    jne cpu_vendor_unknown
    jmp cpu_amd
cpu_vendor_unknown:
    /* this should never happen, but we have a fallback just in case */
    ud2
    hlt

    /* If AMD CPU, clear R_INIT and DIS_A20M. Enable HDT debugging iff debug build. */
cpu_amd:
    movl $(VM_CR_MSR), %ecx
    rdmsr
#ifndef PERFCRIT
    andl $(~(1<<VM_CR_DPD)), %eax
#endif
    andl $(~(1<<VM_CR_R_INIT)), %eax
    andl $(~(1<<VM_CR_DIS_A20M)), %eax
    wrmsr

    movl $(CPU_VENDOR_AMD), %edx
    jmp cpu_common

cpu_intel:
    movl $(CPU_VENDOR_INTEL), %edx
    jmp cpu_common

    /* Common code from now on */
cpu_common:

    /**************************************************************************
     * Fixup the GDT pseudo-descriptor's self-referencing physical base addr; *
     * Fixup many entries inside the GDT itself.                              *
     * EBP stores the physical base address of this code.                     *
     **************************************************************************/
    cmpl $(CPU_VENDOR_AMD), %edx
    jne 1f
    addl %ebp, %ss:pgdt_base(%ebp) // AMD
    jmp 2f
    1: addl %ebp, %ds:pgdt_base(%ebp) // Intel
    2:

    /* Create a mask in EAX that can be ORed against GDT entries to dynamically
     * set an appropriate base address. It contains [byte 3] [0] [0] [byte 2]
     * of a segment descriptor's base address. */

    movl $(gdt), %ebx            //grab offset of gdt start
    movl %ebp, %eax              //eax = sl base address
    movl %ebp, %ecx              //ecx = sl base address
    andl $0xFF000000, %eax       //eax = bits 24-31 of sl base address
    andl $0x00FF0000, %ecx       //ecx = bits 16-23 of sl base address
    shrl $16, %ecx               //bits 0-7 of ecx = bits 16-23 of sl base address
    orl %ecx, %eax               //bits 0-7 of eax = bits 16-23 of sl base address
                                 //bits 24-31 of eax = bits 24-31 of sl base address
                                 //eax is now in accordance with the high 32-bits
                                 //of a segment descriptor

    /* Create a mask in ECX that can be ORed against GDT entries to dynamically
     * set an appropriate base address.  It contains [byte 1] [byte 0] [0] [0]
     * of a segment descriptor's base address. */
    movl %ebp, %ecx
    shll $16, %ecx
    andl $0xffff0000, %ecx

    /* Update the ring 3 code, data, and stack descriptors by OR-ing in
       portions of PAL's physical base address.  ECX contains a mask for the
       low 32 bits of a descriptor, and EAX for the high 32-bits. */
    cmpl $(CPU_VENDOR_AMD), %edx
    jne 1f
    orl %eax, %ss:(cs_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ss:(cs_flicker_sel  )(%ebp, %ebx)
    orl %eax, %ss:(ds_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ss:(ds_flicker_sel  )(%ebp, %ebx)
    orl %eax, %ss:(ss_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ss:(ss_flicker_sel  )(%ebp, %ebx)
    jmp 2f
    1:
    orl %eax, %ds:(cs_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ds:(cs_flicker_sel  )(%ebp, %ebx)
    orl %eax, %ds:(ds_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ds:(ds_flicker_sel  )(%ebp, %ebx)
    orl %eax, %ds:(ss_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, %ds:(ss_flicker_sel  )(%ebp, %ebx)
    2:

    /* Load the GDT and segment selectors */
    lgdt %cs:pgdt_descr(%ebp)
    movw $ds_sel, %dx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs
    mov %dx, %ss

    /* Setup ESP to top of 64K (it will already be there on AMD) */
    movl $STACK_TOP, %esp
    addl %ebp, %esp

    /* populate global variable with physical base address */
    movl %ebp, g_phys_base_addr(%ebp)

    /***********************************************************
     * Remaining updates are CPU-agnostic since DS and SS have *
     * been reloaded.                                          *
     ***********************************************************/

    /* Repeat for tss_flicker_sel (tss descriptor). */
    orl %eax, (tss_flicker_sel+4)(%ebp, %ebx)
    orl %ecx, (tss_flicker_sel  )(%ebp, %ebx) // XXX should we 16-bit add instead?

    /* Update the Call Gate Descriptor in the GDT; needs
       Target Code-Segment Offset base addr bits 31..16 updated */
    movl $(gdt), %ebx // grab offset of gdt start
    movl %ebp, %eax   // get physical base address
    shrl $16, %eax    // shift 16 bits to the right (we need bits 31..16)
    mov %ax, (call_gate_flicker_sel+6)(%ebp, %ebx) // overwrite bits 31..16 of target CS offset
    /* Now update bits 15..0; note that there is existing data in these bits
       that should be preserved, so we add to it. */
    movl %ebp, %ecx
    andl  $0x0000ffff, %ecx // keep low 16 bits
    addw %cx, (call_gate_flicker_sel)(%ebp, %ebx)

    /* Update TSS entries; ESP values in TSS need accurate base address */
    movl $(tss_start+4), %ebx   // grab offset of tss start, bypassing [link/ign]
    // %ax already contains bits 31..16 of physical base addr
    mov %ax,  0x2(%ebp, %ebx) // esp0 base addr bits 31..16 [used when call gate called]
    mov %ax,  0xa(%ebp, %ebx) // esp1 base addr bits 31..16 [unused]
    mov %ax, 0x12(%ebp, %ebx) // esp2 base addr bits 31..16 [unused]

    /* TODO: complete the timing of the DRTM invocation */

#ifndef PERFCRIT
    /* Setup 115200,8n1 on ttyS0 */
    call slb_serial_init
    SayAsm // 1 (before slb_outlong)

    /* output the base physical address of the SLB */
    call 1f
    1: pop %eax
    andl $ALIGN_4K, %eax
    pushl %eax
    call slb_outlong
    addl $0x4, %esp  /* pop w/o data */
#endif

    /* set IOPL to 3 by setting bits 13-12 in eFLAGS */
    /* This allows serial I/O and timer usage (tis.c) */
    /* TODO: finer granularity of control! */
    pushfl
    popl %eax
    orl $0x00003000, %eax
    pushl %eax
    popfl

    SayAsm // 2

    /* Load TR now, though it is not used until ring 3 exits */
    movw $(tss_flicker_sel), %ax
    ltr %ax

    SayAsm // 3

    movl  %ss:g_phys_base_addr(%ebp), %eax
    pushl %eax
    call slb_outlong
    popl %eax
    movl %ds:g_phys_base_addr(%ebp), %eax
    pushl %eax
    call slb_outlong
    popl %eax

    movl $(gdt), %ebx
    movl %ds:(ds_flicker_sel)(%ebp, %ebx), %eax /* ring 3 data */
    pushl %eax
    call slb_outlong
    popl %eax
    movl $(gdt), %ebx
    movl %ds:(ds_flicker_sel+4)(%ebp, %ebx), %eax /* ring 3 data */
    pushl %eax
    call slb_outlong
    popl %eax

    /* Load ring 3 DS */
    movw $(0x00000003+ds_flicker_sel), %ax
    mov %ax, %ds
    mov %ax, %es

    xor %eax, %eax
    movl %ds, %eax
    pushl %eax
    call slb_outlong
    popl %eax

    movl $(gdt), %ebx
    movl %ds:(ds_flicker_sel)(%ebx), %eax /* ring 3 data */
    pushl %eax
    call slb_outlong
    popl %eax
    movl $(gdt), %ebx
    movl %ds:(ds_flicker_sel+4)(%ebx), %eax /* ring 3 data */
    pushl %eax
    call slb_outlong
    popl %eax

    /* Load ring 3 DS, ES */
    movw $(0x00000003+ds_flicker_sel), %ax
    mov %ax, %ds
    mov %ax, %es

    /* Load all-of-memory ring 3 FS */
    movw $(0x00000003+fs_flicker_sel), %ax
    mov %ax, %fs

    /* iret (far) to ring 3 CS and SS */
    pushl $(0x00000003+ss_flicker_sel) /* ring 3 SS */
    pushl $0x0000ff80 /* ESP: 64KB - 128 bytes */
    pushf /* current eFLAGS should be fine for ring 3 */
    pushl $(0x00000003+cs_flicker_sel) /* ring 3 CS */
    call 1f /* push EIP of next instruction onto stack */
1:  addl $0xc, (%esp) /* set target EIP to beyond iret */
    andl $0x00000fff, (%esp) /* strip off slb base addr */
    /* Pop the last 5 dwords into the appropriate places
     * and put us in ring 3 */
    iret

    SayAsm // 4

//     /* Load ring 3 DS */
//     movw $(ds_flicker_sel), %ax
//     //orw $0x3, %ax // ring 3
//     mov %ax, %ds
//     mov %ax, %es

    SayAsm // 5

    /* Do the work! */
    movl $PAL_SIZE, %eax /* offset to params (XXX) */
    pushl %eax
    call slb_dowork
    addl $0x4, %esp /* pop w/o data */

    SayAsm // 1.

    /* Go back to ring 0 via far call using call gate */
    pushw $(0x0003+call_gate_flicker_sel) /* call gate offset in GDT, RPL=3 */
    call 1f /* push EIP, though it gets ignored */
1:  lcall   *(%esp) /* control xfer through call gate -> ring0_reenter */


ring0_reenter: /* This is the target of the call gate */

    SayAsm2 // 2.

    // Intel-only GETSEC[SEXIT]
    call get_cpu_vendor // return value winds up in EAX
    cmpl $CPU_VENDOR_INTEL, %eax
    jne 1f // don't do this on AMD
    movl $0x5, %eax
    getsec
1:

    SayAsm2 // 3.

    /* Load ring 0 DS and ES */
    movw $(ds_sel), %ax
    mov %ax, %ds
    mov %ax, %es

    SayAsm // 4.

    /* Pop 4 32-bit values off stack (SS, ESP, CS, EIP from before lcall) */
    addl $0x10, %esp

    SayAsm2 // 5.

    /* reload EBP with physical base addr of PAL */
    call 1f
1:  popl %ebp
    andl $ALIGN_4K, %ebp

    SayAsm // 6.

    /* Calculate the physical base address of the struct cpu_state
     * which contains kernel's saved values */
    movl %ebp, %esi      /* physical base addr of PAL */
    addl $PAL_SIZE, %esi /* advance to stored cpu_state */ /* XXX TODO : use actual size */

    /* turn on paging!!! */
    movl %ebp, %eax /* base physical address of the SLB */
    //addl $0x0000c000, %eax /* 0xc000 = SLB_PT_OFFSET */
    addl $0x000b9000, %eax /* 0xb9000 = kmod/resume.c's pagetab's ; XXX TODO: figure out automagically */
    movl %eax, %cr3 /* write CR3 */

    movl %cr0, %eax
    orl $0x80000000, %eax /* CR0.PG = 1 (bit 31) */
    movl %eax, %cr0
    jmp 1f

    /* Relocate eip and esp into virtual memory space */
1:
    movl CPU_STATE_OFFSET_P2V_OFFSET(%esi), %eax
    movl %eax, %ebx /* copy value */
    movl %eax, %ecx /* copy value */
    pushw $(cs_sel)
    call 1f /* top of stack now contains "&here" */
1:
here:
    addl $(target-here), %eax
    addl %eax, (%esp) /* increase ljmp target by P2V_OFFSET */
    /* (%esp): contains ljmp target addr */
    ljmp *(%esp)
target:
    addl %ebx, %esp /* relocate ESP by P2V_OFFSET */
    /* pop 6 byte ljmp descriptor */
    addl $0x6, %esp

    /* now we are executing in virtual memory space which
     * corresponds with that of the kernel's page tables. */

    SayQ /* 1 */

    /* Calculate the base address of the struct cpu_state which
     * contains kernel's saved values */
    movl %ebp, %esi      /* physical base addr of PAL */
    addl %ecx, %esi      /* offset to virtual base addr of PAL */
    addl $PAL_SIZE, %esi /* advance to stored cpu_state */ /* XXX TODO : use actual size */

    /* Load the kernel's original interrupt descriptor table */
    movl %esi, %ebx /* start addr of struct cpu_state */
    addl $CPU_STATE_OFFSET_IDT, %ebx
    lidt (%ebx)

    SayAsm
    movl  %cr4, %eax
    // Enable PSE (bit 4 of CR4) since OS page tables may use page size extensions
    orl $0x10, %eax
    movl %eax, %cr4

    /* reload CR3 with the kernel's original value */
    movl %esi, %ebx /* start addr of struct cpu_state */
    movl CPU_STATE_OFFSET_CR3(%ebx), %eax
    movl  %eax, %cr3
    SayAsm // print something without touching stack or data

    /* -------------------------------------------------
     * Reload GDTR with the kernel's original GDT.
     */
    movl %esi, %ebx /* start addr of struct cpu_state */
    addl $CPU_STATE_OFFSET_GDT, %ebx /* addr of OS's gdt_limit:gdt_base */
    lgdt (%ebx)
    /* ------------------------------------------------- */


    /* -------------------------------------------------
     * Load segment descriptors with kernel values */
    movl %esi, %ebx /* start addr of struct cpu_state */
    movw CPU_STATE_OFFSET_CS(%ebx), %bx
    /* cannot mov to cs; must use jmp far (6-byte cs:address) */
    pushw %bx
    call 1f /* top of stack now contains "&here2" */
1:
here2:
    /* add to eip to compute addr of next instr after ljmp */
    addl  $(target2-here2), (%esp)
    ljmp *(%esp)
target2:
    addl $0x4, %esp

    /* lss loads ss and esp atomically */
    movl %esi, %ebx /* start addr of struct cpu_state */
    movw CPU_STATE_OFFSET_SS(%ebx), %bx
    pushw %bx

    pushl %esp
    lss (%esp), %esp
    addl $0x6, %esp

    movl %esi, %ebx /* start addr of struct cpu_state */
    movw CPU_STATE_OFFSET_DS(%ebx), %bx
    movw %bx, %ds

    /* -------------------------------------------------
     * restore return_address
     */
    movl %esi, %ebx /* start addr of struct cpu_state */
    movl CPU_STATE_OFFSET_RETURN_ADDRESS(%ebx), %eax

    jmp *%eax


//-- //-- EXPERIMENTAL INTERRUPT STUFF --------------
//-- //-- Making this position independent is a headache.
//-- int_handler:
//-- .rept 16
//--         SayAsm
//-- .endr
//--         ud2

//-- idt_descr:
//--         .word   idt_table_end - idt_table - 1
//--         .long   idt_table
//--         .word   0
//-- idt_table:
//--         .rept 256
//--                 .word   int_handler - PAL_ZERO /*_start*/
//--                 .word   0x0008 /* CS */
//--                 .word   0x8e00   /* present, DPL=0, 32b, interrupt */
//--                 .word   (int_handler - _start) >> 16
//--         .endr
//-- idt_table_end:


//-- DEBUG STUFF -------------------------------
reg2hex:      //    convert byte in al to ascii equivalent in AX, e.g., 0x12 -> 0x3132
    mov %al, %ah // duplicate byte
    and $0x0f, %al // keep only low nibble
    shr $4, %ah  // shift right to access high nibble
    and $0x0f, %ah // keep only low nibble
low:
    cmp $0xa, %al
    jge lowletter
lowdigit:
    add $0x30, %al // convert digit from numerical value to ASCII
    jmp high
lowletter:
    add $0x57, %al // convert digit from numerical value to ASCII

high:
    cmp $0xa, %ah
    jge highletter
highdigit:
    add $0x30, %ah
    ret
highletter:
    add $0x57, %ah // - 0xa + 'a'
    ret

nomoredebug:
    jmp nomoredebug

    /*
     * Quick and dirty timing test
     * Place first snippet before, and second snippet after
     * Place guess as to how long it took in literal
     * Make sure intervening code doesn't clobber ecx
     * and isn't expecting anything to be in eax or edx
     */
    /*
    rdtsc
    mov %eax, %ecx
    */
    /*
    rdtsc
    subl %ecx, %eax
    cmp $0x1c9c380, %eax
    jl 1f
    hlt
1:  xor %eax, %eax
    */


//-- END DEBUG STUFF ---------------------------
