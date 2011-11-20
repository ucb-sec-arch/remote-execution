/*
 * mle.h: Intel(r) TXT MLE header definition
 *
 * Copyright (c) 2003-2008, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *  mle.h: Modified for Flicker
 */


#ifndef __MLE_H__
#define __MLE_H__

#include "log.h"

#ifdef _WIN32
#include "wintypes.h"
#endif // _WIN32

#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32
struct _uuid_t {
  uint32_t    data1;
  uint16_t    data2;
  uint16_t    data3;
  uint16_t    data4;
  uint8_t     data5[6];
#ifndef _WIN32
} __attribute__ ((__packed__));
#else // _WIN32
};
#pragma pack(pop) // Pop the packed attribute
#endif // _WIN32

typedef struct _uuid_t uuid_t;

static inline bool are_uuids_equal(const uuid_t *uuid1, const uuid_t *uuid2)
{
    return (memcmp(uuid1, uuid2, sizeof(*uuid1)) == 0);
}

static inline void print_uuid(const uuid_t *uuid)
{
    dbg("{0x%08x, 0x%04x, 0x%04x, 0x%04x,\n"
        "\t\t{0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x}}",
        uuid->data1, (uint32_t)uuid->data2, (uint32_t)uuid->data3,
        (uint32_t)uuid->data4, (uint32_t)uuid->data5[0],
        (uint32_t)uuid->data5[1], (uint32_t)uuid->data5[2],
        (uint32_t)uuid->data5[3], (uint32_t)uuid->data5[4],
        (uint32_t)uuid->data5[5]);
}

/*
 * SINIT/MLE capabilities
 */
typedef union {
    uint32_t  _raw;
    struct {
        uint32_t  rlp_wake_getsec     : 1;
        uint32_t  rlp_wake_monitor    : 1;
        uint32_t  reserved            : 30;
    };
} txt_caps_t;


/*
 * MLE header structure
 *   describes an MLE for SINIT and OS/loader SW
 */
typedef struct {
    uuid_t      uuid;
    uint32_t    length;
    uint32_t    version;
    uint32_t    entry_point;
    uint32_t    first_valid_page;
    uint32_t    mle_start_off;
    uint32_t    mle_end_off;
    txt_caps_t  capabilities;
} mle_hdr_t;

#define MLE_HDR_UUID      {0x9082ac5a, 0x476f, 0x74a7, 0x5c0f, \
                              {0x55, 0xa2, 0xcb, 0x51, 0xb6, 0x42}}

/*
 * values supported by current version of tboot
 */
#define MLE_HDR_VER       0x00020000     /* 2.0 */
#define MLE_HDR_CAPS      0x00000003     /* rlp_wake_{getsec, monitor} = 1 */

#endif      /* __MLE_H__ */


/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
