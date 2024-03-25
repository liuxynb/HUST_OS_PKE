// thansk to  https://github.com/MrBad/vsscanf/blob/master/vsscanf.c


/**
 * vsscanf - a simple vsscanf() implementation, with very few dependencies.
 * Note: it does not support floating point numbers (%f), nor gnu "m" pointers.
 *
 * ---------------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2018 MrBadNews <viorel dot irimia at gmail dot come>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 */

#include "util/snscanf.h"

#ifndef isdigit
#define isdigit(c) (c >= '0' && c <= '9')
#endif

#ifndef isspace
#define isspace(c) (c == ' ' || c == '\f' || c == '\n' || \
                    c == '\r' || c == '\t' || c == '\v')
#endif

/**
 * Check if the first letter c of a string, can be the start of a valid
 *  integer in base n, having sign or not. 
 */
static int valid_sint(char c)
{
    if ((c >= '0' && c <= '9') || (c == '-' || c == '+'))
        return 0;
    return -1;
}

/**
 * Having a string, consumes width chars from it, and return an integer
 * in base base, having sign or not.
 * Will work for base 2, 8, 10, 16
 * For base 16 will skip 0x infront of number.
 * For base 10, if signed, will consider - infront of number.
 * For base 8, should skip 0
 * I should reimplement this using sets. 
 */
static long long get_int(const char **str)
{
    long long n = 0;
    int neg = 0;
    char c;

    for (n = 0; **str; (*str)++)
    {
        c = **str;
        if (neg == 0 && c == '-')
        {
            neg = 1;
            continue;
        }
        if (!isdigit(c))
        {
            if (c < 'A' || c > 'F')
                break;
        }
        n = 10 * n + c - '0';
    }
    if (neg && n > 0)
        n = -n;

    return n;
}

/**
 * Gets a string from str and puts it into ptr, if skip is not set
 * if ptr is NULL, it will consume the string, but not save it
 * if width is set, it will stop after max width characters.
 * if set[256] is set, it will only accept characters from the set,
 * eg: set['a'] = 1 - it will accept 'a'
 * otherwise it will stop on first space, or end of string.
 * Returns the number of characters matched
 */
static int get_str(const char **str, char *ptr, char *set, int width)
{
    int n, w, skip;
    unsigned char c;
    w = (width > 0);
    skip = (ptr == NULL);

    for (n = 0; **str; (*str)++, n++) {
        c = **str;
        if ((w && width-- == 0) || (!set && isspace(c)))
            break;
        if (set && (set[c] == 0))
            break;
        if (!skip)
            *ptr++ = c;
    }
    if (!skip)
        *ptr = 0;

    return n;
}

/* Flags */
#define F_SKIP   0001   // don't save matched value - '*'
#define F_ALLOC  0002   // Allocate memory for the pointer - 'm'
#define F_SIGNED 0004   // is this a signed number (%d,%i)?

/* Format states */
#define S_DEFAULT   0
#define S_FLAGS     1
#define S_WIDTH     2
#define S_PRECIS    3
#define S_LENGTH    4
#define S_CONV      5

/* Lenght flags */
#define L_CHAR      1
#define L_SHORT     2
#define L_LONG      3
#define L_LLONG     4
#define L_DOUBLE    5

/**
 * Shrinked down, vsscanf implementation.
 *  This will not handle floating numbers (yet), nor allocated (gnu) pointers.
 */
int vsscanf(const char *str, const char *fmt, va_list ap)
{
    size_t n = 0; // number of matched input items
    char state = S_DEFAULT;
    void *ptr;
    long long num;
    int base, sign, flags, width = 0, lflags;

    if (!fmt)
        return 0;

    for (; *fmt && *str; fmt++)
    {
        if (state == S_DEFAULT)
        {
            if (*fmt == '%')
            {
                flags = 0;
                state = S_CONV;
            }
            else if (isspace(*fmt))
            {
                while (isspace(*str))
                    str++;
            }
            else
            {
                if (*fmt != *str++)
                    break;
            }
            continue;
        }
        if (state == S_CONV)
        {
            if (strchr("di", *fmt))
            {
                state = S_DEFAULT;

                /* Numbers should skip starting spaces "  123l",
                 *  strings, chars not
                 */
                while (isspace(*str))
                    str++;

                if (valid_sint(*str) < 0)
                    break;

                num = get_int(&str);
                if (flags & F_SKIP)
                {
                    continue;
                }
                ptr = va_arg(ap, void *);
                *(int *)ptr = num;
                n++;
            }
            else if ('c' == *fmt)
            {
                state = S_DEFAULT;
                if (flags & F_SKIP)
                {
                    str++;
                    continue;
                }
                ptr = va_arg(ap, void *);
                *(char *)ptr = *(str)++;
                n++;
            }
            else if ('s' == *fmt)
            {
                state = S_DEFAULT;
                if (flags & F_SKIP)
                {
                    get_str(&str, NULL, NULL, width);
                    continue;
                }
                ptr = va_arg(ap, void *);
                get_str(&str, (char *)ptr, NULL, width);
                n++;
            }
            else if ('%' == *fmt)
            {
                state = S_DEFAULT;
                if (*str != '%')
                    break;
                str++;
            }
            else
            {
                break;
            }
        }
    }

    return n;
}