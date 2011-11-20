/*
 *  sysfs.c: sysfs entries for controlling Flicker
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

/**
 * SysFs entries.  This is how userspace communicates with
 * Flicker. See Documentation/sysfs-rules.txt in the linux kernel
 * source for more information.
 */

#include "flicker.h"
#include "sysfs.h"
#include "acmod.h"
//#include "acpi.h" /* for acpi_dump */
#include "log.h"

/* globals declared in flickermod.c */
extern acmod_t *g_acmod;
extern size_t g_acmod_size;

static int num_output = -1;

struct flicker_obj {
    char *name;
    struct list_head slot_list;
    struct kobject kobj;
};

enum {
    IDLE = 0,
    READING_ACMOD,
    READING_PAL,
    READING_INPUTS,
    RUNNING
};

/* So we can show the status in control. */
char* stateName[] = {
  "IDLE",
  "READING_ACMOD",
  "READING_PAL",
  "READING_INPUTS",
  "RUNNING"
};

int flicker_sysfs_data_state;


/* TODO: We should probably print this in its raw form, and let other
   programs parse it. */
static ssize_t data_read(struct file *f, struct kobject *kobj,
// static ssize_t data_read(struct kobject *kobj,
                         struct bin_attribute *binattr,
                         char *buf, loff_t pos,
                         size_t count) {
  unsigned int copybytes;
  static int bcount = 0;

  if (pos == 0) {
    bcount = sizeof(g_pal->outputs);
  }

  /* copy min(bcount, count) */
  copybytes = bcount < count ? bcount : count;

  memcpy(buf, g_pal->outputs + pos, copybytes);

  bcount -= copybytes;

  dbg("data_read() count %d", count);

  return copybytes;
}

/* better consume all available bytes, or call will recur forever*/
static ssize_t data_write(struct file *f, struct kobject *kobj,
// static ssize_t data_write(struct kobject *kobj,
                          struct bin_attribute *binattr,
                          char *buf, loff_t pos,
                          size_t count)
{
    dbg("data_write() state %d, count %d, pos %d, total %d",
        flicker_sysfs_data_state, count, (uint32_t)pos, (uint32_t)count+(uint32_t)pos);

    assert(NULL != g_pal);

    switch(flicker_sysfs_data_state) {
        case READING_ACMOD:
            assert(NULL != g_acmod);
            if(pos+count > sizeof(g_acmod->acm)) {
                error("Error: pos+count > sizeof(g_acmod->acm) (0x%08x bytes)",
                      sizeof(g_acmod->acm));
                break;
            }
            memcpy(g_acmod->acm.raw+pos, buf, count);
            g_acmod_size += count;
            break;
        case READING_PAL:
            if(pos+count > sizeof(g_pal->pal)) {
                error("Error: pos+count > sizeof(g_pal->pal)");
                break;
            }

            memcpy(g_pal->pal + pos, buf, count);

            break;
        case READING_INPUTS:
            if(pos+count > sizeof(g_pal->inputs)) {
                error("Error: pos+count > sizeof(g_pal->inputs)");
                break;
            }
            memcpy(g_pal->inputs+pos, buf, count);
            break;
        case IDLE:
        default:
            error("ERROR: data received in invalid state %s",
                  stateName[flicker_sysfs_data_state]);
            break;
    }
    return count;
}


/**
 * Called whenever the user reads the sysfs control handle.
 */
static ssize_t control_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    logit("control_show() invoked");
    return snprintf(buf, PAGE_SIZE, "%s", stateName[flicker_sysfs_data_state]);
}


/**
 * Called whenever the user writes to the sysfs control handle to this kernel
 * object.
 */
static ssize_t
control_store(struct kobject *kobj, struct kobj_attribute *attr,
              const char *buf, size_t count)
{
    dbg("control_store() invoked");
    if(count >= 1) {
        switch(buf[0]) {
            case 'g':
                dbg("Final preparations for launch underway");
                break;
            case 'G':
                dbg("Attempting launch");
                if(launch_drtm()) { /* in flickermod.c */
                    dbg("ERROR: launch_drtm FAILED!");
                }
                num_output = INT_MAX;
                break;
            case 'A':
                dbg("Begin ACmod acquisition");
                assert(NULL != g_acmod); /* will be NULL on non-Intel CPU */
                g_acmod_size = 0;
                flicker_sysfs_data_state = READING_ACMOD;
                break;
            case 'a':
                dbg("End of ACmod acquisition; got %d bytes", g_acmod_size);
                flicker_sysfs_data_state = IDLE;
                break;
            case 'M':
                dbg("Begin PAL acquisition");
                flicker_sysfs_data_state = READING_PAL;
                break;
            case 'm':
                dbg("End of PAL acquisition");
                compute_expected_pcrs();
                flicker_sysfs_data_state = IDLE;
                break;
            case 'I':
                dbg("Begin Input acquisition");
                flicker_sysfs_data_state = READING_INPUTS;
                break;
            case 'i':
                dbg("End of Input acquisition");
                flicker_sysfs_data_state = IDLE;
                break;
            case 'd':
                dbg("ACPI debug dump DISABLED");
                //acpi_dump(); /* in acpi.c */
                break;
     default:
                logit("Unknown command %08x, count %08x", buf[0] & 0xff, count);
        }
    }

    return 1;
}

/**
 * Binary sysfs filesystem entries.
 *
 * TODO: No need for an array of binary attributes, since there is
 * only one.
 */
#define FLICKER_BIN_ATTR(_name, _mode) \
struct bin_attribute bin_attr_##_name = { \
        .attr =  { .name = __stringify(_name), \
                   .mode = _mode, \
                 }, \
        .read =  _name##_read, \
        .write = _name##_write, \
}

FLICKER_BIN_ATTR(data, 0600); /* Read-Write */

static struct bin_attribute *flicker_bin_attrs[] = {
        &bin_attr_data,
        NULL
};



/* Generate sysfs_attr_flicker */
static struct kobj_attribute attr_control =
__ATTR(control,
       0600,
       control_show,
       control_store);

/* Need a kobj to get a home in sysfs: /sys/kernel/... */
static struct kobject *flicker_kobj;


/**
 * Sysfs registrations
 */
int flicker_sysfs_init(void)
{
    int rc = 0;
    int i;

    assert(NULL != g_pal);

    if(get_cpu_vendor() == CPU_VENDOR_INTEL) {
        assert(NULL != g_acmod);
        assert(NULL != g_mle_ptab);

        g_acmod_size = 0;
    }

    flicker_sysfs_data_state = IDLE;

    /*
     * Create a simple kobject with the name of "flicker",
     * located under /sys/kernel/
     *
     * As this is a simple directory, no uevent will be sent to
     * userspace.  That is why this function should not be used for
     * any type of dynamic kobjects, where the name and number are
     * not known ahead of time.
     */
    flicker_kobj = kobject_create_and_add("flicker", kernel_kobj);
    if (!flicker_kobj)
    	return -ENOMEM;

    /* reg other files and handle errors w/ nested gotos */
    if((rc = sysfs_create_file(flicker_kobj, &attr_control.attr))) {
        error("Error [%d] registering flicker sysfs control", rc);
        return rc;
    }

    for (i = 0; flicker_bin_attrs[i]; i++) {
        rc = sysfs_create_bin_file(flicker_kobj,
                                   flicker_bin_attrs[i]);
        if (rc) {
            error("Error [%d] registering flicker sysfs binary file", rc);
            while (--i >= 0)
                sysfs_remove_bin_file(flicker_kobj,
                                      flicker_bin_attrs[i]);
            return rc;
        }
    }

    logit("flicker: sysfs entries registered successfully");

    /* TODO: gracefully handle failure conditions and free kobj reference. */
    //kobject_put(flicker_kobj);

    return rc;
}


void flicker_sysfs_cleanup(void)
{
  int i;

  sysfs_remove_file(flicker_kobj, &attr_control.attr);

  for (i = 0; flicker_bin_attrs[i]; i++) {
    sysfs_remove_bin_file(flicker_kobj, flicker_bin_attrs[i]);
  }

  kobject_put(flicker_kobj);

  logit("flicker: sysfs entries unloaded.");
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
