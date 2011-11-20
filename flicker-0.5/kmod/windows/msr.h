/*
 *  msr.h: MSR magic numbers for Flicker
 *
 *  Copyright (C) 2006-2010 Jonathan M. McCune
 */

#ifndef __ASM_MSR_H
#define __ASM_MSR_H

#define IA32_FEATURE_CONTROL_MSR                       0x3a
#define IA32_FEATURE_CONTROL_MSR_LOCK                  0x1
#define IA32_FEATURE_CONTROL_MSR_ENABLE_VMX_IN_SMX     0x2
#define IA32_FEATURE_CONTROL_MSR_ENABLE_VMX_OUT_SMX    0x4
#define IA32_FEATURE_CONTROL_MSR_SENTER_PARAM_CTL      0x7f00
#define IA32_FEATURE_CONTROL_MSR_ENABLE_SENTER         0x8000

#define MSR_IA32_FEATURE_CONTROL		0x0000003a
#define MSR_IA32_MTRRCAP                0x000000fe
#define MSR_IA32_MTRR_DEF_TYPE          0x2ff
#define MSR_IA32_MISC_ENABLE            0x000001a0
#define FEATURE_CONTROL_LOCKED			(1<<0)

#define MSR_IA32_MCG_CAP                0x00000179
#define MSR_IA32_MCG_STATUS             0x0000017a
#define MSR_IA32_MCG_CTL                0x0000017b
#define MSR_IA32_MC0_STATUS             0x00000401
#define MSR_IA32_APICBASE               0x0000001b
#define MSR_IA32_APICBASE_BSP           (1<<8)

#endif /* __ASM_MSR_H */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
