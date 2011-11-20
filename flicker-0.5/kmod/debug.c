/*
 *  debug.c: Various debugging/dumping functions
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

#define DUMP_BUF_SIZE 2048

#ifndef _WIN32

#define malloc(x) kmalloc((x), GFP_KERNEL)
#define free(x) kfree((x))

#else  // _WIN32

#include "Globals.h"
#include "ntstrsafe.h"
#define malloc(x) ExAllocatePoolWithTag(NonPagedPool, (x), FLICKER_POOL_ALLOCATION_TAG)
#define free(x) ExFreePoolWithTag((x), FLICKER_POOL_ALLOCATION_TAG)
#define snprintf(str, size, format, ...) RtlStringCchPrintfA((str), (size), (format), __VA_ARGS__)

#endif // _WIN32

void dump_bytes(unsigned char *bytes, int len) {
  static char buf[DUMP_BUF_SIZE];
  int i=0, j=0;
  char *ptr = buf;
  while (i < len) {
    for (j=0; (j < 15) && (i < len); j++, i++)
      ptr += snprintf(ptr, (DUMP_BUF_SIZE - (ptr-buf)), "%02x ", bytes[i] & 0xff);
    ptr += snprintf(ptr, (DUMP_BUF_SIZE - (ptr-buf)), "\n");
    if((ptr-buf) >= DUMP_BUF_SIZE) break;
  }
  logit("%s", buf);
}

/**
 * Dump MLE page tables to debug output.
 */
void dump_mle_page_table(unsigned char *location, int len){
  dbg("dump page directory entry @ %p", location);
  dump_bytes(location, sizeof(uint64_t));

  dbg("dump page table entry @ %p", location+PAGE_SIZE);
  dump_bytes(location+PAGE_SIZE, sizeof(uint64_t));

  dbg("dump page table @ %p", location+2*PAGE_SIZE);
  dump_bytes(location+2*PAGE_SIZE, 8*20);
}

/**
 * Dump a page table page to debug output. The argument is assumed to
 * be a 4096 byte memory region which contains page table entries.
 */
void dump_page_table_page(char *label, void *page)
{
  char *buf;
  char *ptr;
  int i=0, j=0;
  int len=4096/4; /* how many words of page table to print */

  buf = malloc(4*len);
  if(NULL == buf) { return; }
  ptr = buf;

  while (i < len) {
    for (j=0; (j < 4) && (i < len); j++, i++)
      ptr += snprintf(ptr, (4*len - (ptr-buf)), "%08lx ", ((unsigned long *)page)[i]);
    dbg("%s %08lx+%x: %s", label, (unsigned long)page, i-4, buf);
    ptr = buf;
  }
  free(buf);
}

/**
 * Dump saved processor state to debug output
 */
void dump_state(cpu_t *s) {
  if(s == NULL) { return; }

  dbg("eax %08lx, ebx %08lx, ecx %08lx, edx %08lx, edi %08lx, esi %08lx",
      s->eax, s->ebx, s->ecx, s->edx, s->edi, s->esi);

  dbg("cs %04x, ds %04x, es %04x, fs %04x, gs %04x, ss %04x",
      s->cs, s->ds, s->es, s->fs, s->gs, s->ss);

  dbg("eflags %08lx, efer %08lx, esp %08lx, ebp %08lx",
      s->eflags, s->efer, s->esp, s->ebp);

  dbg("cr0 %08lx, cr2 %08lx, cr3 %08lx, cr4 %08lx",
      s->cr0, s->cr2, s->cr3, s->cr4);

  dbg("gdt_base:gdt_limit %08lx:%04x, idt_base:idt_limit %08lx:%04x",
      s->gdt_base, s->gdt_limit, s->idt_base, s->idt_limit);

  dbg("ldt %04x, tss %04x, tr %08lx, safety %08lx, return_address %08lx",
      s->ldt, s->tss, s->tr, s->safety, s->return_address);

  dbg("addr of isk_state.ebp: 0x%08x, esp: 0x%08x",
      (uint32_t)&(s->ebp), (uint32_t)&(s->esp));
}

void printbitdiffs(unsigned long orig, unsigned long curr, char *id) {
  unsigned long x;
  int n;
  char buf[42]; /* 32 bits + 8 spaces + NULL*/
  memset(buf, '\0', 42);

  x = orig ^ curr;
  buf[0] = '\0';

  for(n=0; n<32; n++)
  {
    if((x & 0x80000000) !=0) { strncat(buf,"1",1); }
    else { strncat(buf,"0",1); }

    /* insert a space between nibbles */
    if (n%4==3) { strncat(buf," ",1); }

    x = x<<1;
  }

  dbg("\ndiff for %s: %s", id, buf);
}

void print_stack_around(void){
    unsigned char *ptr;
    dbg("print_stack_around\n");
    ptr = (unsigned char*) get_esp();
    dbg("%p\n", ptr);
    dump_bytes(ptr, 0x80);
}

/* print details on the translation of a virtual address to a physical
 * address assuming CR3 contains a Page-Table Base Address of cr3val. */
void virt_through_pt(unsigned long cr3val, unsigned long virt)
{
    /* page directory base */
    unsigned long pdb;
    /* page directory entry, page table entry, physical address */
/*     unsigned long pde, pte, pa; */
    /* page directory offset, page-table offset, page offset */
    unsigned long pdo, pto, po;

    pdo = (virt >> 22) & 0x3ff;
    pto = (virt >> 12) & 0x3ff;
    po = virt & 0xfff;

    dbg("virt decodes into PDO %03lx, PTO %03lx, PO %03lx",
        pdo, pto, po);

    pdb = cr3val & 0xfffff000; /* strip off flags */
    dbg("cr3val %08lx yields pdb %08lx, PCD=%ld, PWT=%ld", cr3val, pdb, cr3val & 0x00000010, cr3val & 0x00000004);
}

