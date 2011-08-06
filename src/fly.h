/**
 * @file fly.h
 */

/*
 * Copyright (c) 2010-2011
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

#ifndef _FLY_H
#define _FLY_H

#include <piano.h>
#include <stdbool.h>
#include <stdio.h>

#include "settings.h"

/**
 * The length of the buffers for the artist, album, and title.
 */
#define BAR_FLY_NAME_LENGTH 256

/**
 * Number of bytes used when copying blocks of audio data between files.
 */
#define BAR_FLY_COPY_BLOCK_SIZE (100 * 1024)

/**
 * The status of the recoding.
 */
typedef enum BarFlyStatus {
	NOT_RECORDING = 0,
	NOT_RECORDING_EXIST,
	RECORDING,
	DELETING,
	TAGGING
} BarFlyStatus_t;

/**
 * The BarFly structure.  The artist, album, and title are saved here in
 * addition to the app->playlist.  This is done because this information may be
 * needed after the playlist is destroyed.  For example if the station is
 * changed.
 */
typedef struct BarFly {
	/**
	 * The stream to which the audio stream is written.
	 */
	FILE* audio_file;

	/**
	 * The audio file path.
	 */
	char* audio_file_path;

	/**
	 * The format of the audio file being played.
	 */
	PianoAudioFormat_t audio_format;

	/**
	 * Once a song has been completely recorded this variable is set to true.
	 * This variable is used to tell the BarFlyClose() function the song was
	 * completely recorded and thus the file should not be deleted.  If the
	 * audio file already existed when BarFlyOpen() was called this variable is
	 * set to true from the start.
	 */
	bool completed;
	
	/**
	 * The song's artist.
	 */
	char artist[BAR_FLY_NAME_LENGTH];

	/**
	 * The song's album.
	 */
	char album[BAR_FLY_NAME_LENGTH];

	/**
	 * The song's title.
	 */
	char title[BAR_FLY_NAME_LENGTH];

	/**
	 * The year.  A year of 0 means the year could not be found.
	 */
	short unsigned year;

	/**
	 * The track number.  A track of 0 means the track number could not be
	 * found.
	 */
	short unsigned track;

	/**
	 * The disc number.  A disc of 0 means the disc number could not be found.
	 */
	short unsigned disc;

	/**
	 * Cover art URL.
	 */
	char* cover_art_url;

	/**
	 * The current status of the recording.
	 */
	BarFlyStatus_t status;
} BarFly_t;


/**
 * Closes the file stream and writes a metadata tag to the file.  If the song
 * was not fully recorded the file is deleted.
 *
 * @param fly Pointer to the BarFly_t structure to be closed.
 * @param settings Pointer to the application settings structure.
 * @return If the file stream was successfully closed and the tag was written
 * to it or the file was deleted.
 */
int BarFlyClose(BarFly_t* fly, BarSettings_t const* settings);

/**
 * Finalize the BarFly module.  Cleans up anything allocated by the
 * BarFlyInit() function.
 */
void BarFlyFinalize(void);

/**
 * Returns a string giving the status of the recording.  
 *
 * @return The status of the recording.
 */
char const* BarFlyStatusGet(BarFly_t* fly);

/**
 * Initialize the BarFly module.
 *
 * @param settings Pointer to the settings structure that has already been
 * filled out.
 * @return Upon success 0 is returned otherwise -1 is returned.
 */
int BarFlyInit(BarSettings_t const* settings);

/**
 * Populates a BarFly structure, opening the associated file for writing.  The
 * file will be located relative to the current working directory under an
 * artist and album subdirectories.  The file will have the artist's name and
 * song title.  Characters not valid in file names and spaces will be replaced
 * by _.
 *
 * If the audio file already exists it will not be overwritten and the song
 * will be marked as completed so the file is not deleted when BarFlyClose() is
 * called.
 *
 * @param fly Pointer to the BarFly_t structure to be populated.
 * @param song The song for whom the file stream is to be opened.
 * @param settings Pointer to the application settings structure.
 * @return If the file was successfully opened for writing or it already
 * existed 0 is returned otherwise -1 is returned.
 */
int BarFlyOpen(BarFly_t* fly, PianoSong_t const* song,
		BarSettings_t const* settings);

/**
 * Writes a metadata tag to the file.  MP3 files will have an ID3 tag written to
 * then and MP4/AAC files will have an iTunes style tag written to them.  If the
 * fly->audio_file variable is NULL this function does nothing.
 *
 * Before the tag is written the file stream will be closed.  So this function
 * should only be called once the file has been completely recorded.
 *
 * The file will also be marked as complete so that it will not be deleted when
 * BarFlyClose() is called.
 *
 * @param fly Pointer to a BarFly_t structure.
 * @param settings Pointer to the application settings structure.
 * @return Upon success 0 is returned otherwise -1 is returned.
 */
int BarFlyTag(BarFly_t* fly, BarSettings_t const* settings);

/**
 * Writes the given buffer to the audio file.  If the fly->audio_file variable
 * is NULL nothing will be done.
 *
 * @param fly Pointer to the BarFly_t structure.
 * @param data The buffer to be written to the file.
 * @param data_size The size of the data buffer in bytes.
 * @return If the buffer was written successfully or fly->audio_file == NULL 0
 * will be returned, otherwise -1 is returned.
 */
int BarFlyWrite(BarFly_t* fly, void const* data, size_t data_size);

#endif /* _FLY_H */

// vim: set noexpandtab:
