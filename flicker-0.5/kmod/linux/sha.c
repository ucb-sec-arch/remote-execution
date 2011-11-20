/*
 * sha.c: Compute a SHA hash
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

#include <linux/module.h>
#include <linux/crypto.h> /* for sha1 */
#include <linux/scatterlist.h> /* for sha1 */

#include "log.h"
#include "flicker.h"

/* A one-shot invocation of SHA-1 using the kernel's cryptographic
 * API. Returns 1 on success, 0 on failure. */
int sha1(void *data, const unsigned int len, unsigned char digest[20]) {
    int ret, i;
    int num_pages, num_extra;

    int sg_entries = 0; /* number of entries in sg */
    struct scatterlist *sg;

    struct crypto_hash *tfm;
    struct hash_desc desc;
    void *ptr;

    if(!data) return 0;

    sg_entries = len / PAGE_SIZE + 1;
    sg = kmalloc(sg_entries*sizeof(struct scatterlist), GFP_KERNEL);
    if(!sg) {
        error("Failed to kmalloc scatterlist structure");
        return 0;
    }

    tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
    if (tfm == NULL) {
        error("Alloc of TFM for sha1 failed!");
        goto fail;
    }

    if(crypto_hash_digestsize(tfm) != 20) {
        error("crypto_hash_digestsize(tfm) is %d; != 20",
              crypto_hash_digestsize(tfm));
        goto fail;
    }

    desc.tfm = tfm;

    ret = crypto_hash_init(&desc);
    if (ret != 0) {
        error("crypto_hash_init returned erroneous value %d", ret);
        goto fail;
    }

    sg_init_table(sg, sg_entries);

    num_pages = len / PAGE_SIZE;
    num_extra = len % PAGE_SIZE;
    i = 0;
    for(i=0; i < num_pages; i++) {
        ptr = data + i*PAGE_SIZE;
        //dbg("sg_set_buf(&sg[%d], 0x%08x, %ld)", i, (uint32_t)ptr, PAGE_SIZE);
        sg_set_buf(&sg[i], ptr, PAGE_SIZE);
    }
    if(num_extra > 0) {
        ptr = data + i*PAGE_SIZE;
        //dbg("sg_set_buf(&sg[%d], 0x%08x, %d)", i, (uint32_t)ptr, num_extra);
        sg_set_buf(&sg[i], ptr, num_extra);
    }

    ret = crypto_hash_digest(&desc, sg, len, digest);
    if (ret != 0) {
        error("crypto_hash_digest returned erroneous value %d", ret);
        goto fail;
    }

    crypto_free_hash(tfm); tfm = NULL;

    //dbg("crypto_hash_digest SHA-1 result:");
    //dump_bytes(digest, 20);

    kfree(sg); sg = NULL;

    return 1;

  fail:
    if(sg) kfree(sg);
    if(tfm) crypto_free_hash(tfm);

    return 0;
}

