/*
 * flickermod.c: Main file for Flicker kernel module
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
#include "sysfs.h"
#include "txt.h"
#include "mtrrs.h"
#include "svm.h"

/* region of memory to hold saved mtrrs and other kernel-restored cpu
 * state. Accessed here and in intel/txt.c. */
extern kcpu_state_t kcpu_region;

extern acmod_t *g_acmod;
extern size_t g_acmod_size;

/* container of g_pal and (if Intel CPU detected) g_mle_ptab */
static void *g_pal_region = NULL;


#define MODULE_NAME "flicker"

static int do_allocations(void) {
    uint32_t needed_alloc_size = sizeof(pal_t);

    assert(NULL == g_pal_region);
    assert(NULL == g_pal);
    assert(NULL == g_mle_ptab);
    assert(NULL == g_acmod);

    dbg("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");

    if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
        dbg("Intel CPU detected; doing additional allocation for ACMod and MLE PTs");
        /* We need more space on Intel for the MLE page tables. */
        needed_alloc_size += sizeof(mle_pt_t);

        /* We need a buffer into which the Authenticated Code module
         * can be stored.  From MLE developer's guide: "System
         * software should not utilize the memory immediately after
         * the SINIT AC module up to the next 4KByte boundary."  We
         * get this for free since kmalloc uses __get_free_pages
         * internally, and always returns 4K-aligned chunks.
         *
         * XXX TODO: Copy it directly into the TXT-specified location
         * without additional buffering. (i.e., the above comment is
         * irrelevant since the AC Mod is copied again before use. XXX
         * is it?) */
        g_acmod = (acmod_t*)kmalloc(sizeof(acmod_t), GFP_KERNEL);

        if(g_acmod == NULL) {
            error("alloc of 0x%08x bytes failed!", sizeof(acmod_t));
            return -ENOMEM;
        }

        dbg("alloc of 0x%08x bytes for acmod at virt 0x%08x.", sizeof(acmod_t), (uint32_t)g_acmod);
        dbg("g_mle_ptab @ 0x%p", g_mle_ptab);
        dbg("->pdpt  @ 0x%p", g_mle_ptab->pdpt);
        dbg("->pd    @ 0x%p", g_mle_ptab->pd);
        dbg("->pt    @ 0x%p", g_mle_ptab->pt);

    }

    dbg("PAGE_SIZE = 0x%08lx (%ld)", PAGE_SIZE, PAGE_SIZE);
    dbg("sizeof(pal_t)= 0x%08x (%d)", sizeof(pal_t), sizeof(pal_t));

    g_pal_region = kmalloc(needed_alloc_size, GFP_KERNEL);

    if(g_pal_region == NULL) {
        error("alloc of %d bytes failed!", needed_alloc_size);
        if(g_acmod) { kfree(g_acmod); g_acmod = NULL; }
        return -ENOMEM;
    }

    dbg("alloc of %d bytes at virt 0x%08x.",
        needed_alloc_size, (uint32_t)g_pal_region);

    /* Verify that we have at least a 128K aligned block */
    if((unsigned long)(g_pal_region) != ((unsigned long)(g_pal_region) & ALIGN_128K)) {
        error("ERROR: memory not aligned!");
        kfree(g_pal_region); g_pal_region = NULL;
        if(g_acmod) { kfree(g_acmod); g_acmod = NULL; }
        return -ENOMEM;
    }

    /* zero the PAL container */ /* slow? necessary? */
    memset(g_pal_region, 0, needed_alloc_size);

    if(needed_alloc_size > sizeof(pal_t)) {
        /* Intel system; assign g_mle_ptab, then g_pal */
        g_mle_ptab = (mle_pt_t*)g_pal_region;
        g_pal = (pal_t*)(g_pal_region + sizeof(mle_pt_t));
    } else {
        /* AMD system; just assign g_pal */
        g_pal = (pal_t*)g_pal_region;
    }

    dbg("g_pal           @ 0x%p", g_pal);
    dbg("g_pal->pal      @ 0x%p", g_pal->pal);
    dbg("&g_pal->reload  @ 0x%p", &(g_pal->reload));
    dbg("g_pal->inputs   @ 0x%p", g_pal->inputs);
    dbg("g_pal->outputs  @ 0x%p", g_pal->outputs);

    build_resume_page_tables(g_pal->pal, g_pal->resume_pagetabs);

    return 0;
}

static void free_allocations(void) {
    if(g_acmod) {
        kfree(g_acmod);
        g_acmod = NULL;
    }
    if(g_pal_region) {
        if(g_pal) { memset(g_pal, 0, sizeof(pal_t)); } /* slow? */
        kfree(g_pal_region);
        g_pal_region = NULL;
        g_pal = NULL;
        g_mle_ptab = NULL;
    }
}

static int __init init_flicker(void)
{
  int rv = 0;
  logit("Flicker module initializing.");

  if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
      /* Don't bother to load the module if the platform does not support
       * the TXT extensions.
       *
       * TODO: Is this a better home for the checks in prepare_for_launch?
       */
      rv = txt_verify_platform();
      if (!rv) {
          error("Intel Platform that does not support TXT extensions.");
          return rv;
      }
  } else {
      /* On AMD, we need to clear the Microcode on all CPUs. This
       * introduces the requirement that this module is loaded before
       * CPU hotplug disables all the other CPUs, since otherwise they
       * won't all be cleared. */
      do_amducodeclear();
  }

  /* allocate memory for PAL and (on Intel) ACmod */
  rv = do_allocations();
  if (rv) {
    error("Error during alloc_pal(): rv = [%d]", rv);
    return rv;
  }

  /* Initialize sysfs entries */
  rv = flicker_sysfs_init();
  if(rv) {
    error("Error initializing sysfs: rv = [%d]", rv);
    free_allocations();
    return rv;
  }

  assert(0 == rv);
  return rv;
}

static void __exit cleanup_flicker(void)
{
  flicker_sysfs_cleanup();

  free_allocations();

    /* Check the MTRRs before we leave */
    if ( !validate_mtrrs(&(kcpu_region.saved_mtrr_state)) ) {
       dbg("MTRRs failed validation");
    }

  logit("Flicker module unloaded.");
}

module_init(init_flicker);
module_exit(cleanup_flicker);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Jonathan M. McCune, Hiroshi Isozaki, and Ed Schwartz");
MODULE_DESCRIPTION("Flicker minimal TCB code execution (Intel).");
