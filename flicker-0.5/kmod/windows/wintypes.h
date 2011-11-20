#ifndef _WIN_TYPES_H
#define _WIN_TYPES_H

// Basic types not included in standard Windows driver build environment
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned long
#define uint64_t unsigned long long

// Map common Linux idioms to corresponding Windows versions
#define inline __inline
#define bool int
#define false 0
#define true 1
#define assert ASSERT

#define write_cr0(x) __writecr0((x))
#define write_cr3(x) __writecr3((x))
#define write_cr4(x) __writecr4((x))
#define read_cr0() __readcr0()
#define read_cr3() __readcr3()
#define read_cr4() __readcr4()

#define rdmsrl(msr, val) (val) = __readmsr(msr)
#define wrmsrl(msr, val) __writemsr(msr, val)

#define _PAGE_PRESENT  0x001
#define _PAGE_RW	   0x002
#define _PAGE_PCD	   0x010
#define _PAGE_ACCESSED 0x020


/*
#define rdmsrl(msr, val) (val) = win_read_msrl(msr)
static inline win_read_msrl(uint32_t msr) {
	uint32_t val;
	__asm {
		mov ecx, msr
		rdmsr
		mov val, eax
	}
}

#define wrmsrl(msr, val) win_read_msrl(msr, val)
static inline win_write_msrl(uint32_t msr, uint64_t val) {
	uint32_t val_low, val_high;
	val_low = (uint32_t) val;
	val_high = (uint32_t) (val >> 32);

	__asm {
		mov ecx, msr
		mov eax, val_low
		mov edx, val_high
		wrmsr		
	}
}
*/
#endif  //_WIN_TYPES_H
