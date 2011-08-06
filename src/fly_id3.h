/**
 * @file fly_id3.h
 *
 * Declaration of the pianobarfly ID3 helper functions.  These are helper
 * functions to assist with creating an ID3 tag using the libid3tag library.
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

#ifndef _FLY_ID3_H
#define _FLY_ID3_H

#if defined ENABLE_MAD && defined ENABLE_ID3TAG

#include <id3tag.h>
#include <stdint.h>

/**
 * Creates and attaches an image frame with the cover art to the given tag.
 *
 * @param tag A pointer to the tag.
 * @param cover_art A buffer containing the cover art image.
 * @param cover_size The size of the buffer.
 * @param settings A pointer to the application settings structure.
 * @return If successful 0 is returned otherwise -1 is returned.
 */
int BarFlyID3AddCover(struct id3_tag* tag, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings);

/**
 * Creates and attaches a frame of the given type to the tag with the given
 * string value.  All values are written to the string list field of the frame.
 *
 * @param tag A pointer to the tag.
 * @param type A string containing the frame type, ex. TIT2.
 * @param value A string containing the value.
 * @param settings A pointer to the application settings structure.
 * @return If the frame was successfully added 0 is returned, otherwise -1 is
 * returned.
 */
int BarFlyID3AddFrame(struct id3_tag* tag, char const* type,
		char const* value, BarSettings_t const* settings);

/**
 * Physically writes the ID3 tag to the audio file.  A temproary file is create
 * into which the tag is written.  After which the contents of the audio file is
 * copied into it.  Once the temporary file is complete the audio file is
 * overwritten with it.
 *
 * @param file_path A pointer to a string containing the path to the audio file.
 * @param tag A pointer to the tag structure to be written to the file.
 * @param settings A pointer to the application settings structure.
 * @return If successful 0 is returned otherwise -1 is returned.
 */
int BarFlyID3WriteFile(char const* file_path, struct id3_tag const* tag,
		BarSettings_t const* settings);



#endif
#endif

// vim: set noexpandtab:
