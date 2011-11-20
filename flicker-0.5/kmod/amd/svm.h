/*
 * svm.h - AMD Secure Virtual Machine helpers
 */

#ifndef _SVM_H_
#define _SVM_H_

int do_amducodeclear(void);

#ifndef _WIN32
static inline void enable_svme(void) {
    /* Set SVME bit in EFER */
    asm volatile(
        "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
        "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
        "orl $0x00001000, %%eax\n\t" /* set bit 12 */
        "wrmsr\n\t" /* SVME should now be enabled */
        :
        :
        : "%edx");
}
#else  // _WIN32
static inline void enable_svme(void) {
    /* Set SVME bit in EFER */
    //asm volatile(
    //    "movl $0xc0000080, %%ecx\n\t" /* specify EFER */
    //    "rdmsr\n\t" /* puts value in EAX (and EDX in 64-bit mode) */
    //    "orl $0x00001000, %%eax\n\t" /* set bit 12 */
    //    "wrmsr\n\t" /* SVME should now be enabled */
    //    :
    //    :
    //    : "%edx");
    __asm {
    	mov ecx, 0xc0000080	/* specify EFER */
    	rdmsr				/* puts value in EAX (and EDX in 64-bit mode) */
    	or eax, 0x00001000  /* set bit 12 */
    	wrmsr				/* SVME should now be enabled */
    }
}
#endif // _WIN32

#endif /* _SVM_H_ */


