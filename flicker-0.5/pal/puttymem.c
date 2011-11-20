/*
 * Platform-independent routines shared between all PuTTY programs.
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
 * puttymem.c: Modified for Flicker.
 */

#include "string.h" /* strlen etc. */
#include "util.h" /* For perf stuff */
#include "puttymem.h"
/* ----------------------------------------------------------------------
 * My own versions of malloc, realloc and free. Because I want
 * malloc and realloc to bomb out and exit the program if they run
 * out of memory, realloc to reliably call malloc if passed a NULL
 * pointer, and free to reliably do nothing if passed a NULL
 * pointer. We can also put trace printouts in, if we need to; and
 * we can also replace the allocator with an ElectricFence-like
 * one.
 */

/* int totalmem = 0; */
void *safemalloc(size_t n, size_t size)
{
    void *p;

#ifdef PERFCRIT
    struct st_timer_vars tv;
    start_timer(&tv);
#endif // PERFCRIT

    	 /*totalmem += size;
    	 printf("Allocated %d bytes so far\n", totalmem);*/
    if (n > INT_MAX / size) {
    	p = NULL;
    } else {
    	size *= n;
    	p = static_malloc(size);
    }

#ifdef PERFCRIT
    stop_timer(&tv);
    update_sum(&g_perf.sum_rsag_malloc, &tv);
#endif // PERFCRIT

    if (!p) {
    	return NULL;
    }
    return p;
}


void safefree(void *ptr)
{
    if (ptr) {
    	static_free(ptr);
    }
}


/***********************************************************************
 * Replacements for string.h (see man pages for details)
 ***********************************************************************/



/* taken from glibc */
char *
simple_stpncpy (char *dst, const char *src, size_t n)
{
  while (n--)
    if ((*dst++ = *src++) == '\0')
      {
        size_t i;

        for (i = 0; i < n; ++i)
          dst[i] = '\0';
        return dst - 1;
      }
  return dst;
}

size_t
simple_strcspn (const char *s, const char *rej)
{
  const char *r, *str = s;
  char c;

  while ((c = *s++) != '\0')
    for (r = rej; *r != '\0'; ++r)
      if (*r == c)
        return s - str - 1;
  return s - str - 1;
}

#ifndef TRUSTSIM
char *strcat(char *s, char *append) {
    char *save = s;

    for (; *s; ++s);
    while ((*s++ = *append++) != 0);
    return(save);
}
#endif // TRUSTSIM

/* append a single char by making a temporary array */
char *strcat1(char *s, char append) {
    char tmp[2];
    tmp[0] = append;
    tmp[1] = '\0';
    return strcat(s, tmp);
}

char *strstr (const char *str1, const char *str2) {
    char *cp = (char *) str1;
    char *s1, *s2;

    if ( !*str2 )
        return((char *)str1);

    while (*cp) {
        s1 = cp;
        s2 = (char *) str2;

        while( *s1 && *s2 && !(*s1-*s2) )
            s1++, s2++;

        if (!*s2)
            return(cp);

        cp++;
    }

    return NULL;
}

#ifndef TRUSTSIM
char *strcpy(char *dst, const char *src)
{
    char *curr_dst, *curr_src;
    if((dst != NULL) && (src != NULL)){
        for(curr_dst = dst, curr_src = (char *) src; *curr_src != '\0'; curr_src++, curr_dst++){
            *curr_dst = *curr_src;
        }
        *curr_dst = '\0';
    }
    return dst;
}

int atoi(const char *s) {
    int i,num=0,flag=0;
    for(i=0;i<=strlen(s);i++) {
        if(s[i] >= '0' && s[i] <= '9')
            num = num * 10 + s[i] -'0';
        else if(s[0] == '-' && i==0)
            flag =1;
        else break;
    }

    if(flag == 1)
        num = num * -1;

    return num;
}

#endif // TRUSTSIM
