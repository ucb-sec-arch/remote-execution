/*
 * tpm.c: TPM-related support functions
 *
 * Copyright (c) 2006-2007, Intel Corporation
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
 *  tpm.c: Modified for Flicker
 */

/**
 * TODO: A better way to do all of this would be to cooperate with the
 * existing kernel's TPM device driver. This code is largely
 * redundant with respect to the kernel.
 */

#ifndef _WIN32
#include <asm/io.h> /* for ioremap, readb and writeb */
#else  // _WIN32
#include "wintypes.h"
#define cpu_relax() /* TODO: Find Windows equivalent */
#endif // _WIN32

#include "tpm.h"
#include "log.h"
#include "io.h"

#define TPM_TAG_RQU_COMMAND         0x00C1
#define TPM_TAG_RQU_AUTH1_COMMAND   0x00C2
#define TPM_TAG_RQU_AUTH2_COMMAND   0x00C3
#define TPM_ORD_PCR_EXTEND          0x00000014
#define TPM_ORD_PCR_READ            0x00000015
#define TPM_ORD_PCR_RESET           0x000000C8
#define TPM_ORD_NV_READ_VALUE       0x000000CF
#define TPM_ORD_NV_WRITE_VALUE      0x000000CD
#define TPM_ORD_GET_CAPABILITY      0x00000065
#define TPM_ORD_SEAL                0x00000017
#define TPM_ORD_UNSEAL              0x00000018
#define TPM_ORD_OSAP                0x0000000B
#define TPM_ORD_OIAP                0x0000000A

#define TPM_TAG_PCR_INFO_LONG       0x0006
#define TPM_TAG_STORED_DATA12       0x0016

#define readb(x) (*(volatile char *)(x))
#define writeb(d,x) (*(volatile char *)(x) = (d))

/*
 * TPM registers and data structures
 *
 * register values are offsets from each locality base
 * see {read,write}_tpm_reg() for data struct format
 */

#ifdef _WIN32
#pragma pack(push, 1)
#endif // _WIN32

/* TPM_ACCESS_x */
#define TPM_REG_ACCESS           0x00
typedef union {
    uint8_t _raw[1];                      /* 1-byte reg */
#ifndef _WIN32
    struct __attribute__ ((packed)) {
#else  // _WIN32
    struct {
#endif // _WIN32
        uint8_t tpm_establishment   : 1;  /* RO, 0=T/OS has been established
                                        before */
        uint8_t request_use         : 1;  /* RW, 1=locality is requesting TPM use */
        uint8_t pending_request     : 1;  /* RO, 1=other locality is requesting
                                        TPM usage */
        uint8_t seize               : 1;  /* WO, 1=seize locality */
        uint8_t been_seized         : 1;  /* RW, 1=locality seized while active */
        uint8_t active_locality     : 1;  /* RW, 1=locality is active */
        uint8_t reserved            : 1;
        uint8_t tpm_reg_valid_sts   : 1;  /* RO, 1=other bits are valid */
    };
} tpm_reg_access_t;

/* TPM_STS_x */
#define TPM_REG_STS              0x18
typedef union {
    uint8_t _raw[3];                  /* 3-byte reg */
#ifndef _WIN32
    struct __attribute__ ((packed)) {
#else  // _WIN32
    struct {
#endif // _WIN32
        uint8_t reserved1       : 1;
        uint8_t response_retry  : 1;  /* WO, 1=re-send response */
        uint8_t reserved2       : 1;
        uint8_t expect          : 1;  /* RO, 1=more data for command expected */
        uint8_t data_avail      : 1;  /* RO, 0=no more data for response */
        uint8_t tpm_go          : 1;  /* WO, 1=execute sent command */
        uint8_t command_ready   : 1;  /* RW, 1=TPM ready to receive new cmd */
        uint8_t sts_valid       : 1;  /* RO, 1=data_avail and expect bits are
                                    valid */
        uint16_t burst_count    : 16; /* RO, # read/writes bytes before wait */
    };
} tpm_reg_sts_t;

#ifdef _WIN32
#pragma pack(pop)
#endif // _WIN32

/* TPM_DATA_FIFO_x */
#define TPM_REG_DATA_FIFO        0x24
typedef union {
        uint8_t _raw[1];                      /* 1-byte reg */
} tpm_reg_data_fifo_t;

/*
 * assumes that all reg types follow above format:
 *   - packed
 *   - member named '_raw' which is array whose size is that of data to read
 */
#define read_tpm_reg(locality, reg, pdata)      \
    _read_tpm_reg(locality, reg, (pdata)->_raw, sizeof(*(pdata)))

#define write_tpm_reg(locality, reg, pdata)     \
    _write_tpm_reg(locality, reg, (pdata)->_raw, sizeof(*(pdata)))


static void _read_tpm_reg(int locality, uint32_t reg, uint8_t *_raw, size_t size)
{
    size_t i;
    void *io_mem;

    io_mem = f_ioremap((TPM_LOCALITY_BASE_N(locality) | reg), size);
    if(io_mem == NULL){
        logit("ioremap failed (_read_tpm_reg), size %08x\n", size);
        return;
    }
    for ( i = 0; i < size; i++ )
        //_raw[i] = readb((TPM_LOCALITY_BASE_N(locality) | reg) + i);
        _raw[i] = readb((char*)io_mem + i);
    f_iounmap(io_mem, size);
}

static void _write_tpm_reg(int locality, uint32_t reg, uint8_t *_raw, size_t size)
{
    size_t i;
    void *io_mem;

    io_mem = f_ioremap((TPM_LOCALITY_BASE_N(locality) | reg), size);
    if(io_mem == NULL){
        logit("ioremap failed (_write_tpm_reg)\n");
        return;
    }
    for ( i = 0; i < size; i++ )
        //writeb(_raw[i], (TPM_LOCALITY_BASE_N(locality) | reg) + i);
        writeb(_raw[i], (char*)io_mem + i);
    f_iounmap(io_mem, size);
}

#define reverse_copy(out, in, count) \
    _reverse_copy((uint8_t *)(out), (uint8_t *)(in), count)

static inline void _reverse_copy(uint8_t *out, uint8_t *in, uint32_t count)
{
    uint32_t i;
    for ( i = 0; i < count; i++ )
        out[i] = in[count - i - 1];
}

#define TPM_VALIDATE_LOCALITY_TIME_OUT  0x100

static bool tpm_validate_locality(uint32_t locality)
{
    tpm_reg_access_t reg_acc;
    uint32_t i;

    for ( i = 0; i < TPM_VALIDATE_LOCALITY_TIME_OUT; i++ ) {
        /*
         * TCG spec defines reg_acc.tpm_reg_valid_sts bit to indicate
         * whether other bits of access reg are valid. (but this bit
         * will also be 1 while this locality is not available, so
         * check seize bit too) It also defines that reading
         * reg_acc.seize should always return 0
         */
        read_tpm_reg(locality, TPM_REG_ACCESS, &reg_acc);
        if ( reg_acc.tpm_reg_valid_sts == 1 && reg_acc.seize == 0)
            return true;
        cpu_relax();
    }

    return false;
}

#define TPM_ACTIVE_LOCALITY_TIME_OUT    0x800
#define TPM_STS_VALID_TIME_OUT          0x100
#define TPM_CMD_READY_TIME_OUT          0x100
#define TPM_CMD_WRITE_TIME_OUT          0x100
#define TPM_DATA_AVAIL_TIME_OUT         0x100
#define TPM_RSP_READ_TIME_OUT           0x100

/*
 *   locality : TPM locality (0 - 3)
 *   in       : All bytes for a single TPM command, including TAG, SIZE,
 *              ORDINAL, and other arguments. All data should be in big-endian
 *              style. The in MUST NOT be NULL, containing at least 10 bytes.
 *              0   1   2   3   4   5   6   7   8   9   10  ...
 *              -------------------------------------------------------------
 *              | TAG  |     SIZE      |    ORDINAL    |    arguments ...
 *              -------------------------------------------------------------
 *   in_size  : The size of the whole command contained within the in buffer.
 *              It should equal to the SIZE contained in the in buffer.
 *   out      : All bytes of the TPM response to a single command. All data
 *              within it will be in big-endian style. The out MUST not be
 *              NULL, and will return at least 10 bytes.
 *              0   1   2   3   4   5   6   7   8   9   10  ...
 *              -------------------------------------------------------------
 *              | TAG  |     SIZE      |  RETURN CODE  |    other data ...
 *              -------------------------------------------------------------
 *   out_size : In/out paramter. As in, it is the size of the out buffer;
 *              as out, it is the size of the response within the out buffer.
 *              The out_size MUST NOT be NULL.
 *   return   : 0 = success; if not 0, it equal to the RETURN CODE in out buf.
 */

#define CMD_HEAD_SIZE           10
#define RSP_HEAD_SIZE           10
#define CMD_SIZE_OFFSET         2
#define CMD_ORD_OFFSET          6
#define RSP_SIZE_OFFSET         2
#define RSP_RST_OFFSET          6

/*
 * The _tpm_submit_cmd function comes with 2 global buffers: cmd_buf & rsp_buf.
 * Before calling, caller should fill cmd arguements into cmd_buf via
 * WRAPPER_IN_BUF macro. After calling, caller should fetch result from
 * rsp_buffer via WRAPPER_OUT_BUF macro.
 * cmd_buf content:
 *  0   1   2   3   4   5   6   7   8   9   10  ...
 * -------------------------------------------------------------
 * |  TAG  |     SIZE      |    ORDINAL    |    arguments ...
 * -------------------------------------------------------------
 * rsp_buf content:
 *  0   1   2   3   4   5   6   7   8   9   10  ...
 * -------------------------------------------------------------
 * |  TAG  |     SIZE      |  RETURN CODE  |    other data ...
 * -------------------------------------------------------------
 *
 *   locality : TPM locality (0 - 4)
 *   tag      : The TPM command tag
 *   cmd      : The TPM command ordinal
 *   arg_size : Size of argument data.
 *   out_size : IN/OUT paramter. The IN is the expected size of out data;
 *              the OUT is the size of output data within out buffer.
 *              The out_size MUST NOT be NULL.
 *   return   : TPM_SUCCESS for success, for other error code, refer to the .h
 */

#define WRAPPER_IN_BUF          (cmd_buf + CMD_HEAD_SIZE)
#define WRAPPER_OUT_BUF         (rsp_buf + RSP_HEAD_SIZE)
#define WRAPPER_IN_MAX_SIZE     (TPM_CMD_SIZE_MAX - CMD_HEAD_SIZE)
#define WRAPPER_OUT_MAX_SIZE    (TPM_RSP_SIZE_MAX - RSP_HEAD_SIZE)

/* ensure TPM is ready to accept commands */
bool is_tpm_ready(uint32_t locality)
{
    tpm_reg_access_t reg_acc;

    if ( !tpm_validate_locality(locality) ) {
        logit("TPM is not available.\n");
        return false;
    }

    /*
     * must ensure TPM_ACCESS_0.activeLocality bit is clear
     * (: locality is not active)
     */
    read_tpm_reg(locality, TPM_REG_ACCESS, &reg_acc);
    if ( reg_acc.active_locality != 0 ) {
        dbg("(in tpm.c) reg_acc.active_locality != 0\n");
        /* make inactive by writing a 1 */
        reg_acc.active_locality = 1;
        write_tpm_reg(locality, TPM_REG_ACCESS, &reg_acc);
    }

    dbg("TPM is ready\n");

    return true;
}

