/*
 * PuTTY memory-handling header.
 *
 * PuTTY is copyright 1997-2007 Simon Tatham.
 *
 * Portions copyright Robert de Bath, Joris van Rantwijk, Delian
 * Delchev, Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry,
 * Justin Bradford, Ben Harris, Malcolm Smith, Ahmad Khalifa, Markus
 * Kuhn, and CORE SDI S.A.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * puttymem.h: Modified for Flicker.
 */

#ifndef PUTTY_PUTTYMEM_H
#define PUTTY_PUTTYMEM_H

#include "malloc.h"

/* #define memset myMemset */
/* #define memcpy myMemcpy */
/* #define memmove myMemmove */

#define smalloc(z) safemalloc(z,1)
#define snmalloc safemalloc
#define sfree safefree

#ifndef size_t
typedef unsigned int size_t;
#endif /* size_t */

#ifndef INT_MAX
#define INT_MAX 0x7FFFFFFF
#endif

void *safemalloc(size_t, size_t);
void safefree(void *);

/*
 * Direct use of smalloc within the code should be avoided where
 * possible, in favour of these type-casting macros which ensure
 * you don't mistakenly allocate enough space for one sort of
 * structure and assign it to a different sort of pointer.
 */
#define snew(type) ((type *)snmalloc(1, sizeof(type)))
#define snewn(n, type) ((type *)snmalloc((n), sizeof(type)))

/***********************************************************************
 * Replacements for string.h (see man pages for details)
 ***********************************************************************/

/* Removing the following because they're in string.h:
void * memset(void *b, int c, size_t len);
void * memcpy(void *dst, const void *src, size_t len);
*/
void * memmove(void *dst, const void *src, size_t len);

#define strncmp(s1, s2, size) memcmp(s1, s2, size)

size_t strnlen(const char *s, size_t maxlen);

#define stpncpy(dst, src, n) simple_stpncpy(dst, src, n)

char *simple_stpncpy (char *dst, const char *src, size_t n);

#define strcspn(s, rej) simple_strcspn(s, rej)
size_t simple_strcspn (const char *s, const char *rej);
char *strcat(char *s, char *append);
char *strcat1(char *s, char append);
char *strstr (const char *str1, const char *str2);
char *strcpy (char *dst, const char *src);
int atoi(const char *s);
#endif
