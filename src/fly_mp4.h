/**
 * @file fly_mp4.h
 *
 * The MP4 tag header file for pianobarfly.
 *
 * This is a limited implementation of MP4 tags designed specifically for 
 * pianobarfly needs.  Specifically this implmentation does not allow editing or
 * deleting of tags.  It only allows adding of tags to a file for which it is
 * assumed no metadata tags exist.  Once added a tag cannot be edited.  For
 * example you can not add the artist tag then later decide to change it by
 * re-adding it.
 *
 * To sum it up do not use this API to edit MP4 metadata.  It will screw up the
 * file!
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

#ifndef _FLY_MP4_H
#define _FLY_MP4_H

#ifdef ENABLE_FAAD

#include <stdint.h>

/**
 * The MP4 tag structure.  This structure represents a set of MP4 metadata tags.
 */
struct BarFlyMp4Tag;
typedef struct BarFlyMp4Tag BarFlyMp4Tag_t;

/**
 * Adds a grouping to the tag.  Do not call this function more than once.  Calli
 * it a second time will add a second grouping tag and possibly corrupt the file
 *
 * @param tag A pointer to the tag.
 * @param grouping A string containing the grouping name.
 * @param settings A pointer to the application's settings structure.
 * @return If the album is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddGrouping(BarFlyMp4Tag_t* tag, char const* grouping,
		BarSettings_t const* settings);

/**
 * Adds an album to the tag.  Do not call this function more than once.  Calling
 * it a second time will add a second album tag and possibly corrupt the file.
 *
 * @param tag A pointer to the tag.
 * @param album A string containing the album name.
 * @param settings A pointer to the application's settings structure.
 * @return If the album is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddAlbum(BarFlyMp4Tag_t* tag, char const* album,
		BarSettings_t const* settings);

/**
 * Adds an artist name to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second artist tag and possibly corrupt
 * the file.
 *
 * @param tag A pointer to the tag.
 * @param artist A string containing the artist name.
 * @param settings A pointer to the application's settings structure.
 * @return If the artist is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddArtist(BarFlyMp4Tag_t* tag, char const* artist,
		BarSettings_t const* settings);

/**
 * Adds cover art to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second cover art tag and possibly corrupt
 * the file.
 *
 * In a full MP4 implementation multiple images could be added to the cover art
 * tag.  This implemntation does not allow this.  The image that is added is
 * used as the front cover.
 *
 * @param tag A pointer to the tag.
 * @param cover_art A buffer containing the cover art image.
 * @param cover_size The size of the buffer in bytes.
 * @param settings A pointer to the application's settings structure.
 * @return If the artist is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddCoverArt(BarFlyMp4Tag_t* tag, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings);

/**
 * Adds the disk number to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second disk tag and possibly corrupt the
 * file.
 *
 * @param tag A pointer to the tag.
 * @param disk The disk number.
 * @param settings A pointer to the application's settings structure.
 * @return If the disk is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddDisk(BarFlyMp4Tag_t* tag, short unsigned disk,
		BarSettings_t const* settings);

/**
 * Adds a song title to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second title tag and possibly corrupt
 * the file.
 *
 * @param tag A pointer to the tag.
 * @param title A string containing the song title.
 * @param settings A pointer to the application's settings structure.
 * @return If the title is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddTitle(BarFlyMp4Tag_t* tag, char const* title,
		BarSettings_t const* settings);

/**
 * Adds the track number to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second track tag and possibly corrupt
 * the file.
 *
 * @param tag A pointer to the tag.
 * @param track The track number.
 * @param settings A pointer to the application's settings structure.
 * @return If the track is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddTrack(BarFlyMp4Tag_t* tag, short unsigned track,
		BarSettings_t const* settings);

/**
 * Adds a year to the tag.  Do not call this function more than once.
 * Calling it a second time will add a second year tag and possibly corrupt
 * the file.
 *
 * @param tag A pointer to the tag.
 * @param year The year.
 * @param settings A pointer to the application's settings structure.
 * @return If the year is successfully added to the tag 0 is returned
 * otherwise -1 is returned.
 */
int BarFlyMp4TagAddYear(BarFlyMp4Tag_t* tag, short unsigned year,
		BarSettings_t const* settings);

/**
 * Close an MP4 tag and free assciated resources.
 *
 * @param tag A pointer to a tag opened with BarFlyMp4TagOpen().
 */
void BarFlyMp4TagClose(BarFlyMp4Tag_t* tag);

/**
 * Open a MP4 tag from the given file.
 *
 * @param file_path The path to the MP4 file.
 * @param settings A pointer to the application's settings structure.
 * @return Upon success a pointer to the tag will be returned.  Otherwise, NULL
 * is returned.
 */
BarFlyMp4Tag_t* BarFlyMp4TagOpen(char const* file_path,
		BarSettings_t const* settings);

/**
 * Writes the contents of the tag to the MP4 file.  This function must be called
 * only once for a tag as calling it more than that will result in duplicate
 * tags written to the audio file potentially corrupting it.
 *
 * Once successfully tagged the original MP4 file will be overwritten and the
 * tag's reference to this file closed.  The tag must then be closed. 
 *
 * @param tag A pointer to the tag to be written.
 * @param settings A pointer to the application's settings structure.
 * @return If successful 0 is returned otherwise -1 is returned.
 */
int BarFlyMp4TagWrite(BarFlyMp4Tag_t* tag, BarSettings_t const* settings);

#endif
#endif

// vim: set noexpandtab:
