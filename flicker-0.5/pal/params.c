/*
 * params.c - Marshalls binary parameters into and out of a 4K buffer.
 * Format is as follows:
 * [numparams][param1size][param1data][param2size][param2data] where
 * numparams and the param sizes are 16-bit unsigned values, and the
 * data values are arbitrary length, so long as the total is below 4K.
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

#include "params.h"
#include "printk.h"
#include "string.h"

char *inputBuffer, *outputBuffer;
int inputBufferLength, outputBufferLength;
int nextOutputPos;

int pm_init(void *inbuf, int inbuf_length, void *outbuf, int outbuf_length)
{
    inputBuffer = (char *) inbuf;
    inputBufferLength = inbuf_length;
    outputBuffer = (char *) outbuf;
    outputBufferLength = outbuf_length;

    * (int *) outputBuffer = 0;
    nextOutputPos = sizeof(int);

    printk("Params initialized: inputBuffer @ 0x%p, outputBuffer @ 0x%p\n",
           inputBuffer, outputBuffer);

    dump_bytes((unsigned char *)inputBuffer, 128);

    return 1;
}

/* return amount of output space remaining for a single additional
 * output param (result subtracts bytes needed for metadata, i.e.,
 * result is amount of _data_ that can fit) */
int pm_avail (void)
{
    return outputBufferLength - nextOutputPos - sizeof(int) - sizeof(int);
}

/* reserve space for an output parameter */
char *pm_reserve(int paramType, int paramSize)
{
    char *data;

    if (paramSize > pm_avail()) {
        printk("ERROR - Not enough space in output buffer for %d bytes of data\n", paramSize);
        return 0;
    }

    // Fill in the next 8 bytes with the parameter type and size.

    * (int *) (outputBuffer + nextOutputPos) = paramType;
    nextOutputPos += sizeof(int);
    * (int *) (outputBuffer + nextOutputPos) = paramSize;
    nextOutputPos += sizeof(int);

    // Allocate the next 'paramSize' bytes for the parameter's data.

    data = outputBuffer + nextOutputPos;
    nextOutputPos += paramSize;

    // Increment the number of parameters, which is the first int in the
    // output buffer.

    ++(* (int *) outputBuffer);

    return data;
}

/* append some data to the output params */
int pm_append(int paramType, char *paramData, int paramSize)
{
    char *location;

    location = pm_reserve(paramType, paramSize);
    if (location == 0) {
        return -1;
    }
    memcpy(location, paramData, paramSize);
    return paramSize;
}

/* return the size of param type param_type, put location in *data */
int pm_get_addr(int param_type, char **data)
{
    char *currentOffset, *endOfInputBuffer;
    int numParams, paramIndex, paramType, paramSize;

    currentOffset = inputBuffer;
    endOfInputBuffer = inputBuffer + inputBufferLength;

    // Read the number of parameters at the beginning of the input buffer.

    if (currentOffset + sizeof(int) > endOfInputBuffer) {
        return -1;
    }
    numParams = * (int *) currentOffset;
    printk("pm_get_addr: numParams = %d\n", numParams);
    currentOffset += sizeof(int);

    // Search through the input buffer for a parameter with the requested
    // parameter type.

    for (paramIndex = 0; paramIndex < numParams; ++paramIndex) {
        // If there isn't enough room in the input buffer for a type and a
        // size field, we return an error.  We don't want to read invalid
        // data past the end of the input buffer.

        if (currentOffset + sizeof(int) + sizeof(int) > endOfInputBuffer) {
            return -1;
        }

        // Read the parameter type field as the first four bytes at the
        // location.

        paramType = * (int *) currentOffset;
        currentOffset += sizeof(int);

        // Read the parameter size field as the next four bytes at the
        // location.  Check for invalid sizes, i.e. negative sizes, ones
        // that are too big to fit in the input buffer, and ones that
        // extend beyond the end of the input buffer.

        paramSize = * (int *) currentOffset;
        currentOffset += sizeof(int);
        if (paramSize < 0 || paramSize > inputBufferLength) {
            return -1;
        }
        if (currentOffset + paramSize > endOfInputBuffer) {
            return -1;
        }

        // If the parameter's type matches the one we're looking for, then
        // we're done.  Return the data and the size.

        if (paramType == param_type) {
            *data = currentOffset;
            return paramSize;
        } else {
            printk("paramType (0x%08x) != param_type (0x%08x)\n",
                   paramType, param_type);
        }

        // Otherwise, advance to the next parameter location by skipping
        // over the parameter value.

        currentOffset += paramSize;
    }

    return -1;
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
