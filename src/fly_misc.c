/**
 * @file fly_misc.c

 * This file contains the definitions of generic utility function for
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "fly_misc.h"

int BarFlyasprintf(char** strp, char const* fmt, ...)
{
	int size;
	va_list ap;

	va_start(ap, fmt);
	size = BarFlyvasprintf(strp, fmt, ap);
	va_end(ap);

	return size;
}

int BarFlyvasprintf(char** strp, char const* fmt, va_list ap)
{
	char* str = NULL;
	int size;
	va_list sizing_ap;

	assert(strp != NULL);

	va_copy(sizing_ap, ap);
	size = vsnprintf(str, 0, fmt, sizing_ap);
	
	str = malloc(size + 1);
	if (str == NULL) {
		goto error;
	}

	size = vsnprintf(str, size + 1, fmt, ap);
	if (size == -1) {
		goto error;
	}

	goto end;

error:
	if (str != NULL) {
		free(str);
	}

	size = -1;

end:
	va_end(sizing_ap);

	return size;
}

// vim: set noexpandtab:

