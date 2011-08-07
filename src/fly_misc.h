/**
 * @file fly_misc.h
 *
 * This file contains the declarations of generic utility function for
 * Pianobarfly.
 */

/*
 * Copyright (c) 2011
 * Author: Ted Jordan
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef _FLY_MISC_H
#define _FLY_MISC_H

#include <stdarg.h>

/**
 * Print to allocated string.  This function is an analog to the GNU C asprintf
 * function.  It is provided since not all platforms provide that function.
 *
 * @param strp A pointer to a string that will be allocated and be large enough
 * to hold the output including the terminating null byte.
 * @param fmt The format string.
 * @return On success the number of bytes printed is returned.  If an error
 * occurs -l is returned and strp is undefined.
 */
int BarFlyasprintf(char** strp, char const* fmt, ...);

/**
 * Print to allocated string.  This function is an analog to the GNU C vasprintf
 * function It is provided since not all platforms provide that function.
 *
 * @param strp A pointer to a string that will be allocated and be large enough
 * to hold the output including the terminating null byte.
 * @param fmt The format string.
 * @param ap The variable arguments list.
 * @return On success the number of bytes printed is returned.  If an error
 * occurs -l is returned and strp is undefined.
 */
int BarFlyvasprintf(char** strp, char const* fmt, va_list ap);

#endif /* _FLY_MISC_H */

// vim: set noexpandtab:
