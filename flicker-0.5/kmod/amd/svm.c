/*
 * svm.c: AMD Secure Virtual Machine helpers
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

#ifndef _WIN32
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#else  // _WIN32
#include "wintypes.h"
#endif // _WIN32

#include "flicker.h"
#include "svm.h"
#include "log.h"


#ifndef _WIN32
/**
 * For AMD Microcode Clearing, see also:
 * smp_call_function_single()
 * smp_call_function_many()
 * rdmsr_on_cpu()
 * wrmsr_on_cpu()
 *
 * http://www.fsl.cs.sunysb.edu/kernel-api/re68.html
 * http://stackoverflow.com/questions/3033471/processor-affinity-settings-for-linux-kernel-modules
 * http://www.ibm.com/developerworks/linux/library/l-affinity.html#download
 */

DEFINE_SPINLOCK(microcode_update_lock);

static int clear_current_cpu(void) {
    uint32_t id=1, apicid=0, dummy=0;
    unsigned long flags=0;
    apicid = cpu_data(smp_processor_id()).apicid;

    if(smp_processor_id() != apicid) {
      logit("WARNING: apicid(%d) != smp_processor_id(%d)",
         apicid, smp_processor_id());
      /* TODO: Figure out if this is a fatal problem */
      /* ... and if so return -1 */
    }
    logit("CPU(%d): apicid = %d", smp_processor_id(), apicid);

    rdmsr(0x0000008B, id, dummy); /* get patch id */
    logit("CPU(%d): current microcode patch id %08x", smp_processor_id(), id);

    if (id != 0) {
        spin_lock_irqsave(&microcode_update_lock, flags);
        /* XXX Why is this called twice? */
        asm volatile("wrmsr" :: "c" (0xc0010021)); /* clear patch */
        wrmsrl(0xc0010021, (u64)0);
        rdmsr(0x0000008B, id, dummy); /* get patch id */
        spin_unlock_irqrestore(&microcode_update_lock, flags);
        logit("CPU(%d): cleared microcode patch to %08x", smp_processor_id(), id);
    } else
        logit("current patch id is 0, nothing to clear");

    return 0;
}

static void do_ucode_clear(void *info) {
  int rv;

  rv = clear_current_cpu();
  if(0 != rv) {
    error("CPU(%d): ERROR %d clearing microcode", smp_processor_id(), rv);
  }
}

static void ucode_clear_all(void *info) {
  unsigned int i;
  int rv;
  const int wait = 1;

  logit("CPU(%d): preparing to clear microcode on all CPUs",
     smp_processor_id());

  /* go through CPUs one at a time and clear their microcode */
  for(i=0; i<num_online_cpus(); i++) {
    if(i == smp_processor_id()) {
      /* just call the function directly on the local CPU */
      do_ucode_clear(&info);
    } else {
      rv = smp_call_function_single(i, do_ucode_clear, &info, wait);
      if(0 != rv) {
        error("CPU(%d): ERROR: Attempt to call function on CPU %d FAILED (%d)",
          smp_processor_id(), i, rv);
      }
    }
  }
}

int do_amducodeclear(void)
{
  unsigned int info = 0xf00d0000; /* meaningless */

  logit("CPU(%d): do_amducodeclear invoked ------------",
      smp_processor_id());

  /* This module is specific to AMD CPUs */
  assert(get_cpu_vendor() == CPU_VENDOR_AMD);

  /* DRTM won't work if we can't clear microcode on all CPUs */
  if(num_online_cpus() != num_possible_cpus()) {
    logit("WARNING: num_online_cpus(%d) != num_possible_cpus(%d)\n"
           "UNDEFINED BEHAVIOR MAY RESULT",
           num_online_cpus(), num_possible_cpus());
  //return -1;  Keep going for now
  } else {
    logit("num_online_cpus() = num_possible_cpus() = %d",
       num_online_cpus());
  }

  /* Clear microcode on all CPUs */
  ucode_clear_all(&info);

  return 0;
}

#else  // _WIN32
static int clear_current_cpu(void) {
    return 0;
}

static void do_ucode_clear(void *info) {
    ;
}

static void ucode_clear_all(void *info) {
    ;
}

int do_amducodeclear(void) {
    return 0;
}

#endif // _WIN32
