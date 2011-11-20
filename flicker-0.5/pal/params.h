/* params.h - definitions for params.c
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

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include <stdint.h>

#include "util.h"
#ifdef TRUSTSIM
#include "resume.h" /* cpu_t */
#else
#include "../common/resume.h" /* cpu_t */
#endif

/* Dependent on layout of 'struct pal_descriptor' in ../kmod/flicker.h
   TODO: Use a common .h file to remove this cross-dependency. */
#define MAX_INPUT_PARAM_SIZE (0x1d * PAGE_SIZE - sizeof(cpu_t))
#define MAX_OUTPUT_PARAM_SIZE (0x1c * PAGE_SIZE)

extern char outputbuf[MAX_OUTPUT_PARAM_SIZE];

#define PARAMETER_TYPE_LOG_ENTRY              1
#define PARAMETER_TYPE_TIMING_INFO            2

/* Initialize this code with the memory location where the params are
 * stored. */
int pm_init(void *inbuf, int inbuf_length, void *outbuf, int outbuf_length);


/****** PARAMETERS OUTPUT FROM MLE ******/

/* return amount of output space remaining for a single additional
 * output param (result subtracts bytes needed for metadata, i.e.,
 * result is amount of _data_ that can fit) */
int pm_avail(void);

/* append some data to the output params */
int pm_append(int paramType, char *paramData, int paramSize);

/* reserve space for an output parameter */
char *pm_reserve(int paramType, int paramSize);


/****** PARAMETERS INPUT TO MLE ******/

/* get a pointer to param number which in-place.  this way app code
 * doesn't need to do an extra memory copy */
int pm_get_addr(int param_type, char **data);

#endif /* _PARAMS_H_ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
