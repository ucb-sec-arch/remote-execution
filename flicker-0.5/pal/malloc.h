/**
 * malloc.h - Provide memory allocation / freeing to the PAL.
 *
 * Copyright (C) 2006-2009 Bryan Parno
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

#ifndef _MALLOC_H_
#define _MALLOC_H_

#include <stdint.h>
#include <stddef.h>
#ifndef _WIN32
#include <stdbool.h>
#else  // _WIN32
#include "wintypes.h"
#endif // _WIN32

/*
 * Maximum number of slots that can be allocated at one time
 */
//#define MEMORY_BUFFER_SIZE 2048
#define MEMORY_BUFFER_SIZE 25000
//#define MEMORY_BUFFER_SIZE 10000

/*
 * Number of bytes in each slot
 * Total memory that can theoretically be allocated at any time is:
 *   MEMORY_BUFFER_SIZE * MEMORY_SLOT bytes
 */
#define MEMORY_SLOT_SIZE 4

/* Programmers get to define the meaning of truth */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Bitset definitions */
#define Bitslot unsigned int
#define Bitset Bitslot *

/* Number of bits in a bitset slot
 * Currently, we use unsigned ints, so there are 32 bits/slot */
#define BITSET_SLOT_SIZE 32

#define BITSET_SLOT_ALL 0xFFFFFFFF

/* Number of slots in a bitset */
#define BITSET_SIZE (MEMORY_BUFFER_SIZE / BITSET_SLOT_SIZE + 1)


/***********************************************************************
 * Global Variable Declarations
 ***********************************************************************/

/* Pool of memory */
extern unsigned int memBuffer[MEMORY_BUFFER_SIZE];

/* Tracks which memory slots are available */
extern Bitslot memAvail[BITSET_SIZE];

/* Index i is set to 1 if it was allocated along with the previous slot */
extern Bitslot memContig[BITSET_SIZE];

/***********************************************************************
 * Global Function Declarations
 ***********************************************************************/

void *static_malloc(unsigned int size);
void static_free(void* target);
int static_malloc_test(void); /* Controlled if #ifdef in malloc.c */

#define malloc static_malloc
#define free static_free

/***********************************************************************
 * File-Local Function Declarations (even though they are visible
 * globally)
 ***********************************************************************/

/*
 * Initialize the memory regions
 */
void static_malloc_init();

/*
 * Find enough contiguous slots to allocate memory for this request
 */
int findFreeSlots(int numSlots);

/*
 * Determines whether there are numSlots available starting at index
 */
bool checkContigSlots(int index, int numSlots);

/*
 * Get the availability of memory slot i
 */
bool getSlotAvail(int i);

/*
 * Set the availability of memory slot i
 */
void setSlotAvail(int i, bool avail);

/*
 * Determine if this slot (slot i) is part of the previous slot
 */
bool getSlotContig(int i);

/*
 * Adjust the contiguity record of slot i
 */
void setSlotContig(int i, bool contig);

/*
 * Debug routine to display the bitmaps
 */
void printAvail();
void printContig();

/* Test whether a particular bit is set */
bool testBit(Bitset bits, int index);

/* Set the value of a particular bit */
void setBit(Bitset bits, int index, bool val);

/* Determine if there are any empty bits in the slot for the index indicated */
Bitslot testBitSlot(Bitset bits, int slotIndex);

/* Set all of the bits in the bitset (also need the length of the bitset) */
void bitsetSetAll(Bitset bits, int len);

/* Clear all of the bits in the bitset (also need the length of the bitset) */
void bitsetClearAll(Bitset bits, int len);


#endif /* _MALLOC_H_ */


