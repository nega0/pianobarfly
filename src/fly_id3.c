/**
 * @file fly_id3.c
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

#if defined ENABLE_MAD && ENABLE_ID3TAG

#include <assert.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fly.h"
#include "fly_id3.h"
#include "ui.h"

int BarFlyID3AddCover(struct id3_tag* tag, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings)
{
	/*
	 * http://flac.sourceforge.net/api/group__flac__format.html#ga113
	 */
	int const PICTURE_TYPE_FRONT_COVER = 3;

	char const BAR_FLY_ID3_FRAME_PICTURE[] = "APIC";

	int exit_status = 0;
	int status;
	struct id3_frame* frame = NULL;
	union id3_field* field;
	int index;
	char* mime_type;

	assert(tag != NULL);
	assert(cover_art != NULL);
	assert(settings != NULL);

	/*
	 * Get a new picture frame.
	 */
	frame = id3_frame_new(BAR_FLY_ID3_FRAME_PICTURE);
	if (frame == NULL) {
		BarUiMsg(settings, MSG_ERR, "Failed to create new frame (type = %s).\n", 
				BAR_FLY_ID3_FRAME_PICTURE);
		goto error;
	}
	
	/*
	 * Go through all the frame fields setting the mime type, image type, and
	 * the image data.
	 */
	index = 0;
	field = id3_frame_field(frame, index);
	while (field != NULL) {
		switch (id3_field_type(field)) {
			/*
			 * Set the cover art mime type.
			 */
			case (ID3_FIELD_TYPE_LATIN1):
				if ((cover_art[0] == 0xFF) && (cover_art[1] == 0xD8)) {
					mime_type = "image/jpeg";
				} else if ((cover_art[0] == 0x89) &&
				           (cover_art[1] == 0x50) &&
				           (cover_art[2] == 0x4E) &&
				           (cover_art[3] == 0x47) &&
				           (cover_art[4] == 0x0D) &&
				           (cover_art[5] == 0x0A) &&
				           (cover_art[6] == 0x1A) &&
				           (cover_art[7] == 0x0A)) {
					mime_type = "image/png";
				} else {
					mime_type = NULL;
				}

				id3_field_setlatin1(field, (id3_latin1_t const*)mime_type);
				break;

			/*
			 * Designate this as the front cover.
			 */
			case (ID3_FIELD_TYPE_INT8):
				id3_field_setint(field, PICTURE_TYPE_FRONT_COVER);
				break;

			/*
			 * Set the image data.
			 */
			case (ID3_FIELD_TYPE_BINARYDATA):
				id3_field_setbinarydata(field, cover_art, cover_size);
				break;

			default:
				break;
		}

		index++;
		field = id3_frame_field(frame, index);
	}

	/*
	 * Attach the frame to the tag.
	 */
	status = id3_tag_attachframe(tag, frame);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to attach cover art frame.\n");
		goto error;
	}

	goto end;

error:
	if (frame != NULL) {
		id3_frame_delete(frame);
	}

	exit_status = -1;

end:
	return exit_status;
}

int BarFlyID3AddFrame(struct id3_tag* tag, char const* type,
		char const* value, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	struct id3_frame* frame = NULL;
	union id3_field* field;
	id3_ucs4_t* ucs4 = NULL;
	int index;

	assert(tag != NULL);
	assert(type != NULL);
	assert(value != NULL);
	assert(settings != NULL);

	/*
	 * Create the frame.
	 */
	frame = id3_frame_new(type);
	if (frame == NULL) {
		BarUiMsg(settings, MSG_ERR, "Failed to create new frame (type = %s).\n",
				type);
		goto error;
	}

	frame->flags &= ~ID3_FRAME_FLAG_FORMATFLAGS;

	/*
	 * Get the string list field of the frame.
	 */
	index = 0;
	do {
		field = id3_frame_field(frame, index);
		index++;
	} while (id3_field_type(field) != ID3_FIELD_TYPE_STRINGLIST);
	assert(id3_field_type(field) == ID3_FIELD_TYPE_STRINGLIST);

	/*
	 * Add the value as a string to the field.
	 */
	ucs4 = id3_latin1_ucs4duplicate((id3_latin1_t*)value);
	if (ucs4 == NULL) {
		BarUiMsg(settings, MSG_ERR, "Could not allocate memory.\n");
		goto error;
	}

	status = id3_field_addstring(field, ucs4);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to set field value (value = %s).\n",
				value);
		goto error;
	}

	/*
	 * Attach the frame to the tag.
	 */
	status = id3_tag_attachframe(tag, frame);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Failed to attach frame (type = %s).\n",
				type);
		goto error;
	}

	goto end;
	
error:
	if (frame != NULL) {
		id3_frame_delete(frame);
	}

	exit_status = -1;

end:
	if (ucs4 != NULL) {
		free(ucs4);
	}

	return exit_status;
}

int BarFlyID3WriteFile(char const* file_path, struct id3_tag const* tag,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status_int;
	id3_length_t size1;
	id3_length_t size2;
	id3_byte_t* tag_buffer = NULL;
	FILE* audio_file = NULL;
	FILE* tmp_file = NULL;
	uint8_t audio_buffer[BAR_FLY_COPY_BLOCK_SIZE];
	char tmp_file_path[L_tmpnam];
	char* junk;
	size_t read_count;
	size_t write_count;

	/*
	 * For starters libid3tag kinda sucks.  It will only write a tag to a file 
	 * if the new tag is the same size as the old tag.  Which in this case,
	 * since there is no tag, will never work.  So writing of the tag to the
	 * file has to be done manually.
	 */

	/*
	 * Render the tag to a buffer that can then be written to the audio file.
	 */
	size1 = id3_tag_render(tag, NULL);
	tag_buffer = malloc(size1);
	if (tag_buffer == NULL) {
		BarUiMsg(settings, MSG_ERR, "Failed to allocate memory (bytes = %d).\n",
				size1);
		goto error;
	}

	size2 = id3_tag_render(tag, tag_buffer);
	if (size1 != size2) {
		BarUiMsg(settings, MSG_ERR, "Invalid tag size (expected = %d, "
				"recevied = %d).\n", size1, size2);
		goto error;
	}

	/*
	 * Prepending data to a file is not trivial in C.  Here the approach taken
	 * is to create a temporary file, write the tag to the beginning of the
	 * file, copy the audio file block by block to the tmp file, then overwrite
	 * the audio file with the tmp file.  This was done in order to minimize the
	 * chance of ending up with a broken audio file in case the program stopped
	 * durring this process.
	 */

	/*
	 * Open the audio file.
	 */
	audio_file = fopen(file_path, "rb");
	if (audio_file == NULL) {
		BarUiMsg(settings, MSG_ERR, "Could not read the audio file (%s) "
				"(%d:%s).\n", file_path, errno, strerror(errno));
		goto error;
	}

	/*
	 * Open the tmp file.
	 *
	 * Assigning the return value of tmpnam() to a junk pointer to get the
	 * compiler to be quiet.
	 */
	junk = tmpnam(tmp_file_path);
	junk = junk;
	tmp_file = fopen(tmp_file_path, "w+b");
	if (tmp_file == NULL) {
		BarUiMsg(settings, MSG_ERR, "Could not open the temporary file (%s) "
				"(%d:%s).\n", tmp_file_path, errno, strerror(errno));
		goto error;
	}

	/*
	 * Write the tag to the tmp file.
	 */
	write_count = fwrite(tag_buffer, 1, size2, tmp_file);
	if (write_count != size2) {
		BarUiMsg(settings, MSG_ERR, "Could not write the tag to the file (%s) "
				"(%d:%s).\n", tmp_file_path, errno, strerror(errno));
		goto error;
	}

	/*
	 * Read the audio file block by block until the end is reached.  Each block
	 * is written to the tmp file.
	 */
	while (feof(audio_file) == 0) {
		read_count = fread(audio_buffer, 1, BAR_FLY_COPY_BLOCK_SIZE,
				audio_file);
		if ((read_count != BAR_FLY_COPY_BLOCK_SIZE) &&
			(feof(audio_file) == 0)) {
			BarUiMsg(settings, MSG_ERR, "Failed to read the audio file (%s) "
					"(%d:%s).\n", file_path, errno, strerror(errno));
			goto error;
		}

		write_count = fwrite(audio_buffer, 1, read_count, tmp_file);
		if (write_count != read_count) {
			BarUiMsg(settings, MSG_ERR, "Failed to write to the tmp file "
					"(%s).\n", tmp_file_path);
			goto error;
		}
	}

	/*
	 * The entire contents of the audio file was copied to the tmp file.  Close
	 * the two files.
	 */
	fclose(tmp_file);
	tmp_file = NULL;

	fclose(audio_file);
	audio_file = NULL;

	/*
	 * Overwrite the audio file with the tmp file.
	 */
	status_int = rename(tmp_file_path, file_path);
	if (status_int != 0) {
		BarUiMsg(settings, MSG_ERR, "Could not overwrite the audio file "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	goto end;

error:
	/*
	 * Delete the tmp file if it exists.
	 */
	unlink(tmp_file_path);

	exit_status = -1;

end:
	if (tag_buffer != NULL) {
		free(tag_buffer);
	}

	if (audio_file != NULL) {
		fclose(audio_file);
	}

	if (tmp_file != NULL) {
		fclose(tmp_file);
	}

	return exit_status;
}

#endif

// vim: set noexpandtab:
