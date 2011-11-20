/* resumeoffsets.h - offset #defines for use in C and assembler
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

#ifndef _RESUMEOFFSETS_H_
#define _RESUMEOFFSETS_H_

/* These are the offsets of members of struct cpu_t declared in
 * resume.h. We need them because the GNU assembler can't access
 * members of structs directly. */

#define CPU_STATE_OFFSET_CS 0x18
#define CPU_STATE_OFFSET_DS 0x1a
#define CPU_STATE_OFFSET_SS 0x22
#define CPU_STATE_OFFSET_CR3 0x3c
#define CPU_STATE_OFFSET_GDT 0x46
#define CPU_STATE_OFFSET_IDT 0x4e
#define CPU_STATE_OFFSET_RETURN_ADDRESS 0x60
#define CPU_STATE_OFFSET_P2V_OFFSET 0x74

#endif /* _RESUMEOFFSETS_H_ */
