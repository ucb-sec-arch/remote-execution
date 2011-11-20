#ifndef _IO_H
#define _IO_H

#ifndef _WIN32
#define f_ioremap(ptr,size) ioremap((ptr), (size))
#define f_iounmap(ptr,size) iounmap((ptr))
#else  // _WIN32
#define f_ioremap(ptr,size) ioremap_win((ptr), (size))
#define f_iounmap(ptr,size) MmUnmapIoSpace((ptr), (size))
#define memcpy_toio(dst,src,size) memcpy((dst), (src), (size))
#define memcpy_fromio(dst,src,size) memcpy((dst), (src), (size))
#define wbinvd() __asm {wbinvd}
#define virt_to_phys(x) (uint32_t) MmGetPhysicalAddress((x)).QuadPart

static __inline void* ioremap_win(unsigned long ptr, unsigned long size) {
    PHYSICAL_ADDRESS myAddr;
    myAddr.QuadPart = ptr;
    return MmMapIoSpace(myAddr, size, MmNonCached);
}
#endif // _WIN32

#endif // _IO_H
