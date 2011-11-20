/*
 * acmod.c: support functions for use of Intel(r) TXT Authenticated
 *          Code (AC) Modules
 *
 * Copyright (c) 2003-2007, Intel Corporation
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
 *  acmod.c: Modified for Flicker
 */

#ifndef _WIN32
#include <linux/module.h>
#include <asm/processor-flags.h> /* for X86_CR* */
#include <asm/mtrr.h>
#else  // _WIN32
#include "wintypes.h"
#include "txt_processor.h"
#endif // _WIN32

#include "acmod.h"
#include "mle.h" /* for UUID stuff */
#include "heap.h"
#include "config_regs.h"
#include "smx.h"
#include "mtrrs.h"
#include "io.h"


#define ACM_MEM_TYPE_UC                 0x0100
#define ACM_MEM_TYPE_WC                 0x0200
#define ACM_MEM_TYPE_WT                 0x1000
#define ACM_MEM_TYPE_WP                 0x2000
#define ACM_MEM_TYPE_WB                 0x4000

/* this is arbitrary and can be increased when needed */
#define MAX_SUPPORTED_ACM_VERSIONS      16

typedef struct {
    struct {
        uint32_t mask;
        uint32_t version;
    } acm_versions[MAX_SUPPORTED_ACM_VERSIONS];
    int n_versions;
    uint32_t acm_max_size;
    uint32_t acm_mem_types;
    uint32_t senter_controls;
} getsec_parameters_t;

#define DEF_ACM_MAX_SIZE                0x8000
#define DEF_ACM_VER_MASK                0xffffffff
#define DEF_ACM_VER_SUPPORTED           0x00
#define DEF_ACM_MEM_TYPES               ACM_MEM_TYPE_UC
#define DEF_SENTER_CTRLS                0x00

static bool get_parameters(getsec_parameters_t *params)
{
    unsigned long cr4;
    uint32_t index, eax, ebx, ecx;
    int param_type;

    /* sanity check because GETSEC[PARAMETERS] will fail if not set */
    cr4 = read_cr4();
    if ( !(cr4 & X86_CR4_SMXE) ) {
        dbg("SMXE not enabled, can't read parameters");
        return false;
    }

    memset(params, 0, sizeof(*params));
    params->acm_max_size = DEF_ACM_MAX_SIZE;
    params->acm_mem_types = DEF_ACM_MEM_TYPES;
    params->senter_controls = DEF_SENTER_CTRLS;
    index = 0;
    do {
        __getsec_parameters(index++, &param_type, &eax, &ebx, &ecx);
        /* the code generated for a 'switch' statement doesn't work in this */
        /* environment, so use if/else blocks instead */
        if ( param_type == 0 )
            ;
        else if ( param_type == 1 ) {
            if ( params->n_versions == MAX_SUPPORTED_ACM_VERSIONS )
                dbg("number of supported ACM version exceeds "
                       "MAX_SUPPORTED_ACM_VERSIONS");
            else {
                params->acm_versions[params->n_versions].mask = ebx;
                params->acm_versions[params->n_versions].version = ecx;
                params->n_versions++;
            }
        }
        else if ( param_type == 2 )
            params->acm_max_size = eax & 0xffffffe0;
        else if ( param_type == 3 )
            params->acm_mem_types = eax & 0xffffffe0;
        else if ( param_type == 4 )
            params->senter_controls = (eax & 0x00007fff) >> 8;
        else {
            dbg("unknown GETSEC[PARAMETERS] type: %d", param_type);
            param_type = 0;    /* set so that we break out of the loop */
        }
    } while ( param_type != 0 );

    if ( params->n_versions == 0 ) {
        params->acm_versions[0].mask = DEF_ACM_VER_MASK;
        params->acm_versions[0].version = DEF_ACM_VER_SUPPORTED;
        params->n_versions = 1;
    }

    return true;
}

static acm_info_table_t *get_acmod_info_table(acm_hdr_t* hdr)
{
    uint32_t user_area_off;

    /* this fn assumes that the ACM has already passed at least the initial */
    /* is_acmod() checks */

    user_area_off = (hdr->header_len + hdr->scratch_size) * 4;
    /* check that table is within module */
    if ( user_area_off + sizeof(acm_info_table_t) > hdr->size*4 ) {
        dbg("ACM info table size too large: %x",
               user_area_off + sizeof(acm_info_table_t));
        return NULL;
    }

    return (acm_info_table_t *)((uint32_t)hdr + user_area_off);
}

static acm_chipset_id_list_t *get_acmod_chipset_list(acm_hdr_t* hdr)
{
    acm_info_table_t* info_table;
    uint32_t size, id_list_off;
    acm_chipset_id_list_t *chipset_id_list;

    /* this fn assumes that the ACM has already passed the is_acmod() checks */

    info_table = get_acmod_info_table(hdr);
    if ( info_table == NULL )
        return NULL;
    id_list_off = info_table->chipset_id_list;

    size = hdr->size * 4;

    /* check that chipset id table is w/in ACM */
    if ( id_list_off + sizeof(acm_chipset_id_t) > size ) {
        dbg("ACM chipset id list is too big: chipset_id_list=%x",
               id_list_off);
        return NULL;
    }

    chipset_id_list = (acm_chipset_id_list_t *)
                             ((unsigned long)hdr + id_list_off);

    /* check that all entries are w/in ACM */
    if ( id_list_off + sizeof(acm_chipset_id_t) +
         chipset_id_list->count * sizeof(acm_chipset_id_t) > size ) {
        dbg("ACM chipset id entries are too big:"
               " chipset_id_list->count=%x", chipset_id_list->count);
        return NULL;
    }

    return chipset_id_list;
}

void print_txt_caps(const char *prefix, txt_caps_t caps)
{
    dbg("%scapabilities: 0x%08x", prefix, caps._raw);
    dbg("%s    rlp_wake_getsec: %d", prefix, caps.rlp_wake_getsec);
    dbg("%s    rlp_wake_monitor: %d", prefix, caps.rlp_wake_monitor);
}

static void print_acm_hdr(acm_hdr_t *hdr, const char *mod_name)
{
    acm_info_table_t *info_table;
    acm_chipset_id_list_t *chipset_id_list;
    unsigned int i;
    acm_chipset_id_t *chipset_id;
    const uuid_t acm_v3_ref_uuid = ACM_UUID_V3;

    dbg("AC module header dump for %s:",
           (mod_name == NULL) ? "?" : mod_name);

    /* header */
    dbg("\t type: 0x%x ", hdr->module_type);
    if ( hdr->module_type == ACM_TYPE_CHIPSET )
        dbg("(ACM_TYPE_CHIPSET)");
    else
        dbg("(unknown)");
    dbg("\t length: 0x%x (%u)", hdr->header_len, hdr->header_len);
    dbg("\t version: %u", hdr->header_ver);
    dbg("\t chipset_id: 0x%x", (uint32_t)hdr->chipset_id);
    dbg("\t flags: 0x%x", (uint32_t)hdr->flags._raw);
    dbg("\t\t pre_production: %d", (int)hdr->flags.pre_production);
    dbg("\t\t debug_signed: %d", (int)hdr->flags.debug_signed);
    dbg("\t vendor: 0x%x", hdr->module_vendor);
    dbg("\t date: 0x%08x", hdr->date);
    dbg("\t size*4: 0x%x (%u)", hdr->size*4, hdr->size*4);
    dbg("\t code_control: 0x%x", hdr->code_control);
    dbg("\t entry point: 0x%08x:%08x", hdr->seg_sel,
           hdr->entry_point);
    dbg("\t scratch_size: 0x%x (%u)", hdr->scratch_size,
           hdr->scratch_size);

    /* info table */
    dbg("\t info_table:");
    info_table = get_acmod_info_table(hdr);
    if ( info_table == NULL ) {
        dbg("\t\t <invalid>");
        return;
    }
    dbg("\t\t uuid: "); print_uuid(&info_table->uuid); dbg("");
    if ( are_uuids_equal(&(info_table->uuid), &acm_v3_ref_uuid) )
        dbg("\t\t     ACM_UUID_V3");
    else
        dbg("\t\t     unknown");
    dbg("\t\t chipset_acm_type: 0x%x ",
           (uint32_t)info_table->chipset_acm_type);
    if ( info_table->chipset_acm_type == ACM_CHIPSET_TYPE_SINIT )
        dbg("(SINIT)");
    else if ( info_table->chipset_acm_type == ACM_CHIPSET_TYPE_BIOS )
        dbg("(BIOS)");
    else
        dbg("(unknown)");
    dbg("\t\t version: %u", (uint32_t)info_table->version);
    dbg("\t\t length: 0x%x (%u)", (uint32_t)info_table->length,
           (uint32_t)info_table->length);
    dbg("\t\t chipset_id_list: 0x%x", info_table->chipset_id_list);
    dbg("\t\t os_sinit_data_ver: 0x%x", info_table->os_sinit_data_ver);
    dbg("\t\t min_mle_hdr_ver: 0x%08x", info_table->min_mle_hdr_ver);
    print_txt_caps("\t\t ", info_table->capabilities);
    dbg("\t\t acm_ver: %u", (uint32_t)info_table->acm_ver);

    /* chipset list */
    dbg("\t chipset list:");
    chipset_id_list = get_acmod_chipset_list(hdr);
    if ( chipset_id_list == NULL ) {
        dbg("\t\t <invalid>");
        return;
    }
    dbg("\t\t count: %u", chipset_id_list->count);
    for ( i = 0; i < chipset_id_list->count; i++ ) {
        dbg("\t\t entry %u:", i);
        chipset_id = &(chipset_id_list->chipset_ids[i]);
        dbg("\t\t     flags: 0x%x", chipset_id->flags);
        dbg("\t\t     vendor_id: 0x%x", (uint32_t)chipset_id->vendor_id);
        dbg("\t\t     device_id: 0x%x", (uint32_t)chipset_id->device_id);
        dbg("\t\t     revision_id: 0x%x",
               (uint32_t)chipset_id->revision_id);
        dbg("\t\t     extended_id: 0x%x", chipset_id->extended_id);
    }
}

uint32_t get_supported_os_sinit_data_ver(acm_hdr_t* hdr)
{
    acm_info_table_t *info_table;

    /* assumes that it passed is_sinit_acmod() */

    info_table = get_acmod_info_table(hdr);
    if ( info_table == NULL )
        return 0x00;

    return info_table->os_sinit_data_ver;
}

txt_caps_t get_sinit_capabilities(acm_hdr_t* hdr)
{
    /* assumes that it passed is_sinit_acmod() */

    acm_info_table_t *info_table = get_acmod_info_table(hdr);
    if ( info_table == NULL || info_table->version < 3 ) {
    	txt_caps_t nocaps;
    	nocaps._raw = 0;
        return nocaps;
    }

    return info_table->capabilities;
}

static bool is_acmod(void *acmod_base, uint32_t acmod_size, uint8_t *type)
{
    acm_hdr_t *acm_hdr;
    acm_info_table_t *info_table;
    const uuid_t acm_v3_ref_uuid = ACM_UUID_V3;

    acm_hdr = (acm_hdr_t *)acmod_base;

    /* first check size */
    if ( acmod_size < sizeof(acm_hdr_t) ) {
        dbg("ACM size is too small: acmod_size=%x,"
               " sizeof(acm_hdr_t)=%x", acmod_size,
               (uint32_t)sizeof(acm_hdr_t) );
        return false;
    }
    if ( acmod_size != acm_hdr->size * 4 ) {
        dbg("ACM size is too small: acmod_size=%x,"
               " acm_hdr->size*4=%x", acmod_size, acm_hdr->size*4);
        return false;
    }

    /* then check type and vendor */
    if ( (acm_hdr->module_type != ACM_TYPE_CHIPSET) ||
         (acm_hdr->module_vendor != ACM_VENDOR_INTEL) ) {
        dbg("ACM type/vendor mismatch: module_type=%x,"
               " module_vendor=%x", acm_hdr->module_type,
               acm_hdr->module_vendor);
        return false;
    }

    info_table = get_acmod_info_table(acm_hdr);
    if ( info_table == NULL )
        return false;

    /* check if ACM UUID is present */
    if ( !are_uuids_equal(&(info_table->uuid), &acm_v3_ref_uuid) ) {
        dbg("unknown UUID: ");
        print_uuid(&info_table->uuid);
        dbg("\nExpected UUID: ");
        print_uuid(&acm_v3_ref_uuid);
        dbg("");
        return false;
    }

    if ( type != NULL )
        *type = info_table->chipset_acm_type;

    if ( info_table->version < 3 ) {
        dbg("ACM info_table version unsupported (%u)",
               (uint32_t)info_table->version);
        return false;
    }
    /* there is forward compatibility, so this is just a warning */
    else if ( info_table->version > 3 )
        dbg("ACM info_table version mismatch (%u)",
               (uint32_t)info_table->version);

    return true;
}

bool is_sinit_acmod(void *acmod_base, uint32_t acmod_size)
{
    uint8_t type;

    if ( !is_acmod(acmod_base, acmod_size, &type) )
        return false;

    if ( type != ACM_CHIPSET_TYPE_SINIT ) {
        dbg("ACM is not an SINIT ACM (%x)", type);
        return false;
    }

    return true;
}

bool does_acmod_match_chipset(acm_hdr_t* hdr)
{
    acm_chipset_id_list_t *chipset_id_list;
    acm_chipset_id_t *chipset_id;
    txt_didvid_t didvid;
    unsigned int i;

    /* this fn assumes that the ACM has already passed the is_acmod() checks */

    chipset_id_list = get_acmod_chipset_list(hdr);
    if ( chipset_id_list == NULL )
        return false;

    /* get chipset device and vendor id info */
    didvid._raw = read_pub_config_reg(TXTCR_DIDVID);
    dbg("chipset ids: vendor=%x, device=%x, revision=%x",
           didvid.vendor_id, didvid.device_id, didvid.revision_id);

    dbg("%x ACM chipset id entries:", chipset_id_list->count);
    for ( i = 0; i < chipset_id_list->count; i++ ) {
        chipset_id = &(chipset_id_list->chipset_ids[i]);
        dbg("\tvendor=%x, device=%x, flags=%x, revision=%x, "
               "extended=%x", (uint32_t)chipset_id->vendor_id,
               (uint32_t)chipset_id->device_id, chipset_id->flags,
               (uint32_t)chipset_id->revision_id, chipset_id->extended_id);

        if ( (didvid.vendor_id == chipset_id->vendor_id ) &&
             (didvid.device_id == chipset_id->device_id ) &&
             ( ( ( (chipset_id->flags & 0x1) == 0) &&
                 (didvid.revision_id == chipset_id->revision_id) ) ||
               ( ( (chipset_id->flags & 0x1) == 1) &&
                 ((didvid.revision_id & chipset_id->revision_id) != 0 ) ) ) )
            return true;
    }

    dbg("ACM does not match chipset");

    return false;
}

/**
 * Copy SINIT module to TXTCR-specified location. TODO: Fix the
 * ioremap memory leaks that ensue if something fails.
 */
bool copy_sinit(acm_hdr_t *sinit)
{
    void *sinit_region_base;
    uint32_t sinit_region_size;
    txt_heap_t *txt_heap;
    txt_heap_t *txt_heap_io;
    bios_data_t *bios_os_data;

    void *io_mem;
    uint64_t heap_size;

    if (!sinit) return false;

    /* get BIOS-reserved region from LT.SINIT.BASE config reg */
    sinit_region_base = (void*)(uint32_t)read_pub_config_reg(TXTCR_SINIT_BASE);
    sinit_region_size = (uint32_t)read_pub_config_reg(TXTCR_SINIT_SIZE);
    dbg("sinit_region_base=0x%08x, sinit_region_size=0x%08x",
           (uint32_t)sinit_region_base, (uint32_t)sinit_region_size);

    /*
     * check if BIOS already loaded an SINIT module there
     */
    txt_heap = get_txt_heap();
    if(!txt_heap) {
        error("ERROR: txt_heap NULL");
        return false;
    }

    heap_size = get_txt_heap_size();
    txt_heap_io = f_ioremap((uint32_t)txt_heap, (uint32_t)heap_size);

    if(!txt_heap_io) {
        error("ERROR: ioremap NULL");
        return false;
    }

    bios_os_data = get_bios_data_start(txt_heap_io);

    /* make sure our SINIT fits in the reserved region */
    if ( (sinit->size * 4) > sinit_region_size ) {
        dbg("BIOS-reserved SINIT size (%x) is too small for loaded "
               "SINIT (%x)", sinit_region_size, sinit->size*4);
        return false;
    }

    /* copy it there (need to use ioremap) */
    //memcpy(sinit_region_base, sinit, sinit->size*4);
    io_mem = f_ioremap((unsigned int)sinit_region_base, sinit->size*4);
    if(io_mem == NULL){
        dbg("ioremap failed (copy_sinit)");
        goto fail;
    }

    memcpy_toio(io_mem, sinit, sinit->size*4);
    dbg("copied SINIT (size=%x) to %p remap %p", sinit->size*4,
           sinit_region_base, io_mem);

    f_iounmap(io_mem, sinit->size*4);

  fail:
    f_iounmap(txt_heap_io, (size_t)heap_size);

    return true;
}


/*
 * Do some AC module sanity checks because any violations will cause
 * an TXT.RESET.  Instead detect these and print a desriptive message.
 */
bool verify_acmod(acm_hdr_t *acm_hdr)
{
    getsec_parameters_t params;
    uint32_t size;

    /* assumes this already passed is_acmod() test */

    size = acm_hdr->size * 4;        /* hdr size is in dwords, we want bytes */

    /*
     * AC mod must start on 4k page boundary
     */

    if ( (unsigned long)acm_hdr & 0xfff ) {
        dbg("AC mod base not 4K aligned (%p)", acm_hdr);
        return false;
    }
    dbg("AC mod base alignment OK");

    /* AC mod size must:
     * - be multiple of 64
     * - greater than ???
     * - less than max supported size for this processor
     */

    if ( (size == 0) || ((size % 64) != 0) ) {
        dbg("AC mod size %x bogus", size);
        return false;
    }

    if ( get_parameters(&params) == false ) {
        dbg("get_parameters() failed");
        return false;
    }

    if ( size > params.acm_max_size ) {
        dbg("AC mod size too large: %x (max=%x)", size,
               params.acm_max_size);
        return false;
    }

    dbg("AC mod size OK");

    /*
     * perform checks on AC mod structure
     */

    /* print it for debugging */
    print_acm_hdr(acm_hdr, "SINIT");

    /* entry point is offset from base addr so make sure it is within module */
    if ( acm_hdr->entry_point >= size ) {
        dbg("AC mod entry (%08x) >= AC mod size (%08x)",
               acm_hdr->entry_point, size);
        return false;
    }

    if ( !acm_hdr->seg_sel           ||       /* invalid selector */
         (acm_hdr->seg_sel & 0x07)   ||       /* LDT, PL!=0 */
         (acm_hdr->seg_sel + 8 > acm_hdr->gdt_limit) ) {
        dbg("AC mod selector [%04x] bogus", acm_hdr->seg_sel);
        return false;
    }

    return true;
}

/*
 * this must be done for each processor so that all have the same
 * memory types
 */
bool set_mtrrs_for_acmod(acm_hdr_t *hdr)
{
    unsigned long eflags;
    unsigned long cr0, cr4;

    void *io_mem;
    static acm_hdr_t acm_hdr;

    /*
     * need to do some things before we start changing MTRRs
     *
     * since this will modify some of the MTRRs, they should be saved first
     * so that they can be restored once the AC mod is done
     */

    io_mem = f_ioremap((uint32_t)hdr, sizeof(acm_hdr_t));
    if(io_mem == NULL){
        dbg("ERROR: ioremap FAILED for 0x%08x (set_mtrrs_for_acmod)",
               (uint32_t)hdr);
        return false;
    }

    memcpy_fromio(&acm_hdr, io_mem, sizeof(acm_hdr_t));
    f_iounmap(io_mem, sizeof(acm_hdr_t));

    dbg("sizeof(acm_hdr_t)=0x%08x, acm_hdr.size=%08x",
        sizeof(acm_hdr_t), acm_hdr.size);

    /* disable interrupts */
    eflags = read_eflags();
#ifndef _WIN32
    __asm__ __volatile__("cli": : :"memory");
#else  // _WIN32
    __asm {
    	cli
    }
#endif // _WIN32

    /* save CR0 then disable cache (CRO.CD=1, CR0.NW=0) */
    cr0 = read_cr0();
    write_cr0((cr0 & ~X86_CR0_NW) | X86_CR0_CD);

    /* flush caches */
    wbinvd();

    /* save CR4 and disable global pages (CR4.PGE=0) */
    cr4 = read_cr4();
    write_cr4(cr4 & ~X86_CR4_PGE);

    /* disable MTRRs */
    set_all_mtrrs(false);

    /*
     * now set MTRRs for AC mod and rest of memory. Use a local
     * variable due to iomem issues. See Table 10-8 in
     * Volume3A_SystemProgrammingGuide.pdf, Page 10-28 Vol.3
     * (WRBACK=WriteBack(WB))
     */
    //set_mem_type(hdr, hdr->size*4, MTRR_TYPE_WRBACK);
    set_mem_type(hdr, acm_hdr.size*4, MTRR_TYPE_WRBACK);

    /*
     * now undo some of earlier changes and enable our new settings
     */

    /* flush caches */
    wbinvd();

    /* enable MTRRs */
    /* STUDENTS: Commenting the below line should cause processor
     * error 5: "Load memory type error in Authentication Code
     * Execution Area" */
    set_all_mtrrs(true);

    /* restore CR0 (cacheing) */
    write_cr0(cr0);

    /* restore CR4 (global pages) */
    write_cr4(cr4);

    /* enable interrupts */
    write_eflags(eflags);

    return true; /* success */
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
