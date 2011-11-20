/*
 *  log.h: Debug logging macros and helpers
 */

#ifndef _LOG_H_
#define _LOG_H_

#ifndef _WIN32
    #include <linux/device.h>
    #include <linux/input.h>
    #include <linux/sysfs.h>
    #include <linux/types.h>
#else // _WiN32
    #include "ntddk.h"
    #include "ntstrsafe.h"
#endif // _WIN32

#ifndef _WIN32

#define dbg(format, arg...) printk(KERN_ALERT "%s(%d) " format "\n" , __FUNCTION__ , __LINE__ , ## arg)
//#define dbg(format, arg...)
#define logit(format, arg...) printk(KERN_NOTICE "%s(%d) " format "\n" , __FUNCTION__ , __LINE__ , ## arg)
#define error(format, arg...) printk(KERN_WARNING "%s(%d) " format "\n" , __FUNCTION__ , __LINE__ , ## arg)

#define assert(expr) \
  if(unlikely(!(expr))) {                                   \
    error("Assertion failed! %s,%s,%s,line=%d\n", \
    #expr, __FILE__, __func__, __LINE__);                   \
  }

#else  // _WIN32

/* Attempt to write log info directly over the serial port,
   rather than via kernel printing.  Currently, does not work.
   Unclear if it's an implementation issue, or if other WinDbg
   messages pollute the serial protocol.

#define dbg(fmt, ...) do {\
    char buffer[1000]; \
    memset(buffer, 1000, 0); \
    RtlStringCchPrintfA(buffer, 1000, fmt "\n", __VA_ARGS__); \
    serial_out_string(buffer); \
    } while (0)

#define logit(fmt, ...)  do { \
    char buffer[1000]; \
    memset(buffer, 1000, 0); \
    RtlStringCchPrintfA(buffer, 1000, "LOG: %s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__); \
    serial_out_string(buffer); \
    } while(0)

#define error(fmt, ...) do { \
    char buffer[1000]; \
    memset(buffer, 1000, 0); \
    RtlStringCchPrintfA(buffer, 1000, "ERR: %s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__); \
    serial_out_string(buffer); \
    } while(0)

*/

// dbg() works just like printf
#define dbg(fmt, ...) do { \
    	DbgPrintEx(DPFLTR_IHVDRIVER_ID, \
    		        DPFLTR_ERROR_LEVEL, \
    				fmt "\n", __VA_ARGS__ ); \
    } while(0)


// dbg() but include file and line info
#define dbgfl(fmt, ...) do { \
    	DbgPrintEx(DPFLTR_IHVDRIVER_ID, \
    		        DPFLTR_ERROR_LEVEL, \
    			    "%s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    } while(0)

#define logit(fmt, ...)  do { \
    	DbgPrintEx(DPFLTR_IHVDRIVER_ID, \
    		        DPFLTR_ERROR_LEVEL, \
    			    "%s:%d: LOG: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    } while(0)

#define error(fmt, ...) do { \
    	DbgPrintEx(DPFLTR_IHVDRIVER_ID, \
    		        DPFLTR_ERROR_LEVEL, \
    			    "%s:%d: ERROR: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    } while(0)


#endif // _WIN32

extern void dump_bytes(unsigned char *bytes, int len);
extern void virt_through_pt(unsigned long cr3val, unsigned long virt);

/*
 * If one makes changes that crash the system, the kernel's normal
 * logging facilities are unavailable (i.e., system freezes or reboots
 * before the syslog is appended).  Here are some hideous functions
 * that spew things to the serial port.  I suspect there's a *right
 * way* to do this with existing kernel logging facilities, but I
 * don't know what it is.
 */

#ifndef _WIN32
static inline unsigned long get_esp(void){
    unsigned long esp;
    __asm__ __volatile__ ("movl %%esp, %0" : "=r" (esp) : );
    return esp;
}

static inline unsigned char inB(unsigned short port)
{
    unsigned char _v;

    __asm__ __volatile__ ("inb %w1, %0"
                          : "=a" (_v) : "Nd" (port));

    return _v;
}

static inline void outB(const unsigned short port, unsigned char value){
    asm volatile("outb %0,%1" :: "a"(value),"Nd"(port));
}

#else  // _WIN32

static __inline unsigned long get_esp(void){
    unsigned long myEsp; // "my" is important - VS will interpret "esp" or "Esp" in the register!
    __asm {
    	mov myEsp, esp
    }
    return myEsp;
}

static __inline unsigned char inB(unsigned short port)
{
    unsigned char val;

    __asm {
    	mov dx, port
    	in al, dx
    	mov val, al
    }

    return val;
}

static __inline void outB(const unsigned short port, unsigned char value){
    __asm {
    	mov al, value
    	mov dx, port
    	out dx, al
    }
}

#endif // _WIN32

#define SERIAL_BASE 0x3f8
static inline void serial_outchar(char c) {
    while (!(inB(SERIAL_BASE+0x5) & 0x20))
        ;
    outB(SERIAL_BASE, c);
}

static inline void serial_out_string(const char *value)
{
    for(; (*value) != '\0'; value++)
        serial_outchar(*value);
}

static inline void serial_outbyte(unsigned char b) {
    char c = '0';

    if((b >> 4) <= 9)
        c = (b >> 4) + '0';
    else
        c = (b >> 4) - 0xa + 'a';

    serial_outchar(c);

    if((b & 0xf) <= 9)
        c = (b & 0xf) + '0';
    else
        c = (b & 0xf) - 0xa + 'a';

    serial_outchar(c);
}

static inline void serial_outlong(unsigned long val) {
    int i;
    char c;

    for(i=3; i>=0; i--) {
        c = (char)(val >> (8*i));
        serial_outbyte(c);
    }

    serial_outchar('\n');
    serial_outchar('\r');
}

/* no newline; space instead */
static inline void serial_outlong_nn(unsigned long val) {
    int i;
    char c;

    for(i=3; i>=0; i--) {
        c = (char)(val >> (8*i));
        serial_outbyte(c);
    }

    serial_outchar(' ');
}

static inline void serial_print_stack(int items) {
    unsigned int *esp;
    int i;

    esp = (unsigned int *)get_esp();
    serial_out_string("Printing stack from ESP ");
    serial_outlong((unsigned long)esp);
    for(i=0; i<items; i++) {
        serial_outlong_nn((unsigned long)esp);
        serial_outlong(*esp);
        esp++; /* should advance 4 bytes */
    }
}

#define serial_printk(s, l) \
    serial_out_string("SPK: "); \
    serial_out_string(s); \
    serial_outlong(l);


#endif /* _LOG_H_ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
