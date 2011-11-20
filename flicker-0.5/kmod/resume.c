/*
 * resume.c: Untrusted helpers for resuming OS after PAL execution.
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

#include "flicker.h"

/**
 * Build the page tables that will be used initially when the PAL
 * re-enables paging.  PAGE_SIZE is assumed to be 4096. There are two
 * sets of page tables: one that is unity-mapped and one with virtual
 * addresses adjusted to the relevant address space in the OS kernel
 * (e.g., a constant offset of 0xC0000000 in Linux).
 *
 * 'target' is the 4KB-aligned beginning of the space that we wish to map.
 * 'pagetabs' is the 4KB-aligned beginning of 3 4KB pages that will contain
 * the actual paging structures.
 */
void build_resume_page_tables(void *pal, void *pagetabs) {
    int i;
    uint32_t phys;
    uint32_t virt;

    uint32_t pdt;       /* Page-Directory Table */
    uint32_t pt_direct; /* Page Table containing direct-mapped addresses */
    uint32_t pt_plusc;  /* Page Table containing virtual addresses */

    dbg("KPT build_kernel_page_tables: pal=0x%p, pagetabs=0x%p", pal, pagetabs);

    pdt =       (uint32_t) pagetabs;          /* virtual address of pdt */
    pt_direct = (uint32_t) pagetabs + 0x1000; /* virtual address of pt_direct */
    pt_plusc =  (uint32_t) pagetabs + 0x2000; /* virtual address of pt_plusc */

    /* virtual, physical addresses of MLE */
    virt = (uint32_t) pal;
    phys = (uint32_t) v2p(virt);

    dbg("KPT pdt=%08x, pt_direct=%08x, pt_plusc=%08x", pdt, pt_direct, pt_plusc);

    if(!virt || ((virt & 0xfffff000) != virt) ||
       !pdt  || ((pdt  & 0xfffff000) != pdt)) {
        error("KPT virt/pdt ALIGNMENT FAILURE");
        return;
    }

    memset((void *)pdt, 0, PAGE_SIZE);
    memset((void *)pt_direct, 0, PAGE_SIZE);
    memset((void *)pt_plusc, 0, PAGE_SIZE);

    /* unity PAL entry in PDT */
    *((uint32_t *)(pdt + 4*(phys >> 22))) =
        (v2p(pt_direct) & 0xfffff000)
        | _PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_PCD;
    dbg("KPT pdt=%08x @ %08x",
        v2p(pt_direct) & 0xfffff000,
        (pdt + 4*(phys >> 22)));

    /* PAGE_OFFSET PAL entry in PDT */
    *((uint32_t *)(pdt + 4*(virt >> 22))) =
        (v2p(pt_plusc) & 0xfffff000)
        | _PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_PCD;
    dbg("KPT pt_direct=%08x @ %08x",
        v2p(pt_plusc) & 0xfffff000,
        pdt + 4*(virt >> 22));

    /* 256K including page tables for ACMod, MLE, and page tables for kernel restart */
    /* PAGE_SIZE is 4K, so there are 64 entries. We really only need to map the page
       where asm.S resides, but mapping more gives us greater access to debug output
       during development. */
    for(i=0; i<256; i++) {
        uint32_t phys_offset, virt_offset;
        phys_offset = 4*(((phys+i*PAGE_SIZE) >> 12) & 0x3ff);
        virt_offset = 4*(((virt+i*PAGE_SIZE) >> 12) & 0x3ff);
        /* dbg("offset in loop: phys 0x%08x, virt 0x%08x", phys_offset, virt_offset); */

        /* unity SLB PTEs */
        *((uint32_t *)(pt_direct + phys_offset)) =
            ((phys + i*PAGE_SIZE) & 0xfffff000)
            | _PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_PCD;
        /* PAGE_OFFSET SLB PTEs */
        *((uint32_t *)(pt_plusc + virt_offset)) =
            ((phys + i*PAGE_SIZE) & 0xfffff000)
            | _PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_PCD;
    }

    dbg("dumping bytes at %p, %p, %p, %p",
        (char*)pagetabs + 4*(phys >> 22),
        (char*)pagetabs + 4*(virt >> 22),
        (char*)pagetabs + 0x1000 + 4*((phys >> 12) & 0x3ff),
        (char*)pagetabs + 0x2000 + 4*((phys >> 12) & 0x3ff));

    dump_bytes((char*)pagetabs + 4*(phys >> 22), 16);
    dump_bytes((char*)pagetabs + 4*(virt >> 22), 16);
    dump_bytes((char*)pagetabs + 0x1000 + 4*((phys >> 12) & 0x3ff), 32);
    dump_bytes((char*)pagetabs + 0x2000 + 4*((phys >> 12) & 0x3ff), 32);

}
