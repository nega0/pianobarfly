/**
 * @file fly.c
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

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <piano.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <waitress.h>

#include "fly.h"
#include "fly_id3.h"
#include "fly_mp4.h"
#include "settings.h"
#include "ui.h"

/**
 * Barfly Waitress handle used to fetch the album cover and year.
 */
static WaitressHandle_t fly_waith;


/**
 * Retreives the contents served up by the given URL.
 *
 * In the event of an error the contents of buffer and size are not changed.
 *
 * @param url The URL whose's contents will be fetched.
 * @param buffer A pointer to a buffer that upon success will contain the
 * contents served by the URL.  This buffer must be freed.  The buffer will
 * always be '\0' terminated because libwaitress was originally designed to
 * only fetch string buffers.
 * param size A pointer to a size_t variable that will be set to the size of
 * the returned buffer.  This size does not include the '\0' character at the
 * end of the buffer.  This argument may be NULL if the size of the buffer
 * does not matter as is often the case with string buffers.
 * #param settings Pointer to the application settings structure.
 * @return Upon success 0 is returned otherwise -1 is returned.
 */
static int _BarFlyFetchURL(char const* url, uint8_t** buffer, size_t* size,
		BarSettings_t const* settings);

/**
 * Deletes the audio file.  If the parent directories are empty they too will
 * be deleted.
 *
 * @param fly Pointer to the barfly structure.
 * @param settings Pointer to the application settings structure.
 * @return If the file and parent directories were deleted successfully 0 is
 * returned otherwise -1 is returned.
 */
static int _BarFlyFileDelete(BarFly_t const* fly,
		BarSettings_t const* settings);

/**
 * Returns the path of the audio file.
 *
 * @param artist A string containing the artist.
 * @param album A string containing the album.
 * @param title A string containing the title.
 * @param year The year the song was released.
 * @param track The track number of the song.
 * @param disc The disc number of the song.
 * @param audio_format The audio format of the song.
 * @param settings Pointer to the applicaton settings structure.
 * @return The string containing the audio file path.  This string must be
 * freed when done.
 */
static char* _BarFlyFileGetPath(char const* artist, char const* album,
		char const* title, short unsigned year, short unsigned track,
		short unsigned disc, PianoAudioFormat_t audio_format,
		BarSettings_t const* settings);

/**
 * Opens a file stream for writing.  If the file already exists the stream will
 * not be opened.  Any parent directories in the path will be created if they
 * don't exist.
 *
 * @param file A pointer to a pointer to a file stream that will be populated
 * with the opened file stream.
 * @param path A pointer to a string contining the path to the file to be
 * opened.
 * @param settings Pointer to the applicaton settings structure.
 * @return If the stream is opened successfully 0 is returned.  If the file
 * already exists -2 is returned.  Otherwise -1 is returned.
 */
static int _BarFlyFileOpen(FILE** file, char const* path,
		BarSettings_t const* settings);

/**
 * Opens a file stream using fopen().  Errors caused by signal interrupts are
 * ignored.
 *
 * This function may fail and set errno for any of the errors specified by
 * fopen().
 *
 * @param path The path to the file to open.
 * @param mode The read/write mode with which to open the file.
 * @return A pointer to the open file.
 */
static FILE* _BarFlyFileOpenStream(char const* path, char const* mode);

/**
 * Copies at most n bytes of the source str to the destination.  Then changes
 * the /\|&'?"<>:* and space characters to _.  A terminating nul character will
 * always be added to the end of the destination string.
 *
 * @param dest Pointer to the destination buffer.
 * @param src Pointer to the source string.
 * @param n The number of bytes to be copied.
 * @param settings Pointer to the application settings structure.
 * @return The length of the destination string not including the \0.
 */
static size_t _BarFlyNameTranslate(char* dest, char const* src, size_t n,
		BarSettings_t const* settings);

/**
 * Parses out the cover art URL from the album detail HTML.
 *
 * @param album_html A buffer containing the album detail HTML.
 * @param settings Pointer to the application settings structure.
 * @return A pointer to a string containing the URL will be returned if it was
 * parsed successfully.  Otherwise NULL is returned.  The returned string will
 * need to be freed.
 */
static char* _BarFlyParseCoverArtURL(char const* album_html,
		BarSettings_t const* settings);

/**
 * Parses out the track and disc numbers from the album explorer XML.  In the
 * event of an error the track_num and disc_num variables will not be modified.
 *
 * @param title The title string of the song.
 * @param album_xml A buffer containing the album explorer XML.
 * @param track_num A pointer to a variable that will be assigned the track
 * number.
 * @param disc_num A pointer to a variable that will be assigned the disc
 * number.
 * @param settings Pointer to the application settings structure.
 * @return If successful 0 is returned otherwise -1 is returned.
 */
static int _BarFlyParseTrackDisc(char const* title, char const* album_xml,
		short unsigned* track_num, short unsigned* disc_num,
		BarSettings_t const* settings);

/**
 * Parses out the release year from the album detail HTML.
 *
 * @param album_html A buffer containing the album detail HTML.
 * @param year A pointer to a variable to be populated with the year.
 * @param settings Pointer to the application settings structure.
 * @return If the year was obtained successfully 0 is returned.  Otherwise -1 is
 * returned.
 */
static int _BarFlyParseYear(char const* album_html, short unsigned* year,
		BarSettings_t const* settings);

/**
 * Get the large cover art image from Pandora.
 *
 * In the event of an error the cover_art and cover_size are not modified.
 *
 * @param cover_art A pointer to a buffer that upon success will contain the
 * cover art image.
 * @param cover_size A pointer to a variable that upon success will be assigned
 * the size of the buffer.
 * @param url The URL from which the cover art can be retrevied.
 * @param settings Pointer to the application settings structure.
 * @return Upon success 0 is returned otherwise -1 is returned.
 */
static int _BarFlyTagFetchCover(uint8_t** cover_art, size_t* cover_size,
		char const* url, BarSettings_t const* settings);

#ifdef ENABLE_ID3TAG
/**
 * Writes an ID3 metadata tag to the audio file.
 *
 * @param fly A pointer to a barfly structure.
 * @param cover_art A buffer containing the album cover in JPEG format.  If this
 * is NULL no cover will be written.
 * @param cover_size The size of the cover art buffer in bytes.
 * @param settings Pointer to the application settings structure.
 * @return If the tag was successfully written 0 will be returned, otherwise -1
 * is returned.
 */
static int _BarFlyTagID3Write(BarFly_t const* fly, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings);
#endif

#ifdef ENABLE_FAAD
/**
 * Writes an iTunes style metadata tag to an MP4 file.
 *
 * @param fly A pointer to a barfly structure.
 * @param cover_art A buffer containing the album cover in JPEG format.  If this
 * is NULL no cover will be written.
 * @param cover_size The size of the cover art buffer in bytes.
 * @param settings Pointer to the application settings structure.
 * @return If the tag was successfully written 0 will be returned, otherwise -1
 * is returned.
 */
static int _BarFlyTagMp4Write(BarFly_t const* fly, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings);
#endif

/**
 * Writes a metadata tag to given song's audio file.  MP3 files will be tagged
 * with an ID3v2 tag.  AAC files will be tagged with an iTunes style tag.
 *
 * @param fly A pointer to a barfly structure.
 * @param settings Pointer to the application settings structure.
 * @return Upon successfully writing out the tag 0 is returned otherwise -1 is
 * returned.
 */
static int _BarFlyTagWrite(BarFly_t const* fly, BarSettings_t const* settings);


static int _BarFlyFetchURL(char const* url, uint8_t** buffer, size_t* size,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	char status;
	WaitressReturn_t status_waith;
	uint8_t* tmp_buffer = NULL;
	size_t tmp_size;

	assert(url != NULL);
	assert(buffer != NULL);
	assert(settings != NULL);

	/*
	 * Set the URL in the waitress handler and fetch the buffer.
	 */
	status = WaitressSetUrl(&fly_waith, url);
	if (status != 1) {
		BarUiMsg(settings, MSG_DEBUG, "Invalid URL (%s).\n", url);
		goto error;
	}

	status_waith = WaitressFetchBufEx(&fly_waith, (char**)&tmp_buffer,
		&tmp_size);
	if ((status_waith != WAITRESS_RET_OK) || (tmp_buffer == NULL)) {
		BarUiMsg(settings, MSG_DEBUG, "Failed to fetch the URL contents "
				"(url = %s, waitress status = %d).\n", url, status_waith);
		goto error;
	}

	*buffer = tmp_buffer;
	tmp_buffer = NULL;

	if (size != NULL) {
		*size = tmp_size;
	}

	goto end;

error:
	exit_status = -1;

end:
	if (tmp_buffer != NULL) {
		free(tmp_buffer);
	}

	return exit_status;
}

static int _BarFlyFileDelete(BarFly_t const* fly,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	char* dir_path = NULL;
	char* ptr;

	assert(fly != NULL);
	assert(settings != NULL);

	if (fly->audio_file_path != NULL) {
		/*
		 * Delete the file.
		 */
		BarUiMsg(settings, MSG_DEBUG, "Deleting partially recorded file (%s).\n",
				fly->audio_file_path);
		status = unlink(fly->audio_file_path);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Failed to delete the partially "
					"recorded file (%s).\n", fly->audio_file_path);
			goto error;
		}

		/*
		 * Delete any empty parent directories.
		 */
		dir_path = strdup(fly->audio_file_path);
		if (dir_path == NULL) {
			BarUiMsg(settings, MSG_ERR,
					"Error copying the file path (%s) (%d:%s).\n",
					fly->audio_file_path, errno, strerror(errno));
			goto error;
		}

		ptr = strrchr(dir_path, '/');
		while (ptr != NULL) {
			*ptr = '\0';

			status = rmdir(dir_path);
			if ((status != 0) && (errno != ENOTEMPTY) && (errno != EEXIST)) {
				BarUiMsg(settings, MSG_ERR,
						"Failed to delete the empty artist directory "
						"(%s) (%d:%s).\n", dir_path, errno, strerror);
				goto error;

			}

			ptr = strrchr(dir_path, '/');
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	if (dir_path != NULL) {
		free(dir_path);
	}

	return exit_status;
}

static char* _BarFlyFileGetPath(char const* artist, char const* album,
		char const* title, short unsigned year, short unsigned track,
		short unsigned disc, PianoAudioFormat_t audio_format,
		BarSettings_t const* settings)
{
	char* path = NULL;
	size_t path_length;
	char path_artist[BAR_FLY_NAME_LENGTH];
	char path_album[BAR_FLY_NAME_LENGTH];
	char path_title[BAR_FLY_NAME_LENGTH];
	char const* extension;
	char const* file_pattern_ptr;
	size_t count;
	char* path_ptr;

	assert(artist != NULL);
	assert(album != NULL);
	assert(title != NULL);
	assert(settings != NULL);

	/*
	 * Get the artist, album, and title.  Translate each of the characters
	 * we don't want to _.
	 */
	_BarFlyNameTranslate(path_artist, artist, BAR_FLY_NAME_LENGTH, settings);
	_BarFlyNameTranslate(path_album, album, BAR_FLY_NAME_LENGTH, settings);
	_BarFlyNameTranslate(path_title, title, BAR_FLY_NAME_LENGTH, settings);

	/*
	 * Get the extension.
	 */
	switch (audio_format) {
		#ifdef ENABLE_FAAD
		case PIANO_AF_AACPLUS:
			extension = ".m4a";
			break;
		#endif

		#ifdef ENABLE_MAD
		case PIANO_AF_MP3:
		case PIANO_AF_MP3_HI:
			extension = ".mp3";
			break;
		#endif

		default:
			BarUiMsg(settings, MSG_ERR, "Unsupported audio format!\n");
			goto error;
			break;
	}

	/*
	 * Calculate the length of the path.
	 */
	path_length = 0;
	file_pattern_ptr = settings->audioFileName;
	while (*file_pattern_ptr != '\0') {
		/*
		 * Get the length of everything up to the next '%'.
		 */
		count = strcspn(file_pattern_ptr, "%");
		path_length += count;
		file_pattern_ptr += count;

		/*
		 * Get the length of each substitution.  The track is always at least
		 * 2 digits.
		 */
		if (*file_pattern_ptr != '\0') {
			if (strncmp("%artist", file_pattern_ptr, 7) == 0) {
				path_length += strlen(path_artist);
				file_pattern_ptr += 7;
			} else if (strncmp("%album", file_pattern_ptr, 6) == 0) {
				path_length += strlen(path_album);
				file_pattern_ptr += 6;
			} else if (strncmp("%title", file_pattern_ptr, 6) == 0) {
				path_length += strlen(path_title);
				file_pattern_ptr += 6;
			} else if (strncmp("%year", file_pattern_ptr, 5) == 0) {
				path_length += snprintf(NULL, 0, "%hu", year);
				file_pattern_ptr += 5;
			} else if (strncmp("%track", file_pattern_ptr, 6) == 0) {
				path_length += snprintf(NULL, 0, "%02hu", track);
				file_pattern_ptr += 6;
			} else if (strncmp("%disc", file_pattern_ptr, 5) == 0) {
				path_length += snprintf(NULL, 0, "%hu", disc);
				file_pattern_ptr += 5;
			} else {
				file_pattern_ptr += 1;
			}
		}
	}
	path_length += strlen(extension);

	/*
	 * Allocate space for the path.
	 */
	path = malloc(path_length + 1);
	if (path == NULL) {
		BarUiMsg(settings, MSG_ERR,
				"Error allocating memory (%d bytes) (%d:%s).\n",
				path_length + 1, errno, strerror(errno));
		goto error;
	}

	/*
	 * Populate the buffer with the path.
	 */
	file_pattern_ptr = settings->audioFileName;
	path_ptr = path;
	while (*file_pattern_ptr != '\0') {
		/*
		 * Copy any any characters before the next substitution.
		 */
		count = strcspn(file_pattern_ptr, "%");
		strncpy(path_ptr, file_pattern_ptr, count);
		file_pattern_ptr += count;
		path_ptr += count;

		/*
		 * Substitute in the value.
		 */
		if (*file_pattern_ptr != '\0') {
			if (strncmp("%artist", file_pattern_ptr, 7) == 0) {
				strcpy(path_ptr, path_artist);
				file_pattern_ptr += 7;
				path_ptr += strlen(path_artist);
			} else if (strncmp("%album", file_pattern_ptr, 6) == 0) {
				strcpy(path_ptr, path_album);
				file_pattern_ptr += 6;
				path_ptr += strlen(path_album);
			} else if (strncmp("%title", file_pattern_ptr, 6) == 0) {
				strcpy(path_ptr, path_title);
				file_pattern_ptr += 6;
				path_ptr += strlen(path_title);
			} else if (strncmp("%year", file_pattern_ptr, 5) == 0) {
				sprintf(path_ptr, "%hu", year);
				file_pattern_ptr += 5;
				path_ptr += snprintf(NULL, 0, "%hu", year);
			} else if (strncmp("%track", file_pattern_ptr, 6) == 0) {
				sprintf(path_ptr, "%02hu", track);
				file_pattern_ptr += 6;
				path_ptr += snprintf(NULL, 0, "%02hu", track);
			} else if (strncmp("%disc", file_pattern_ptr, 5) == 0) {
				sprintf(path_ptr, "%hu", disc);
				file_pattern_ptr += 5;
				path_ptr += snprintf(NULL, 0, "%hu", disc);
			} else {
				file_pattern_ptr += 1;
			}
		}
	}
	strcpy(path_ptr, extension);

	goto end;

error:
	if (path != NULL) {
		free(path);
		path = NULL;
	}

end:
	return path;
}

static int _BarFlyFileOpen(FILE** file, char const* path,
		BarSettings_t const* settings)
{
	FILE *tmp_file = NULL;
	int exit_status = 0;
	int status;
	char* dir_path = NULL;
	char* ptr;

	assert(file != NULL);
	assert(path != NULL);
	assert(settings != NULL);

	/*
	 * Create any parent directories.
	 */
	ptr = strchr(path, '/');
	while (ptr != NULL) {
		status = asprintf(&dir_path, "%.*s", (int)(ptr - path), path);
		if (status == -1) {
			BarUiMsg(settings, MSG_ERR, "Error copying the directory path of "
					"the audio file (%d:%s).\n", errno, strerror(errno));
			exit_status = -1;
			goto error;
		}

		status = mkdir(dir_path, 0755);
		if ((status == -1) && (errno != EEXIST)) {
			BarUiMsg(settings, MSG_ERR, "Error creating a parent directory of "
					"the audio file (%s) (%d:%s).\n", errno, strerror(errno));
			exit_status = -1;
			goto error;
		}

		free(dir_path);
		dir_path = NULL;
		ptr = strchr(ptr + 1, '/');
	}

	/*
	 * Open the audio file for writing.
	 */
	tmp_file = _BarFlyFileOpenStream(path, "wb");
	if ((tmp_file == NULL) && (errno == EEXIST)) {
		BarUiMsg(settings, MSG_DEBUG, "The audio file already exists. It will "
				"not be recorded (%s).\n", path);
		exit_status = -2;
		goto error;
	} else if (tmp_file == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error opening the audio file for reading "
				"(%s) (%d:%s).\n", path, errno, strerror(errno));
		exit_status = -1;
		goto error;
	}

	*file = tmp_file;
	tmp_file = NULL;

	goto end;

error:
end:
	if (dir_path != NULL) {
		free(dir_path);
	}

	if (tmp_file != NULL) {
		fclose(tmp_file);
	}

	return exit_status;
}

static FILE* _BarFlyFileOpenStream(char const* path, char const* mode)
{
	FILE* fp = NULL;
	int fd = -1;

	/*
	 * Open the file descriptor ignoring the EINTR error.  The file descriptor
	 * must first be opened because creation of a new file can not be guarnteed
	 * with fopen().
	 */
	do {
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		if ((fd == -1) && (errno != EINTR)) {
			goto error;
		}
	} while (fd == -1);

	/*
	 * Open the file stream ignoring the EINTR error.
	 */
	do {
		fp = fdopen(fd, "wb");
		if ((fp == NULL) && (errno != EINTR)) {
			goto error;
		}
	} while (fp == NULL);

	goto end;

error:
	if (fd != -1) {
		close(fd);
	}

end:
	return fp;
}

static size_t _BarFlyNameTranslate(char* dest, char const* src, size_t n,
		BarSettings_t const* settings)
{
	size_t i;
	size_t i2;

	assert(dest != NULL);
	assert(src != NULL);
	assert(settings != NULL);

	/*
	 * Loop through the string changing all the unusable characters.
	 */
	i2 = 0;
	for (i = 0; src[i] != '\0'; i++) {
		if (src[i] == '/' ||
		    src[i] == '\\' ||
		    src[i] == '|' ||
		    src[i] == ':' ||
		    src[i] == ';' ||
		    src[i] == '*' ||
		    src[i] == '`') {
			dest[i2] = '-';
			i2++;
		} else if (src[i] == '<') {
			dest[i2] = '(';
			i2++;
		} else if (src[i] == '>') {
			dest[i2] = ')';
			i2++;
		} else if (src[i] == ' ' && !settings->useSpaces) {
			dest[i2] = '_';
			i2++;
		} else if (src[i] == '"' ||
		           src[i] == '?') {
			/*
			 * Skip these characters.
			 */
		} else {
			dest[i2] = src[i];
			i2++;
		}
	}
	dest[i2] = '\0';

	return i2;
}

static char* _BarFlyParseCoverArtURL(char const* album_html,
		BarSettings_t const* settings)
{
	size_t const MATCH_COUNT = 2;
	size_t const ERROR_MSG_SIZE = 100;

	char* url = NULL;
	int status;
	regex_t regex_cover;
	regmatch_t cover_match[MATCH_COUNT];
	char error_msg[ERROR_MSG_SIZE];

	assert(album_html != NULL);

	/*
	 * Search the album detail page to find the cover art URL.
	 */
	memset(&regex_cover, 0, sizeof(regex_cover));
	status = regcomp(&regex_cover, "id *= *\"album_art\"[^\"]*\"([^\"]+)",
			REG_EXTENDED);
	if (status != 0) {
		regerror(status, &regex_cover, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_ERR, "Failed to compile the cover at regex "
				"(%d:%s).\n", status, error_msg);
		goto error;
	}

	memset(cover_match, 0, sizeof(cover_match));
	status = regexec(&regex_cover, album_html, MATCH_COUNT, cover_match, 0);
	if (status != 0) {
		regerror(status, &regex_cover, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_DEBUG, "The cover art was not included in the "
				"album detail page (%d:%s).\n", status, error_msg);
		goto error;
	}

	/*
	 * Extract the cover art URL.
	 */
	url = strndup(album_html + cover_match[1].rm_so,
			cover_match[1].rm_eo - cover_match[1].rm_so);
	if (url == NULL) {
		BarUiMsg(settings, MSG_ERR, "Failed to copy the cover art url "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	/*
	 * Make sure this isn't the no_album_art.jpg image.  This check must be
	 * done to only the URL itself and not the whole HTML page since the similar
	 * albums list could also use this image.
	 */
	if (strstr(url, "no_album_art.jpg") != NULL) {
		BarUiMsg(settings, MSG_DEBUG, "This album does not have cover art.\n");
		goto error;
	}

	goto end;

error:
	if (url != NULL) {
		free(url);
		url = NULL;
	}

end:
	regfree(&regex_cover);

	return url;
}

static int _BarFlyParseTrackDisc(char const* title, char const* album_xml,
		short unsigned* track_num, short unsigned* disc_num,
		BarSettings_t const* settings)
{
	size_t const MATCH_COUNT = 3;
	size_t const ERROR_MSG_SIZE = 100;

	int exit_status = 0;
	int status;
	char* regex_string = NULL;
	regex_t regex_track;
	regmatch_t match[MATCH_COUNT];
	char error_msg[ERROR_MSG_SIZE];
	char regex_title[BAR_FLY_NAME_LENGTH];
	int index;
	int index2;
	short unsigned track;
	short unsigned disc;

	assert(title != NULL);
	assert(album_xml != NULL);
	assert(track_num != NULL);
	assert(disc_num != NULL);
	assert(settings != NULL);

	memset(&regex_track, 0, sizeof(regex_track));

	/*
	 * Create the regular expression string.
	 * 
	 * The title needs to have all potential regular expresion metacharacters
	 * fixed.  The real correct way to do this would be to escape all of them
	 * with '\'.  But the easy way to it is simply replace them with '.'.
	 *
	 * Additionally Pandora excludes some characters from the titles in the
	 * xml page.  These characters are ignored.
	 */
	index2 = 0;
	for (index = 0; title[index] != '\0'; index++) {
		if (title[index] == '^' ||
			title[index] == '$' ||
			title[index] == '(' ||
			title[index] == ')' ||
			title[index] == '>' ||
			title[index] == '<' ||
			title[index] == '[' ||
			title[index] == '{' ||
			title[index] == '\\' ||
			title[index] == '|' ||
			title[index] == '.' ||
			title[index] == '*' ||
			title[index] == '+' ||
			title[index] == '&') {
			regex_title[index2] = '.';
			index2++;
		} else if (title[index] == '?' ) {
			/*
			 * Skip these characters.
			 */
		} else {
			regex_title[index2] = title[index];
			index2++;
		}
	}
	regex_title[index2] = '\0';

	status = asprintf(&regex_string,
			"songTitle *= *\"%s\"[^>]+"
			"discNum *= *\"([0-9]+)\"[^>]+"
			"trackNum *= *\"([0-9]+)\"", regex_title);
	if (status == -1) {
		BarUiMsg(settings, MSG_ERR, "Failed to create the regex string to get "
				"the track and disc numbers (%d:%s).\n", errno,
				strerror(errno));
		goto error;
	}

	/*
	 * Compile and execute the regular expression to get the track and disc
	 * numbers.
	 */
	status = regcomp(&regex_track, regex_string, REG_EXTENDED);
	if (status != 0) {
		regerror(status, &regex_track, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_ERR, "Failed to compile the regex to get the "
				"track and disc numbers (%d:%s).\n", status, error_msg);
		goto error;
	}

	memset(match, 0, sizeof(match));
	status = regexec(&regex_track, album_xml, MATCH_COUNT, match, 0);
	if (status != 0) {
		regerror(status, &regex_track, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_DEBUG, "The track and disc numbers were not "
				"included in the album explorer page (%d:%s).\n", status,
				error_msg);
		goto error;
	}

	/*
	 * Copy the track number.
	 */
	status = sscanf(album_xml + match[2].rm_so, "%hu", &track);
	if (status != 1) {
		BarUiMsg(settings, MSG_ERR, "Failed to copy the track number "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	/*
	 * Copy the disc number.
	 */
	status = sscanf(album_xml + match[1].rm_so, "%hu", &disc);
	if (status != 1) {
		BarUiMsg(settings, MSG_ERR, "Failed to copy the disc number (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	*track_num = track;
	*disc_num = disc;

	goto end;

error:
	exit_status = -1;

end:
	regfree(&regex_track);

	if (regex_string != NULL) {
		free(regex_string);
	}

	return exit_status;
}

static int _BarFlyParseYear(char const* album_html, short unsigned* year,
		BarSettings_t const* settings)
{
	size_t const MATCH_COUNT = 2;
	size_t const ERROR_MSG_SIZE = 100;

	int exit_status = 0;
	int status;
	regex_t regex_year;
	regmatch_t match[MATCH_COUNT];
	char error_msg[ERROR_MSG_SIZE];

	assert(album_html != NULL);
	assert(year != NULL);

	/*
	 * Compile and execute the regular expression to get the year.
	 */
	memset(&regex_year, 0, sizeof(regex_year));
	status = regcomp(&regex_year, "class *= *\"release_year\"[^0-9]*([0-9]{4})",
			REG_EXTENDED);
	if (status != 0) {
		regerror(status, &regex_year, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_ERR, "Failed to compile the regex to get the "
				"year (%d:%s).\n", status, error_msg);
		goto error;
	}

	memset(match, 0, sizeof(match));
	status = regexec(&regex_year, album_html, MATCH_COUNT, match, 0);
	if (status != 0) {
		regerror(status, &regex_year, error_msg, ERROR_MSG_SIZE);
		BarUiMsg(settings, MSG_DEBUG, "The year was not included in the album "
				"detail page (%d:%s).\n", status, error_msg);
		goto error;
	}

	/*
	 * Copy the year.
	 */
	status = sscanf(album_html + match[1].rm_so, "%4hu", year);
	if (status != 1) {
		BarUiMsg(settings, MSG_ERR, "Error convertig the year from a string "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	regfree(&regex_year);

	return exit_status;
}

static int _BarFlyTagFetchCover(uint8_t** cover_art, size_t* cover_size,
		char const* url, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	uint8_t* tmp_cover_art = NULL;
	size_t tmp_cover_size;

	assert(cover_art != NULL);
	assert(cover_size != NULL);
	assert(url != NULL);
	assert(settings != NULL);

	/*
	 * Fetch the cover art.
	 */
	status = _BarFlyFetchURL(url, &tmp_cover_art, &tmp_cover_size, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Could not get the cover art.\n");
		goto error;
	}

	*cover_art = tmp_cover_art;
	tmp_cover_art = NULL;
	*cover_size = tmp_cover_size;

	goto end;

error:
	exit_status = -1;

end:
	if (tmp_cover_art != NULL) {
		free(tmp_cover_art);
	}

	return exit_status;
}

#ifdef ENABLE_ID3TAG
static int _BarFlyTagID3Write(BarFly_t const* fly, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings)
{
	int const BUFFER_SIZE = 5;
	int const TAG_PADDED_SIZE = 1024;
	char const BAR_FLY_ID3_FRAME_DISC[] = "TPOS";

	int exit_status = 0;
	int status;
	struct id3_tag* tag;
	char buffer[BUFFER_SIZE];

	assert(fly != NULL);
	assert(fly->audio_file_path != NULL);
	assert(settings != NULL);

	/*
	 * Set the minimum size for the tag.  The tag will use CRC and compression.
	 * FIXME - figure out if the padded size is really needed.
	 */
	tag = id3_tag_new();
	if (tag == NULL) {
		BarUiMsg(settings, MSG_ERR, "Failed to create new tag.\n");
		goto error;
	}
	id3_tag_setlength(tag, TAG_PADDED_SIZE);
	id3_tag_options(tag,
			ID3_TAG_OPTION_UNSYNCHRONISATION |
			ID3_TAG_OPTION_APPENDEDTAG |
			ID3_TAG_OPTION_CRC |
			ID3_TAG_OPTION_COMPRESSION, 0);

	/*
	 * Add the data to the tag.
	 */
	status = BarFlyID3AddFrame(tag, ID3_FRAME_ARTIST, fly->artist, settings); 
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to write artist to tag.\n");
		goto error;
	}

	status = BarFlyID3AddFrame(tag, ID3_FRAME_ALBUM, fly->album, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to write album to tag.\n");
		goto error;
	}

	status = BarFlyID3AddFrame(tag, ID3_FRAME_TITLE, fly->title, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to write album to tag.\n");
		goto error;
	}

	if (fly->year != 0) {
		snprintf(buffer, BUFFER_SIZE, "%hu", fly->year);
		buffer[BUFFER_SIZE - 1] = '\0';
		status = BarFlyID3AddFrame(tag, ID3_FRAME_YEAR, buffer, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Failed to write year to tag.\n");
			goto error;
		}
	}

	if (fly->track != 0) {
		snprintf(buffer, BUFFER_SIZE, "%hu", fly->track);
		buffer[BUFFER_SIZE - 1] = '\0';
		status = BarFlyID3AddFrame(tag, ID3_FRAME_TRACK, buffer, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Failed to write track number to tag.\n");
			goto error;
		}
	}

	if (fly->disc != 0) {
		snprintf(buffer, BUFFER_SIZE, "%hu", fly->disc);
		buffer[BUFFER_SIZE - 1] = '\0';
		status = BarFlyID3AddFrame(tag, BAR_FLY_ID3_FRAME_DISC, buffer,
				settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Failed to write disc number to tag.\n");
			goto error;
		}
	}

	if (cover_art != NULL) {
		status = BarFlyID3AddCover(tag, cover_art, cover_size, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Failed to write cover to tag.\n");
			goto error;
		}
	}

	/*
	 * Write the tag to the file.
	 */
	status = BarFlyID3WriteFile(fly->audio_file_path, tag, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to write the tag.\n");
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	if (tag != NULL) {
		id3_tag_delete(tag);
	}

	return exit_status;
}
#endif

#ifdef ENABLE_FAAD
static int _BarFlyTagMp4Write(BarFly_t const* fly, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	BarFlyMp4Tag_t* tag = NULL;

	assert(fly != NULL);
	assert(fly->audio_file_path != NULL);
	assert(settings != NULL);

	/*
	 * Create the tag.
	 */
	tag = BarFlyMp4TagOpen(fly->audio_file_path, settings);
	if (tag == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error creating new tag.\n");
		goto error;
	}

	/*
	 * Add data to the tag.
	 */
	status = BarFlyMp4TagAddArtist(tag, fly->artist, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error adding the artist to the tag.\n");
		goto error;
	}

	status = BarFlyMp4TagAddAlbum(tag, fly->album, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error adding the album to the tag.\n");
		goto error;
	}

	status = BarFlyMp4TagAddTitle(tag, fly->title, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error adding the title to the tag.\n");
		goto error;
	}

	if (fly->year != 0) {
		status = BarFlyMp4TagAddYear(tag, fly->year, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Error adding the year to the tag.\n");
			goto error;
		}
	}

	if (fly->track != 0) {
		status = BarFlyMp4TagAddTrack(tag, fly->track, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Error adding the track to the tag.\n");
			goto error;
		}
	}

	if (fly->disc != 0) {
		status = BarFlyMp4TagAddDisk(tag, fly->disc, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Error adding the disc to the tag.\n");
			goto error;
		}
	}

	if (cover_art != NULL) {
		status = BarFlyMp4TagAddCoverArt(tag, cover_art, cover_size, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR, "Error adding the cover to the tag.\n");
			goto error;
		}
	}

	/*
	 * Write the tag to the file.
	 */
	status = BarFlyMp4TagWrite(tag, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error writing the tag to the file (%s).\n",
				fly->audio_file_path);
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	if (tag != NULL) {
		BarFlyMp4TagClose(tag);
	}

	return exit_status;
}
#endif

static int _BarFlyTagWrite(BarFly_t const* fly, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	uint8_t* cover_art = NULL;
	size_t cover_size = 0;

	assert(fly != NULL);
	assert(settings != NULL);

	/*
	 * Fetch the album cover.
	 */
	if ((settings->embedCover) && (fly->cover_art_url != NULL)) {
		status = _BarFlyTagFetchCover(&cover_art, &cover_size,
				fly->cover_art_url, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_DEBUG, "The cover art will not be addded to "
					"the tag.\n");
			exit_status = -1;
		}
	}

	switch (fly->audio_format) {
		#ifdef ENABLE_FAAD
		case PIANO_AF_AACPLUS:
			status = _BarFlyTagMp4Write(fly, cover_art, cover_size, settings);
			break;
		#endif

		#if defined ENABLE_MAD && defined ENABLE_ID3TAG
		case PIANO_AF_MP3:
		case PIANO_AF_MP3_HI:
			status = _BarFlyTagID3Write(fly, cover_art, cover_size, settings);
			break;
		#endif

		default:
			/*
			 * If the taging library was not enabled for the audio format being
			 * played just error out.  Don't report an error.
			 */
			BarUiMsg(settings, MSG_DEBUG, "The file was not tagged since the"
					"tagging library was not linked in.\n");
			goto error;
			break;
	}

	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	if (cover_art != NULL) {
		free(cover_art);
	}

	return exit_status;
}

void BarFlyFinalize(void)
{
	WaitressFree(&fly_waith);

	return;
}

int BarFlyClose(BarFly_t* fly, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;

	assert(settings != NULL);

	if (fly != NULL) {
		/*
		 * Close the file stream.
		 */
		if (fly->audio_file != NULL) {
			fclose(fly->audio_file);
		}

		/*
		 * Delete the file if it was not complete.
		 */
		if (!fly->completed) {
			fly->status = DELETING;
			status = _BarFlyFileDelete(fly, settings);
			if (status != 0) {
				exit_status = -1;
			}
		}

		/*
		 * Free the audio file name.
		 */
		if (fly->audio_file_path != NULL) {
			free(fly->audio_file_path);
		}

		/*
		 * Free the cover art URL.
		 */
		if (fly->cover_art_url != NULL) {
			free(fly->cover_art_url);
		}
	}

	return exit_status;
}

int BarFlyInit(BarSettings_t const* settings)
{
	char const* const PATH_SEPARATORS = "/";

	int exit_status = 0;
	int status;
	bool statusb;
	char* component;
	char* path = NULL;
	char* proxy = NULL;

	assert(settings != NULL);
	assert(settings->audioFileDir != NULL);

	/*
	 * Initialize the Waitress handle.
	 */
	WaitressInit(&fly_waith);

	if (settings->controlProxy != NULL) {
		proxy = settings->controlProxy;
	} else if (settings->proxy != NULL) {
		proxy = settings->proxy;
	}

	if (proxy != NULL) {
		statusb = WaitressSetProxy(&fly_waith, proxy);
		if (!statusb) {
			BarUiMsg(settings, MSG_ERR, "Could not set proxy (proxy = '%s').\n",
					proxy);
		}
	}

	/*
	 * Create the audio file directory and change into it.
	 */
	path = strdup(settings->audioFileDir);
	if (path == NULL) {
		BarUiMsg(settings, MSG_ERR, "Out of memory.\n");
		goto error;
	}

	if (path[0] == '/') {
		status = chdir("/");
		if (status != 0) {
			BarUiMsg(settings, MSG_ERR,
					"Could not create the audio file directory (%s).\n", path);
			goto error;
		}
	}

	component = strtok(path, PATH_SEPARATORS);
	while (component != NULL) {
		if ((strcmp(component, ".") == 0) || (strcmp(component, "") == 0)) {
			/*
			 * This is the current directory.  Do Nothing.
			 */
		} else if (strcmp(component, "..") == 0) {
			/*
			 * Just change back up one directory.
			 */
			status = chdir("..");
			if (status != 0) {
				BarUiMsg(settings, MSG_ERR, "Could not create the audio file "
						"directory (%s).\n", path);
				goto error;
			}
		} else {
			/*
			 * Create the component of the path and change into it.
			 */
			status = mkdir(component, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if ((status != 0) && (errno != EEXIST)) {
				BarUiMsg(settings, MSG_ERR, "Could not create the audio file "
						"directory (%s).\n", path);
				goto error;
			}

			status = chdir(component);
			if (status != 0) {
				BarUiMsg(settings, MSG_ERR, "Could not create the audio file "
						"directory (%s).\n", path);
				goto error;
			}
		}

		component = strtok(NULL, PATH_SEPARATORS);
	}

	goto end;

error:
	exit_status = -1;

end:
	if (path != NULL) {
		free(path);
	}

	return exit_status;
}

int BarFlyOpen(BarFly_t* fly, PianoSong_t const* song,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	BarFly_t output_fly;
	char* album_buf = NULL;

	assert(fly != NULL);
	assert(song != NULL);
	assert(settings != NULL);

	/*
	 * Initialize the BarFly_t members.
	 */
	memset(&output_fly, 0, sizeof(BarFly_t));
	output_fly.audio_file = NULL;
	output_fly.completed = false;
	output_fly.status = NOT_RECORDING;

	/*
	 * Copy the artist, album, title, and audio format.
	 */
	strncpy(output_fly.artist, song->artist, BAR_FLY_NAME_LENGTH);
	output_fly.artist[BAR_FLY_NAME_LENGTH - 1] = '\0';
	strncpy(output_fly.album, song->album, BAR_FLY_NAME_LENGTH);
	output_fly.album[BAR_FLY_NAME_LENGTH - 1] = '\0';
	strncpy(output_fly.title, song->title, BAR_FLY_NAME_LENGTH);
	output_fly.title[BAR_FLY_NAME_LENGTH - 1] = '\0';
	output_fly.audio_format = song->audioFormat;

	/*
	 * Get the album detail page and extract the year and cover art URL.
	 */
	status = _BarFlyFetchURL(song->albumDetailURL, (uint8_t**)&album_buf,
			NULL, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_DEBUG, "Couldn't get the album detail page.  "
				"The year and cover art will not be added to the tag.\n");
		exit_status = -1;
	}

	if (album_buf != NULL) {
		status = _BarFlyParseYear(album_buf, &output_fly.year, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_DEBUG, "The album release year will not be "
					"added to the tag.\n");
			exit_status = -1;
		}

		output_fly.cover_art_url = _BarFlyParseCoverArtURL(album_buf, settings);
		if (output_fly.cover_art_url == NULL) {
			BarUiMsg(settings, MSG_DEBUG, "The cover art will not be addded to the "
					"tag.\n");
			exit_status = -1;
		}

		free(album_buf);
		album_buf = NULL;
	}

	/*
	 * Get the album explorer page and extract the track and disc numbers.
	 */
	status = _BarFlyFetchURL(song->albumExplorerUrl, (uint8_t**)&album_buf,
			NULL, settings);
	if (status != 0) {
		BarUiMsg(settings, MSG_DEBUG, "Couldn't get the album explorer page.  "
				"The track and disc numbers will not be added to the tag.\n");
		exit_status = -1;
	}

	if (album_buf != NULL) {
		status = _BarFlyParseTrackDisc(song->title, album_buf,
				&output_fly.track, &output_fly.disc, settings);
		if (status != 0) {
			BarUiMsg(settings, MSG_DEBUG, "The track and disc numbers will not "
					"be added to the tag.\n");
			exit_status = -1;
		}
	}

	/*
	 * Get the path to the file.
	 */
	output_fly.audio_file_path = _BarFlyFileGetPath(song->artist, song->album,
			song->title, output_fly.year, output_fly.track, output_fly.disc,
			song->audioFormat, settings);
	if (output_fly.audio_file_path == NULL) {
		goto error;
	}
	
	/*
	 * Open a stream to the file.
	 */
	status = _BarFlyFileOpen(&output_fly.audio_file,
			output_fly.audio_file_path, settings);
	if (status == 0) {
		output_fly.status = RECORDING;
	} else if (status == -2) {
		output_fly.status = NOT_RECORDING_EXIST;
		output_fly.completed = true;
	} else {
		output_fly.completed = true;
		goto error;
	}

	/*
	 * All members of the BarFly_t structure were created successfully.  Copy
	 * them from the temporary structure to the one passed in.
	 */
	memcpy(fly, &output_fly, sizeof(BarFly_t));
	memset(&output_fly, 0, sizeof(BarFly_t));

	goto end;

error:
	exit_status = -1;

end:
	if (album_buf != NULL) {
		free(album_buf);
	}

	if (output_fly.audio_file != NULL) {
		fclose(output_fly.audio_file);
	}

	if (output_fly.audio_file_path != NULL) {
		free(output_fly.audio_file_path);
	}

	if (output_fly.cover_art_url != NULL) {
		free(output_fly.cover_art_url);
	}

	return exit_status;
}

char const* BarFlyStatusGet(BarFly_t* fly)
{
	char const* string;

	switch (fly->status) {
		case (RECORDING):
			string = "Recording";
			break;

		case (NOT_RECORDING):
			string = "Not Recording";
			break;

		case (NOT_RECORDING_EXIST):
			string = "Not Recording (file exists)";
			break;

		case (DELETING):
			string = "Deleting (partial file)";
			break;

		case (TAGGING):
			string = "Tagging";
			break;

		default:
			string = "Unknown";
			break;
	}

	return string;
}

int BarFlyTag(BarFly_t* fly, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;

	assert(fly != NULL);
	assert(settings != NULL);

	/*
	 * Tag the song if it has not been completed.  If an error occurs still
	 * mark the song as completed.
	 */
	if (!fly->completed) {
		assert(fly->audio_file != NULL);

		fly->status = TAGGING;
		status = _BarFlyTagWrite(fly, settings);
		if (status != 0) {
			exit_status = -1;
		}

		fly->completed = true;
	}

	return exit_status;
}

int BarFlyWrite(BarFly_t* fly, void const* data, size_t data_size)
{
	int exit_status = 0;
	size_t status;

	assert(fly != NULL);
	assert(data != NULL);

	/*
	 * Write the given data buffer to the audio file.
	 */
	if (!fly->completed) {
		assert(fly->audio_file != NULL);
		status = fwrite(data, data_size, 1, fly->audio_file);
		if (status != 1) {
			goto error;
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

// vim: set noexpandtab:

