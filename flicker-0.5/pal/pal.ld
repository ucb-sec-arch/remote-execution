/* slb.ld - linker script for building PALs */

/**
 * 1M-512K Params
 * 512-64K Code
 * 64-60K Stack
 * 60-56K 4K of PT
 * 56-52K 4K of PT
 * 52-48K 4K of PT
 * 48-0K  48K of Code
 */

/* Defines memory regions that the linker can use.  For the moment, 
 * we are simply working around the I/O Pages, Resume state page, 
 * stack, and resume page tables.  Eventually this stuff should
 * auto-layout, taking security concerns wrt early-executing code
 * into consideration. 
 */
MEMORY
{
    low (rwxai)  : ORIGIN = 0, LENGTH = 48K
    /* this hole contains the stack and resumption page tables */
    high (rwxai) : ORIGIN = 0x10000, LENGTH = 0x70000
    null (rwxai) : ORIGIN = 0x80000, LENGTH = 0 /* Intentional 0 length */
}


SECTIONS
{
    /* Note: expression evaulate lazily.  We want to be sure the
     * slb_code contents from asm.S are at very start of the resulting
     * image.  We also need to make sure we don't overflow region low,
     * which we do manually by specifying what input file's contents 
     * go where.
     */

	 /* IMPORTANT: Kernel resume code needs to go in LOW */

/* /rollback/v0.1/flicker-0.4/amd# 
   find common/ tsm/ extra/ trinc/ bitlocker/ diffPriv/ noop/ -name "*.o" | xargs objdump -h | cut -d ' ' -f 4 | sort | uniq 
*/

  LOW :
  {
    *(.text.slb)
    *(.data.slb)
    *(.rodata.slb)
    *(.bss.slb)
    *(.slb.end_of_low) /* not a real variable; only concerned with its address */
    *(.slb.aligned_end_of_low) /* not a real variable; only concerned with its address */
  } >low
  HIGH :
  {
    *(.slb.start_of_high) /* not a real variable; only concerned with its address */
    *(.text)
    *(.data)
    *(.rodata*)
    *(.bss)
    *(.slb.end_of_high) /* not a real variable; only concerned with its address */
  } >high
  /* Code should be built with -fno-common, but if it's not, complain! */
  ERRONEOUS :
  {
    *(COMMON)
  } >null
}
