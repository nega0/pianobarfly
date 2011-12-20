/**
 * @file fly_mp4.c
 *
 * This is not a full implementation of an MP4 metadata handler.  It provides
 * enough functionality for the needs of pianobarfly.  Two other tagging
 * libraries were looked at: mp4v2 and taglib.  libmp4v2 just plain didn't work.
 * While taglib does have C language bindings but not for embedding cover art.
 * 
 * When a tag is opened the moov atom is read in from the MP4 file and added to
 * the tag.  Then the udat, meta, hdlr, and ilst atoms are created and added to
 * the tag.  Later additional metadata atoms for things like the artist and
 * song title are added to the tag.  When the file is written back out
 * everything in the original file before the moov atom is copied to a tmp
 * file.  Then the moov atom is written to the tmp file from the tag.  Lastly
 * everything after the moov atom in the origianl file is copied to the tmp
 * file.  Finally the tmp file is moved overtop the original file.  
 *
 * The following assumtions were made:
 * - The file starts with the 'ftyp' atom.
 * - The second atom is 'moov' and already present in the MP4 file.
 * - The file does not contain a 'udta' atom or any of its children.
 * - The 'udta' atom and all its children are inserted as the last children of
 *   the 'moov' atom.
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

#ifdef ENABLE_FAAD

#define _BSD_SOURCE

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fly.h"
#include "fly_mp4.h"
#include "ui.h"

/**
 * The minimum length of a short integer in bytes.
 */
#define BAR_FLY_MP4_SHORT_LENGTH 2

/**
 * Length of the atom size in bytes.
 */
#define BAR_FLY_MP4_ATOM_SIZE_LENGTH 4

/**
 * Length of the atom name in bytes.
 */
#define BAR_FLY_MP4_ATOM_NAME_LENGTH 4

/**
 * The minimum number of bytes in an atom.
 */
#define BAR_FLY_MP4_ATOM_MIN_LENGTH \
		(BAR_FLY_MP4_ATOM_SIZE_LENGTH + BAR_FLY_MP4_ATOM_NAME_LENGTH)

/**
 * The meta atom's data.
 */
#define BAR_FLY_MP4_ATOM_META_DATA {0x00, 0x00, 0x00, 0x00}

/**
 * The hdlr atom's data.
 */
#define BAR_FLY_MP4_ATOM_HDLR_DATA \
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
		  'm',  'd',  'i',  'r',  'a',  'p',  'p',  'l', \
		 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
		 0x00}

/**
 * Length of an atom's class nul space in bytes.
 */
#define BAR_FLY_MP4_ATOM_CLASS_LENGTH 4

/**
 * The album atom's class.
 */
#define BAR_FLY_MP4_ATOM_ALBUM_CLASS {0x00, 0x00, 0x00, 0x01}

/**
 * The artist atom's class.
 */
#define BAR_FLY_MP4_ATOM_ARTIST_CLASS {0x00, 0x00, 0x00, 0x01}

/**
 * The cover atom's class.
 */
#define BAR_FLY_MP4_ATOM_COVER_CLASS {0x00, 0x00, 0x00, 0x15}

/**
 * The disk atom's class.
 */
#define BAR_FLY_MP4_ATOM_DISK_CLASS {0x00, 0x00, 0x00, 0x00}

/**
 * The title atom's class.
 */
#define BAR_FLY_MP4_ATOM_TITLE_CLASS {0x00, 0x00, 0x00, 0x01}

/**
 * The track atom's class.
 */
#define BAR_FLY_MP4_ATOM_TRACK_CLASS {0x00, 0x00, 0x00, 0x00}

/**
 * The year atom's class.
 */
#define BAR_FLY_MP4_ATOM_YEAR_CLASS {0x00, 0x00, 0x00, 0x01}

/**
 * The MP4 atom structure.  This structure represents a single atom in an MP4 
 * file.
 */
struct BarFlyMp4Atom;
typedef struct BarFlyMp4Atom BarFlyMp4Atom_t;

struct BarFlyMp4Atom {
	/**
	 * The name of the atom.
	 */
	char name[BAR_FLY_MP4_ATOM_NAME_LENGTH + 1];

	/**
	 * The total size of the atom.  This includes its own size plus the size of
	 * all its children.
	 */
	size_t size;

	/**
	 * The parent atom of this atom.  If this pointer is NULL the atom has no
	 * parent and is therefore the top level.
	 */
	BarFlyMp4Atom_t* parent;

	/**
	 * An array of child atoms.  If this pointer is NULL the atom has no
	 * children.
	 */
	BarFlyMp4Atom_t** children;

	/**
	 * The number of child atoms.
	 */
	int child_count;

	/**
	 * The atom's data buffer.
	 */
	uint8_t* data;

	/**
	 * The size of the data buffer in bytes.  If the data size != 0 but the data
	 * buffer is NULL it means the atom contains data but it has not yet been
	 * read from the file.  Before modifying an atom's data this condition must
	 * be checked for and if so the data must be read from the file before it is
	 * modified.
	 */
	size_t data_size;

	/**
	 * The offset of the start of the atom in bytes from the beginning of the
	 * origianl MP4 file.  -1 means the atom was not read from the file.
	 */
	long offset;
};

struct BarFlyMp4Tag {
	/**
	 * The path to the file with which this tag is associated.
	 */
	char* file_path;

	/**
	 * An array of top level atoms in the tag.
	 */
	BarFlyMp4Atom_t** atoms;

	/**
	 * The number of atoms in the array.
	 */
	int atom_count;

	/**
	 * The MP4 file.
	 */
	FILE* mp4_file;
};


/**
 * Appends an atom to the list of children atoms.  All parent atoms' sizes are
 * adjusted accordingly.  After a child is successfully added it must not be
 * closed outside of closing the parent.
 *
 * @param parent A pointer to the parent atom.
 * @param child A pointer to the child atom.
 * @param settings A pointer to the application's settings structure.
 * @return If the child was successfully added 0 is returned.  Otherwise -1 is
 * returned.
 */
static int _BarFlyMp4AtomAddChild(BarFlyMp4Atom_t* parent,
		BarFlyMp4Atom_t* child, BarSettings_t const* settings);

/**
 * Appends the given data to the end of the atom's data buffer.  The atom's size
 * and all parent atoms' sizes are adjusted accordingly.
 *
 * This function does not update the offsets.  Once an atom has been added to
 * the tag this function must not be called.
 *
 * @param atom A pointer to the atom.
 * @param mp4_file Pointer to the MP4 file.
 * @param data A pointer to the data buffer.  If data is NULL nothing happens.
 * @param size The size of the buffer.  If size is 0 nothing happens.
 * @param settings A pointer to the application's settings structure.
 * @return If the buffer is successfully appened to the end of the atom's data
 * 0 is returned otherwise -1 is returned.
 */
static int _BarFlyMp4AtomAppendData(BarFlyMp4Atom_t* atom, FILE* mp4_file,
		uint8_t const* data, size_t size, BarSettings_t const* settings);

/**
 * Adds the delta to the atom's size.  This same delta will also be added
 * recursively to each parent of this atom.
 *
 * @param atom A pointer to the atom.
 * @param delta The positive or negative size difference for which this atom
 * should change.
 */
static void _BarFlyMp4AtomBumpSize(BarFlyMp4Atom_t* atom, long delta);

/**
 * Frees all resources associated with an atom.  Any child atoms will be 
 * recursively destroyed down to the lowest level.
 *
 * @param atom The atom to be closed.
 */
static void _BarFlyMp4AtomClose(BarFlyMp4Atom_t* atom);

/**
 * Creates an atom structure with the given name.  The returned atom must be
 * freed with a call to _BarFlyMp4AtomClose().  The atom's size is set to 
 * BAR_FLY_MP4_ATOM_MIN_LENGTH.
 *
 * @param name The name of the atom.
 * @param offset The offset of the atom in the file.  If this atom is being
 * created and not read from the file pass in -1.
 * @param settings A pointer to the application's settings structure.
 * @return A pointer to the new atom.
 */
static BarFlyMp4Atom_t* _BarFlyMp4AtomOpen(char const* name, long offset,
		BarSettings_t const* settings);

/**
 * Renders an atom to a file.  The atom it's data and all it's children are
 * rendered to the file.
 *
 * @param atom A pointer to the atom to be rendered.
 * @param in_file A pointer to the original MP4 file stream.
 * @param out_file A pointer to the file stream that will be written to.
 * @param settings A pointer to the application's settings structure.
 * @return If the atom was successfully written to the file 0 is returned.
 * Otherwise -1 is returned.
 */
static int _BarFlyMp4AtomRender(BarFlyMp4Atom_t const* atom, FILE* in_file,
		FILE* out_file, BarSettings_t const* settings);

/**
 * Converts the first 4 bytes of the buffer to a long integer.
 *
 * @param buffer The buffer.
 * @return The long integer.
 */
static long _BarFlyMp4BufToLong(uint8_t const* buffer);

/**
 * Reads an MP4 file and creates an atom.  The file position indicator must be
 * positioned at the begining of the atom to be read.  The atom and all its
 * children are read from the file.
 *
 * The size, name, offset, and children atom's are read from the file.  The
 * atom's data is not read in so as to avoid copying the entire file into
 * memory.  The data will only be read in later if it is modified.
 *
 * Upon return the file position indicator will be pointing to the beginning
 * of the next atom.  If an error occurs the file position indicator will be
 * reset to where it was when the function was called.
 *
 * @param mp4_file A file stream pointer with the file position indicator at
 * the beginning of the atom to be read.
 * @param settings A pointer to the application's settings structure.
 * @return Upon success a pointer to an atom is returned.  Otherwise, NULL is
 * returned.
 */
static BarFlyMp4Atom_t* _BarFlyMp4FileParseAtom(FILE* mp4_file,
		BarSettings_t const* settings);

/**
 * Reads an atom's data from an MP4 file.  If the atom has no data nothing
 * will be done.  The atom's data will only be read from the file if it hasn't
 * been read already.
 * 
 * The file position indicator will be moved to the end of the atom's data
 * block.  If an error occurs the file position indicator will be reset to
 * where it was when the function was called.
 *
 * @param mp4_file The MP4 file.
 * @param atom A pointer to the atom.  The atom's offset and data_size must
 * have been set already.
 * @param settings A pointer to the application's settings structure.
 * @return If the data is read successfully or there is no data 0 is returned.
 * Otherwise, -1 is returned.
 */
static int _BarFlyMp4FileParseAtomData(FILE* mp4_file, BarFlyMp4Atom_t* atom,
		BarSettings_t const* settings);

/**
 * Reads an atom's name from a file.  The file's position indicator must be at
 * the start of the atom's name.
 *
 * @param mp4_file A ponter to the MP4 file stream. 
 * @param name A pre-allocated buffer ::BAR_FLY_MP4_ATOM_NAME_LENGTH + 1 bytes
 * long.
 * @param settings A pointer to the application's settings structure.
 * @return If the name was successfully read 0 is returend.  Otherwise -1 is
 * returned.
 */
static int _BarFlyMp4FileParseAtomName(FILE* mp4_file, char* name,
		BarSettings_t const* settings);

/**
 * Reads an atom's size from a file.  The file's position indicator must be at
 * the start of the atom's size.  The size read in must be at least the minimum
 * size allowed for an atom.
 *
 * @param mp4_file A pointer to the MP4 file stream.
 * @param size A pointer to a size_t variable that will be populated with the
 * atom's size.
 * @param settings A pointer to the application's settings structure.
 * @return If the size was successfully parsed and is at least the minimum size
 * allowed 0 is returned.  Otherwise -1 is returned.
 */
static int _BarFlyMp4FileParseAtomSize(FILE* mp4_file, size_t* size,
		BarSettings_t const* settings);

/**
 * Reads in data from a file and validates that the amount of data requested
 * was the amount actually received.
 *
 * @param file A pointer to to a file stream.
 * @param buffer The pre-allocated buffer into which the data will be written.
 * The buffer must be at least size bytes long.
 * @param size The number of bytes to read from the file.
 * @param settings A pointer to the application's settings structure.
 * @return If the data was read successfully 0 is returned.  Otherwise -1 is
 * returned.
 */
static int _BarFlyMp4FileParseData(FILE* file, uint8_t* buffer, size_t size,
		BarSettings_t const* settings);

/**
 * Reads in a long integer from the file.  In this case a long integer is
 * considered to be 4 bytes read from the file.  The file position indicator
 * must be at the start of the 4 bytes to be read.  Upon return it will at the
 * end of the 4 bytes.
 *
 * @param file A pointer to the file stream.
 * @param number A pointer to the long integer to be populated.
 * @param settings A pointer to the application's settings structure.
 * @param If the integer was read successfully 0 is returned.  Otherwise -1 is
 * returned.
 */
static int _BarFlyMp4FileParseLong(FILE* file, long* number,
		BarSettings_t const* settings);

/**
 * Writes a long integer to a buffer ::BAR_FLY_MP4_ATOM_SIZE_LENGTH bytes long.
 *
 * @param size The buffer which must be ::BAR_FLY_MP4_ATOM_SIZE_LENGTH bytes.
 * @param lsize The size to be written to the buffer.
 */
static void _BarFlyMp4LongRender(uint8_t* size, long unsigned lsize);

/**
 * Writes a short to a buffer.
 *
 * @param buffer The buffer which must be at least ::BAR_FLY_MP4_SHORT_LENGTH
 * bytes long.
 * @param value The integer.
 */
static void _BarFlyMp4ShortRender(uint8_t* buffer, short unsigned value);

/**
 * Adds an atom to the tag as the child of the given parent's path.  The parent
 * atom's must already have been added to the tag.  Once an atom is added it is
 * owned by the tag; it's resources should not be freed outside of freeing the
 * tag.
 *
 * Atoms must be added to the tag using this function in order to have the
 * offsets updated correctly.  It is possible to add an atom by calling
 * _BarFlyMp4TagFindAtom() then adding a child atom but this won't update the
 * offsets and will corrupt the file.
 *
 * @param tag A pointer to the tag.
 * @param parent_path The path to the parent atom.  Atoms are separeated by '.'.
 * If the atom has no parent pass in an empty string.
 * @param atom A pointer to the atom to be added.
 * @param update_offsets If true the offsets are updated.  If false they are
 * not.  The offsets should not be updated if the atom being added was parsed
 * from the file and unmodified.
 * @param settings A pointer to the application's settings structure.
 * @return If the atom was succesfully added to the tag 0 is returned otherwise
 * -1 is returned.
 */
static int _BarFlyMp4TagAddAtom(BarFlyMp4Tag_t* tag, char const* parent_path,
		BarFlyMp4Atom_t* atom, bool update_offsets,
		BarSettings_t const* settings);

/**
 * Creates a new metadata atom and adds it to the tag.  The atom is added under
 * the moov.udta.meta.ilst atom, which if it doesn't exist is automatically
 * created.
 *
 * @param tag A pointer to the tag.
 * @param name The name of the atom to be added.  Must be name of an atom that
 * can be a child of "ilst".
 * @param class A buffer ::BAR_FLY_MP4_ATOM_CLASS_LENGTH bytes long containg
 * the atom's class.
 * @param data A buffer containing the meta data.
 * @param data_size The size of the data buffer in bytes.
 * @param settings A pointer to the application's settings structure.
 */
static int _BarFlyMp4TagAddMetaAtom(BarFlyMp4Tag_t* tag, char const* name,
		uint8_t const* class, uint8_t const* data, size_t data_size,
		BarSettings_t const* settings);

/**
 * Returns the atom at the given path.
 *
 * @param tag A pointer to the tag.
 * @param path A string containing a '.' delininated hierachy of atoms.
 * @return A pointer to the found atom or NULL if no atom was found.
 */
static BarFlyMp4Atom_t* _BarFlyMp4TagFindAtom(BarFlyMp4Tag_t const* tag,
		char const* path);

/**
 * Updateds all the atom's in the tag that have offsets to the rest of the file.
 *
 * @param tag A pointer to the tag.
 * @param delta The number of bytes by which the offsets will be changed.
 * @param settings A pointer to the application's settings structure.
 * @return If the offsets were updated successfully 0 is returned.  Otherwise
 * -1 is returned.
 */
static int _BarFlyMp4TagUpdateOffsets(BarFlyMp4Tag_t* tag, long delta,
		BarSettings_t const* settings);


static int _BarFlyMp4AtomAddChild(BarFlyMp4Atom_t* parent,
		BarFlyMp4Atom_t* child, BarSettings_t const* settings)
{
	int exit_status = 0;
	BarFlyMp4Atom_t** temp = NULL;

	assert(parent != NULL);
	assert(child != NULL);
	assert(settings != NULL);

	/*
	 * Allocate space for another child.
	 */
	temp = realloc(parent->children,
			sizeof(BarFlyMp4Atom_t*) * (parent->child_count + 1));
	if (temp == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error allocating memory (%d bytes).\n",
				sizeof(BarFlyMp4Atom_t*) * (parent->child_count + 1));
		goto error;
	}
	parent->children = temp;
	temp = NULL;

	/*
	 * Put the child in the array and add the size of the child to the
	 * parent.
	 */
	parent->children[parent->child_count] = child;
	parent->child_count++;
	_BarFlyMp4AtomBumpSize(parent, child->size);

	/*
	 * Set the parent in the child atom.
	 */
	child->parent = parent;

	goto end;

error:
	exit_status = -1;

end:
	if (temp != NULL) {
		free(temp);
	}

	return exit_status;
}

static int _BarFlyMp4AtomAppendData(BarFlyMp4Atom_t* atom, FILE* mp4_file,
		uint8_t const* data, size_t size, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	uint8_t* temp = NULL;

	assert(atom != NULL);
	assert(mp4_file != NULL);
	assert(settings != NULL);

	if ((data != NULL) && (size > 0)) {
		/*
		 * Make sure to the data has been read from the file before it is
		 * modified.
		 */
		status = _BarFlyMp4FileParseAtomData(mp4_file, atom, settings);
		if (status != 0) {
			goto error;
		}

		/*
		 * Increase the size of the atom's buffer.
		 */
		temp = realloc(atom->data, sizeof(uint8_t) * (atom->data_size + size));
		if (temp == NULL) {
			BarUiMsg(settings, MSG_ERR, "Error allocating memory (%d bytes).\n",
					sizeof(uint8_t) * (atom->data_size + size));
			goto error;
		}
		atom->data = temp;
		temp = NULL;

		/*
		 * Append the new data onto the end of the atom's buffer.
		 */
		memcpy(atom->data + atom->data_size, data, size);
		atom->data_size += size;
		atom->size += size;

		/*
		 * If this atom has a parent change its size to reflect the size change
		 * of it's child.
		 */
		if (atom->parent != NULL) {
			_BarFlyMp4AtomBumpSize(atom->parent, size);
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	if (temp != NULL) {
		free(temp);
	}

	return exit_status;
}

static void _BarFlyMp4AtomBumpSize(BarFlyMp4Atom_t* atom, long delta)
{
	assert(atom != NULL);

	/*
	 * Add the differnce to this atom then to it's parent, if it has one.
	 */
	atom->size += delta;

	if (atom->parent != NULL) {
		_BarFlyMp4AtomBumpSize(atom->parent, delta);
	}

	return;
}

static void _BarFlyMp4AtomClose(BarFlyMp4Atom_t* atom)
{
	int index;

	/*
	 * Destroy the atom.  The parent is not owned by the child so it is not
	 * closed.
	 */
	if (atom != NULL) {
		/*
		 * Close all children of this atom.
		 */
		if (atom->children != NULL) {
			for (index = 0; index < atom->child_count; index++) {
				_BarFlyMp4AtomClose(atom->children[index]);
			}
			free(atom->children);
		}

		/*
		 * Free any data owned by this atom.
		 */
		if (atom->data != NULL) {
			free(atom->data);
		}
	}

	return;
}

static BarFlyMp4Atom_t* _BarFlyMp4AtomOpen(char const* name, long offset,
		BarSettings_t const* settings)
{
	BarFlyMp4Atom_t* atom = NULL;

	assert(name != NULL);
	assert(strlen(name) <= BAR_FLY_MP4_ATOM_NAME_LENGTH);
	assert(settings != NULL);

	/*
	 * Allocate space for the atom.
	 */
	atom = calloc(1, sizeof(BarFlyMp4Atom_t));
	if (atom == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error allocating memory (%d bytes).\n",
				sizeof(BarFlyMp4Atom_t));
		goto error;
	}

	/*
	 * Copy the atom name and set the size to 8, which is the minimum size for
	 * an MP4 atom.  size (4 bytes) + name (4 bytes).
	 */
	strncpy(atom->name, name, BAR_FLY_MP4_ATOM_NAME_LENGTH + 1);
	atom->name[BAR_FLY_MP4_ATOM_NAME_LENGTH] = '\0';
	atom->size = BAR_FLY_MP4_ATOM_MIN_LENGTH;
	atom->offset = offset;

	goto end;

error:
	_BarFlyMp4AtomClose(atom);
	atom = NULL;

end:
	return atom;
}

static int _BarFlyMp4AtomRender(BarFlyMp4Atom_t const* atom, FILE* in_file,
		FILE* out_file, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	size_t write_count;
	int index;
	size_t read_count;
	size_t data_size;
	uint8_t buffer[BAR_FLY_COPY_BLOCK_SIZE];
	size_t buf_size;

	assert(atom != NULL);
	assert(in_file != NULL);
	assert(out_file != NULL);
	assert(settings != NULL);

	/*
	 * Write the atom's size to the file.
	 */
	_BarFlyMp4LongRender(buffer, atom->size);
	write_count = fwrite(buffer, 1, BAR_FLY_MP4_ATOM_SIZE_LENGTH, out_file);
	if (write_count != BAR_FLY_MP4_ATOM_SIZE_LENGTH) {
		BarUiMsg(settings, MSG_ERR, "Error writing the atom's size to the file "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	/*
	 * Write the atom's name to the file.
	 */
	write_count = fwrite(atom->name, 1, BAR_FLY_MP4_ATOM_NAME_LENGTH, out_file);
	if (write_count != BAR_FLY_MP4_ATOM_NAME_LENGTH) {
		BarUiMsg(settings, MSG_ERR, "Error writing the atom's name to the file "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	/*
	 * Write the atom's data to the file.  If the data hasn't been read from the
	 * original MP4 file do that now.
	 */
	if (atom->data_size > 0) {
		if (atom->data == NULL) {
			status = fseek(in_file, atom->offset +
					BAR_FLY_MP4_ATOM_SIZE_LENGTH + BAR_FLY_MP4_ATOM_NAME_LENGTH,
					SEEK_SET);
			if (status != 0) {
				BarUiMsg(settings, MSG_ERR, "Error seeking in the file "
						"(%d:%s).\n", errno, strerror(errno));
				goto error;
			}

			data_size = atom->data_size;
			while (data_size > 0) {
				buf_size = (data_size < BAR_FLY_COPY_BLOCK_SIZE) ? (data_size) :
							(BAR_FLY_COPY_BLOCK_SIZE);
				read_count = fread(buffer, 1, buf_size, in_file);
				if (read_count != buf_size) {
					BarUiMsg(settings, MSG_ERR, "Error reading from a file "
							"(%d:%s).\n", errno, strerror(errno));
					goto error;
				}

				write_count = fwrite(buffer, 1, buf_size, out_file);
				if (write_count != read_count) {
					BarUiMsg(settings, MSG_ERR, "Error writing to a file "
							"(%d:%s).\n", errno, strerror(errno));
					goto error;
				}

				data_size -= buf_size;
			}
		} else {
			write_count = fwrite(atom->data, 1, atom->data_size, out_file);
			if (write_count != atom->data_size) {
				BarUiMsg(settings, MSG_ERR, "Error writing to a file "
						"(%d:%s).\n", errno, strerror(errno));
				goto error;
			}
		}
	}

	/*
	 * Reader each of the atom's children to the file.
	 */
	if (atom->child_count > 0) {
		for (index = 0; index < atom->child_count; index++) {
			status = _BarFlyMp4AtomRender(atom->children[index], in_file,
					out_file, settings);
			if (status != 0) {
				goto error;
			}
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

static long _BarFlyMp4BufToLong(uint8_t const* buffer) {
	long number;
	int index;

	assert(buffer != NULL);

	/*
	 * Convert the first 4 bytes of the buffer to a long integer.
	 */
	number = 0;
	for (index = 0; index <= BAR_FLY_MP4_ATOM_SIZE_LENGTH - 1; index++) {
		number |= buffer[index] << 8 *
				(BAR_FLY_MP4_ATOM_SIZE_LENGTH - 1 - index);
	}

	return number;
}


static BarFlyMp4Atom_t* _BarFlyMp4FileParseAtom(FILE* mp4_file,
		BarSettings_t const* settings)
{
	BarFlyMp4Atom_t* atom = NULL;
	int status;
	long start_pos;
	char name[BAR_FLY_MP4_ATOM_NAME_LENGTH + 1];
	size_t size_l;
	uint8_t* buffer = NULL;
	BarFlyMp4Atom_t* child = NULL;
	size_t remaining;

	assert(mp4_file != NULL);
	assert(settings != NULL);

	/*
	 * Get the atom's offset.
	 */
	start_pos = ftell(mp4_file);

	/*
	 * Read the size.
	 */
	status = _BarFlyMp4FileParseAtomSize(mp4_file, &size_l, settings);
	if (status != 0) {
		goto error;
	}

	/*
	 * Read the name.
	 */
	status = _BarFlyMp4FileParseAtomName(mp4_file, name, settings);
	if (status != 0) {
		goto error;
	}

	/*
	 * Create the atom structure.
	 */
	atom = _BarFlyMp4AtomOpen(name, start_pos, settings);
	if (atom == NULL) {
		goto error;
	}
	
	/*
	 * Set the data size and the atom size which at this point is the size,
	 * name, and data.  The children sizes are added later when they are added
	 * to their parent atom.
	 *
	 * The data is not actually copied into the data buffer here.  It is copied
	 * into the buffer later on only if it needs to be modified.  This is to
	 * avoid copying large chunks of the files data into memory if it isn't 
	 * needed.
	 */

	/*
	 * Atom's with children only.
	 */
	if (strcmp(name, "dinf") == 0 ||
	    strcmp(name, "mdia") == 0 ||
	    strcmp(name, "minf") == 0 ||
	    strcmp(name, "moov") == 0 ||
	    strcmp(name, "stbl") == 0 ||
	    strcmp(name, "trak") == 0) {
		atom->data_size = 0;
		atom->size = BAR_FLY_MP4_ATOM_SIZE_LENGTH +
				BAR_FLY_MP4_ATOM_NAME_LENGTH;

	/*
	 * Atom's with data only.
	 */
	} else if (strcmp(name, "dref") == 0 ||
	           strcmp(name, "esds") == 0 ||
	           strcmp(name, "hdlr") == 0 ||
	           strcmp(name, "iods") == 0 ||
	           strcmp(name, "mdhd") == 0 ||
	           strcmp(name, "mvhd") == 0 ||
	           strcmp(name, "smhd") == 0 ||
	           strcmp(name, "stco") == 0 ||
	           strcmp(name, "stsc") == 0 ||
	           strcmp(name, "stsz") == 0 ||
	           strcmp(name, "stts") == 0 ||
	           strcmp(name, "tkhd") == 0 ) {
		atom->data_size = size_l - BAR_FLY_MP4_ATOM_SIZE_LENGTH -
				BAR_FLY_MP4_ATOM_NAME_LENGTH;
		atom->size = size_l;

	/*
	 * The rest of the atom's have both data and children.
	 */
	} else if (strcmp(name, "stsd") == 0) {
		atom->data_size = 8;
		atom->size =
				BAR_FLY_MP4_ATOM_SIZE_LENGTH + BAR_FLY_MP4_ATOM_NAME_LENGTH + 8;
	} else if (strcmp(name, "mp4a") == 0) {
		atom->data_size = 28;
		atom->size = BAR_FLY_MP4_ATOM_SIZE_LENGTH +
				BAR_FLY_MP4_ATOM_NAME_LENGTH + 28;
	} else {
		BarUiMsg(settings, MSG_ERR, "Unknown atom (name = %s).\n", name);
		goto error;
	}

	/*
	 * Seek to the end of the atom's data.
	 */
	status = fseek(mp4_file, atom->data_size, SEEK_CUR);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error seeking in the audio file "
				"(%d:%s).\n", errno, strerror(errno));
		goto error;
	}

	/*
	 * Parse the atom's children if there are any.
	 */
	remaining = size_l - atom->size;
	while (remaining > 0) {
		child = _BarFlyMp4FileParseAtom(mp4_file, settings);
		if (child == NULL) {
			goto error;
		}

		if (child->size > remaining) {
			BarUiMsg(settings, MSG_ERR, "Invalid MP4 file.  The children atom "
					"sizes are larger than the parent.\n");
			goto error;
		}
		remaining -= child->size;

		status = _BarFlyMp4AtomAddChild(atom, child, settings);
		if (status != 0) {
			goto error;
		}
		child = NULL;
	}

	goto end;

error:
	_BarFlyMp4AtomClose(atom);
	atom = NULL;

	fseek(mp4_file, start_pos, SEEK_SET);

end:
	if (buffer != NULL) {
		free(buffer);
	}

	_BarFlyMp4AtomClose(child);

	return atom;
}

static int _BarFlyMp4FileParseAtomData(FILE* mp4_file, BarFlyMp4Atom_t* atom,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	long start_pos = -1;
	uint8_t* tmp_buffer = NULL;

	assert(mp4_file != NULL);
	assert(atom != NULL);

	/*
	 * Only read the data from the file if it hasn't already been read.
	 */
	if ((atom->data == NULL) && (atom->data_size > 0)) {
		assert(atom->offset != -1);
		/*
		 * Only read data for those atom's that may have some.  Currently only
		 * atom's with just data are handled.  So atom's like meta that have
		 * both data and children are not handled.
		 */
		if (strcmp(atom->name, "dref") == 0 ||
		    strcmp(atom->name, "esds") == 0 ||
		    strcmp(atom->name, "ftyp") == 0 ||
		    strcmp(atom->name, "hdlr") == 0 ||
		    strcmp(atom->name, "iods") == 0 ||
		    strcmp(atom->name, "mdhd") == 0 ||
		    strcmp(atom->name, "mvhd") == 0 ||
		    strcmp(atom->name, "smhd") == 0 ||
		    strcmp(atom->name, "stco") == 0 ||
		    strcmp(atom->name, "stsc") == 0 ||
		    strcmp(atom->name, "stsz") == 0 ||
		    strcmp(atom->name, "stts") == 0 ||
		    strcmp(atom->name, "tkhd") == 0) {
			/*
			 * Move to the beginning of the atom's data block.
			 */
			start_pos = ftell(mp4_file);
			status = fseek(mp4_file,
					atom->offset + BAR_FLY_MP4_ATOM_SIZE_LENGTH +
					BAR_FLY_MP4_ATOM_NAME_LENGTH, SEEK_SET);
			if (status != 0) {
				BarUiMsg(settings, MSG_ERR,
						"Error seeking in a file (%d:%s).\n", errno,
						strerror(errno));
				goto error;
			}

			/*
			 * Only read data if the atom has some.
			 */
			if (atom->data_size > 0) {
				/*
				* Create the buffer and populate it.
				*/
				tmp_buffer = malloc(sizeof(uint8_t) * atom->data_size);
				if (tmp_buffer == NULL) {
					BarUiMsg(settings, MSG_ERR,
							"Error allocating memory (%d bytes).\n",
							sizeof(uint8_t) * atom->data_size);
					goto error;
				}

				status = _BarFlyMp4FileParseData(mp4_file, tmp_buffer,
						atom->data_size, settings);
				if (status != 0) {
					goto error;
				}
				atom->data = tmp_buffer;
				tmp_buffer = NULL;
			}
		}
	}

	goto end;

error:
	exit_status = -1;

	if (start_pos != -1) {
		fseek(mp4_file, start_pos, SEEK_SET);
	}

end:
	if (tmp_buffer != NULL) {
		free(tmp_buffer);
	}

	return exit_status;
}

static int _BarFlyMp4FileParseAtomName(FILE* mp4_file, char* name,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;

	assert(mp4_file != NULL);
	assert(name != NULL);

	/*
	 * Read in the name.
	 */
	status = _BarFlyMp4FileParseData(mp4_file, (uint8_t*)name,
			BAR_FLY_MP4_ATOM_NAME_LENGTH, settings);
	if (status != 0) {
		goto error;
	}
	name[BAR_FLY_MP4_ATOM_NAME_LENGTH] = '\0';

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

static int _BarFlyMp4FileParseAtomSize(FILE* mp4_file, size_t* size,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;

	assert(mp4_file != NULL);
	assert(size != NULL);
	assert(settings != NULL);

	/*
	 * Get the size.
	 */
	status = _BarFlyMp4FileParseLong(mp4_file, (long*)size, settings);
	if (status != 0) {
		goto error;
	}

	/*
	 * Make sure it is at least the minimum size.
	 */
	if (*size < BAR_FLY_MP4_ATOM_MIN_LENGTH) {
		BarUiMsg(settings, MSG_ERR,
				"Invalid atom size (minimum = %d, size = %d).\n",
				BAR_FLY_MP4_ATOM_MIN_LENGTH, *size);
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

static int _BarFlyMp4FileParseData(FILE* file, uint8_t* buffer, size_t size,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	size_t count;

	assert(file != NULL);
	assert(buffer != NULL);
	assert(settings != NULL);

	count = fread(buffer, 1, size, file);
	if (count != size) {
		BarUiMsg(settings, MSG_ERR, "Error reading from a file (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

static int _BarFlyMp4FileParseLong(FILE* file, long* number,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	uint8_t buf[BAR_FLY_MP4_ATOM_SIZE_LENGTH];

	assert(file != NULL);
	assert(number != NULL);
	assert(settings != NULL);

	/*
	 * Read the long int in and convert.
	 */
	status = _BarFlyMp4FileParseData(file, buf, BAR_FLY_MP4_ATOM_SIZE_LENGTH,
		settings);
	if (status != 0) {
		goto error;
	}

	*number = _BarFlyMp4BufToLong(buf);

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

static void _BarFlyMp4LongRender(uint8_t* size, long unsigned lsize)
{
	int index;

	assert(size != NULL);

	for (index = 0; index < BAR_FLY_MP4_ATOM_SIZE_LENGTH; index++) {
		size[index] = lsize >> 8 * (BAR_FLY_MP4_ATOM_SIZE_LENGTH - 1 - index);
	}

	return;
}

static void _BarFlyMp4ShortRender(uint8_t* buffer, short unsigned value)
{
	int index;

	assert(buffer != NULL);

	for (index = 0; index < BAR_FLY_MP4_SHORT_LENGTH; index++) {
		buffer[index] = value >> 8 * (BAR_FLY_MP4_SHORT_LENGTH - 1 - index);
	}

	return;
}

static int _BarFlyMp4TagAddAtom(BarFlyMp4Tag_t* tag, char const* parent_path,
		BarFlyMp4Atom_t* atom, bool update_offsets,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	BarFlyMp4Atom_t* parent;
	BarFlyMp4Atom_t** children = NULL;

	assert(tag != NULL);
	assert(parent_path != NULL);
	assert(atom != NULL);
	assert(settings != NULL);

	if (parent_path[0] == '\0') {
		/*
		 * This atom has no parent so put it in the top level atoms of the tag.
		 */
		children = realloc(tag->atoms,
				sizeof(BarFlyMp4Atom_t*) * (tag->atom_count + 1));
		if (children == NULL) {
			BarUiMsg(settings, MSG_ERR, "Error allocating memory (%d bytes).\n",
					sizeof(BarFlyMp4Atom_t*) * (tag->atom_count + 1));
			goto error;
		}
		tag->atoms = children;
		children = NULL;

		tag->atoms[tag->atom_count] = atom;
		tag->atom_count++;
	} else {
		/*
		 * Find the parent atom and add the atom as its child.
		 */
		parent = _BarFlyMp4TagFindAtom(tag, parent_path);
		if (parent == NULL) {
			goto error;
		}

		status = _BarFlyMp4AtomAddChild(parent, atom, settings);
		if (status != 0) {
			goto error;
		}
	}

	/*
	 * Update the file offsets.
	 */
	if (update_offsets) {
		status = _BarFlyMp4TagUpdateOffsets(tag, atom->size, settings);
		if (status != 0) {
			goto error;
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	if (children != NULL) {
		free(children);
	}
	return exit_status;
}

static int _BarFlyMp4TagAddMetaAtom(BarFlyMp4Tag_t* tag, char const* name,
		uint8_t const* class, uint8_t const* data, size_t data_size,
		BarSettings_t const* settings)
{
	uint8_t const META_DATA[] = BAR_FLY_MP4_ATOM_META_DATA;
	uint8_t const HDLR_DATA[] = BAR_FLY_MP4_ATOM_HDLR_DATA;
	uint8_t const NULL_DATA[] = {0x00, 0x00, 0x00, 0x00};

	int exit_status = 0;
	int status;
	BarFlyMp4Atom_t* meta_atom = NULL;
	BarFlyMp4Atom_t* data_atom = NULL;
	BarFlyMp4Atom_t* atom = NULL;

	assert(tag != NULL);
	assert(name != NULL);
	assert(class != NULL);
	assert(data != NULL);

	/*
	 * Create the ilst atom if it doesn't exist.
	 */
	if (_BarFlyMp4TagFindAtom(tag, "moov.udta.meta.ilst") == NULL) {
		/*
		 * Create the udta atom if it doesn't exist.
		 */
		if (_BarFlyMp4TagFindAtom(tag, "moov.udta") == NULL) {
			atom = _BarFlyMp4AtomOpen("udta", -1, settings);
			if (atom == NULL) {
				goto error;
			}

			status = _BarFlyMp4TagAddAtom(tag, "moov", atom, true, settings);
			if (status != 0) {
			}
			atom = NULL;
		}

		/*
		 * Create the meta atom if it doesn't exist.
		 */
		if (_BarFlyMp4TagFindAtom(tag, "moov.udta.meta") == NULL) {
			atom = _BarFlyMp4AtomOpen("meta", -1, settings);
			if (atom == NULL) {
				goto error;
			}

			status = _BarFlyMp4AtomAppendData(atom, tag->mp4_file, META_DATA,
					sizeof(META_DATA), settings);
			if (status != 0) {
				goto error;
			}
			
			status = _BarFlyMp4TagAddAtom(tag, "moov.udta", atom, true,
					settings);
			if (status != 0) {
				goto error;
			}
			atom = NULL;
		}

		/*
		 * Create the hdlr atom if it doesn't exist.
		 */
		if (_BarFlyMp4TagFindAtom(tag, "moov.udta.meta.hdlr") == NULL) {
			atom = _BarFlyMp4AtomOpen("hdlr", -1, settings);
			if (atom == NULL) {
				goto error;
			}
			
			status = _BarFlyMp4AtomAppendData(atom, tag->mp4_file, HDLR_DATA,
					sizeof(HDLR_DATA), settings);
			if (status != 0) {
				goto error;
			}
			
			status = _BarFlyMp4TagAddAtom(tag, "moov.udta.meta", atom, true,
					settings);
			if (status != 0) {
				goto error;
			}
			atom = NULL;
		}

		/*
		 * Create the ilst atom.
		 */
		atom = _BarFlyMp4AtomOpen("ilst", -1, settings);
		if (atom == NULL) {
			goto error;
		}

		status = _BarFlyMp4TagAddAtom(tag, "moov.udta.meta", atom, true,
				settings);
		if (status != 0) {
			goto error;
		}
		atom = NULL;
	}

	/*
	 * Create the meta data atom.
	 */
	meta_atom = _BarFlyMp4AtomOpen(name, -1, settings);
	if (meta_atom == NULL) {
		goto error;
	}

	/*
	 * Create the child data atom.
	 */
	data_atom = _BarFlyMp4AtomOpen("data", -1, settings);
	if (data_atom == NULL) {
		goto error;
	}

	status = _BarFlyMp4AtomAppendData(data_atom, tag->mp4_file, class,
			BAR_FLY_MP4_ATOM_CLASS_LENGTH, settings);
	if (status != 0) {
		goto error;
	}

	status = _BarFlyMp4AtomAppendData(data_atom, tag->mp4_file, NULL_DATA,
			sizeof(NULL_DATA), settings);
	if (status != 0) {
		goto error;
	}

	status = _BarFlyMp4AtomAppendData(data_atom, tag->mp4_file, data,
			data_size, settings);
	if (status != 0) {
		goto error;
	}

	/*
	 * Add the meta data atom to the tag.
	 */
	status = _BarFlyMp4AtomAddChild(meta_atom, data_atom, settings);
	if (status != 0) {
		goto error;
	}
	data_atom = NULL;

	status = _BarFlyMp4TagAddAtom(tag, "moov.udta.meta.ilst", meta_atom, true,
			settings);
	if (status != 0) {
	}
	meta_atom = NULL;

	goto end;

error:
	exit_status = -1;

end:
	_BarFlyMp4AtomClose(meta_atom);
	_BarFlyMp4AtomClose(data_atom);
	_BarFlyMp4AtomClose(atom);

	return exit_status;
}

static BarFlyMp4Atom_t* _BarFlyMp4TagFindAtom(BarFlyMp4Tag_t const* tag,
		char const* path)
{
	BarFlyMp4Atom_t* atom = NULL;
	char* atom_name;
	char path_sep[BAR_FLY_NAME_LENGTH];
	char* path_ptr;
	int index;
	BarFlyMp4Atom_t* child = NULL;

	assert(tag != NULL);
	assert(path != NULL);

	strncpy(path_sep, path, BAR_FLY_NAME_LENGTH);
	path_sep[BAR_FLY_NAME_LENGTH - 1] = '\0';

	/*
	 * Find the top level atom.
	 */
	atom = NULL;
	path_ptr = path_sep;
	atom_name = strsep(&path_ptr, ".");
	for (index = 0; (atom == NULL) && (index < tag->atom_count); index++) {
		if (strcmp(tag->atoms[index]->name, atom_name) == 0) {
			atom = tag->atoms[index];
		}
	}

	if (atom == NULL) {
		goto error;
	}

	/*
	 * Find the child atom.
	 */
	atom_name = strsep(&path_ptr, ".");
	while (atom_name != NULL) {
		child = NULL;
		for (index = 0;
			 (child == NULL) && (index < atom->child_count);
			 index++) {
			if (strcmp(atom->children[index]->name, atom_name) == 0) {
				child = atom->children[index];
			}
		}

		if (child == NULL) {
			goto error;
		}

		atom = child;
		atom_name = strsep(&path_ptr, ".");
	}

	goto end;

error:
	atom = NULL;

end:
	return atom;
}

static int _BarFlyMp4TagUpdateOffsets(BarFlyMp4Tag_t* tag, long delta,
		BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	BarFlyMp4Atom_t* stco_atom;
	long unsigned count;
	uint8_t* pos;
	long unsigned offset;

	assert(tag != NULL);
	assert(settings != NULL);

	/*
	 * If the stco atom is found update it's offsets.
	 */
	stco_atom = _BarFlyMp4TagFindAtom(tag, "moov.trak.mdia.minf.stbl.stco");
	if (stco_atom != NULL) {
		/*
		 * Make sure the stco's data buffer has been read from the file.
		 */
		status = _BarFlyMp4FileParseAtomData(tag->mp4_file, stco_atom,
				settings);
		if (status != 0) {
			goto error;
		}

		/*
		 * Adjust all the offsets by the delta.
		 */
		pos = stco_atom->data + 4;
		count = _BarFlyMp4BufToLong(pos);
		while (count > 0) {
			pos += 4;
			offset = _BarFlyMp4BufToLong(pos) + delta;
			_BarFlyMp4LongRender(pos, offset);
			count--;
		}
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}


int BarFlyMp4TagAddAlbum(BarFlyMp4Tag_t* tag, char const* album,
		BarSettings_t const* settings)
{
	uint8_t ALBUM_CLASS[] = BAR_FLY_MP4_ATOM_ALBUM_CLASS;

	int exit_status = 0;
	int status;

	assert(tag != NULL);
	assert(album != NULL);
	assert(settings != NULL);

	/*
	 * Add the album atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "\251alb", ALBUM_CLASS,
			(uint8_t const*)album, strlen(album), settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddArtist(BarFlyMp4Tag_t* tag, char const* artist,
		BarSettings_t const* settings)
{
	uint8_t ARTIST_CLASS[] = BAR_FLY_MP4_ATOM_ARTIST_CLASS;

	int exit_status = 0;
	int status;

	assert(tag != NULL);
	assert(artist != NULL);
	assert(settings != NULL);

	/*
	 * Add the artist atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "\251ART", ARTIST_CLASS,
			(uint8_t const*)artist, strlen(artist), settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddCoverArt(BarFlyMp4Tag_t* tag, uint8_t const* cover_art,
		size_t cover_size, BarSettings_t const* settings)
{
	uint8_t COVER_CLASS[] = BAR_FLY_MP4_ATOM_COVER_CLASS;

	int exit_status = 0;
	int status;

	assert(tag != NULL);
	assert(cover_art != NULL);
	assert(settings != NULL);

	/*
	 * Add the covr atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "covr", COVER_CLASS, cover_art,
			cover_size, settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddDisk(BarFlyMp4Tag_t* tag, short unsigned disk,
		BarSettings_t const* settings)
{
	uint8_t DISK_CLASS[] = BAR_FLY_MP4_ATOM_DISK_CLASS;

	int exit_status = 0;
	int status;
	uint8_t buffer[8] = {0x00};

	assert(tag != NULL);
	assert(settings != NULL);

	/*
	 * Render the disk number to the data buffer.
	 */
	_BarFlyMp4ShortRender(&buffer[2], disk);

	/*
	 * Add the disk atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "disk", DISK_CLASS, buffer, 8,
			settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddTitle(BarFlyMp4Tag_t* tag, char const* title,
		BarSettings_t const* settings)
{
	uint8_t TITLE_CLASS[] = BAR_FLY_MP4_ATOM_TITLE_CLASS;

	int exit_status = 0;
	int status;

	assert(tag != NULL);
	assert(title != NULL);
	assert(settings != NULL);

	/*
	 * Add the title atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "\251nam", TITLE_CLASS,
			(uint8_t const*)title, strlen(title), settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddTrack(BarFlyMp4Tag_t* tag, short unsigned track,
		BarSettings_t const* settings)
{
	uint8_t TRACK_CLASS[] = BAR_FLY_MP4_ATOM_TRACK_CLASS;

	int exit_status = 0;
	int status;
	uint8_t buffer[8] = {0x00};

	assert(tag != NULL);
	assert(settings != NULL);

	/*
	 * Render the track number to the data buffer.
	 */
	_BarFlyMp4ShortRender(&buffer[2], track);

	/*
	 * Add the track atom to the tag.
	 */
	status = _BarFlyMp4TagAddMetaAtom(tag, "trkn", TRACK_CLASS, buffer, 8,
			settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

int BarFlyMp4TagAddYear(BarFlyMp4Tag_t* tag, short unsigned year,
		BarSettings_t const* settings)
{
	uint8_t YEAR_CLASS[] = BAR_FLY_MP4_ATOM_YEAR_CLASS;

	int exit_status = 0;
	int status;
	char year_str[5];

	assert(tag != NULL);
	assert(settings != NULL);

	/*
	 * Add the year atom to the tag.
	 */
	snprintf(year_str, 5, "%hu", year);
	year_str[4] = '\0';
	status = _BarFlyMp4TagAddMetaAtom(tag, "\251day", YEAR_CLASS,
			(uint8_t const*)year_str, strlen(year_str), settings);
	if (status != 0) {
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	return exit_status;
}

void BarFlyMp4TagClose(BarFlyMp4Tag_t* tag)
{
	int index;

	if (tag != NULL) {
		if (tag->file_path != NULL) {
			free(tag->file_path);
		}

		if (tag->mp4_file != NULL) {
			fclose(tag->mp4_file);
		}

		/*
		 * Free all the atoms and the atom array itself.
		 */
		if (tag->atoms != NULL) {
			for (index = 0; index < tag->atom_count; index++) {
				_BarFlyMp4AtomClose(tag->atoms[index]);
			}
			free(tag->atoms);
		}

		free(tag);
	}

	return;
}

BarFlyMp4Tag_t* BarFlyMp4TagOpen(char const* file_path,
		BarSettings_t const* settings)
{
	BarFlyMp4Tag_t* tag = NULL;
	int status;
	BarFlyMp4Atom_t* atom = NULL;
	size_t size;
	char name[BAR_FLY_MP4_ATOM_NAME_LENGTH + 1];

	assert(file_path != NULL);
	assert(settings != NULL);

	/*
	 * Create the tag and copy the file path to it.
	 */
	tag = calloc(1, sizeof(BarFlyMp4Tag_t));
	if (tag == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error allocating memory (%d bytes).\n",
				sizeof(BarFlyMp4Tag_t));
		goto error;
	}

	tag->file_path = strdup(file_path);
	if (tag->file_path == NULL) {
		BarUiMsg(settings, MSG_ERR, "Error duplicating a string (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	/*
	 * Read the ftyp atom from the MP4 file.  The first atom in the file must
	 * be named "ftyp".
	 */
	tag->mp4_file = fopen(tag->file_path, "rb");
	if (tag->mp4_file == NULL) {
		BarUiMsg(settings, MSG_ERR,
				"Error opening the MP4 file (%s) (%d:%s).\n",
				tag->file_path, errno, strerror(errno));
		goto error;
	}

	status = _BarFlyMp4FileParseAtomSize(tag->mp4_file, &size, settings);
	if (status != 0) {
		goto error;
	}

	status = _BarFlyMp4FileParseAtomName(tag->mp4_file, name, settings);
	if (status != 0) {
		goto error;
	}

	if (strcmp(name, "ftyp") != 0) {
		BarUiMsg(settings, MSG_ERR, "The first atom was not named 'ftyp'.\n");
		goto error;
	}

	status = fseek(tag->mp4_file,
			size - BAR_FLY_MP4_ATOM_SIZE_LENGTH - BAR_FLY_MP4_ATOM_NAME_LENGTH,
			SEEK_CUR);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error seeking in the file (%d:%s).\n",
				errno,
				strerror(errno));
		goto error;
	}

	/*
	 * Read the moov atom from the MP4 file.  The second atom in the file must
	 * be named "moov".  This is the only top level atom that will be in the
	 * tag.
	 */
	atom = _BarFlyMp4FileParseAtom(tag->mp4_file, settings);
	if (atom == NULL) {
		goto error;
	}

	if (strcmp(atom->name, "moov") != 0) {
		BarUiMsg(settings, MSG_ERR, "The second atom was not named 'moov'.\n");
		goto error;
	}

	status = _BarFlyMp4TagAddAtom(tag, "", atom, false, settings);
	if (status != 0) {
		goto error;
	}
	atom = NULL;

	goto end;

error:
	BarFlyMp4TagClose(tag);
	tag = NULL;

end:
	_BarFlyMp4AtomClose(atom);

	return tag;
}

int BarFlyMp4TagWrite(BarFlyMp4Tag_t* tag, BarSettings_t const* settings)
{
	int exit_status = 0;
	int status;
	uint8_t* buffer = NULL;
	FILE* tmp_file = NULL;
	uint8_t audio_buffer[BAR_FLY_COPY_BLOCK_SIZE];
	size_t audio_buf_size;
	size_t read_count;
	size_t write_count;
	char tmp_file_path[L_tmpnam];
	char* junk;
	size_t atom_size;
	BarFlyMp4Atom_t* atom;

	assert(tag != NULL);
	assert(settings != NULL);

	/*
	 * Open the tmp file.
	 *
	 * Assigning the return value of tmpnam() to a junk pointer to get the
	 * compiler to be quiet.
	 */
	junk = tmpnam(tmp_file_path);
	junk = junk;
	tmp_file = fopen(tmp_file_path, "wb");
	if (tmp_file == NULL) {
		BarUiMsg(settings, MSG_ERR,
				"Error opening the temporary file (%s) (%d:%s).\n",
				tmp_file_path, errno, strerror(errno));
		goto error;
	}

	status = fseek(tag->mp4_file, 0, SEEK_SET);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error seeking in the file (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	/*
	 * Copy everything up to the start of the moov atom from the MP4 file to the
	 * tmp file.
	 */
	atom = _BarFlyMp4TagFindAtom(tag, "moov");
	if (atom == NULL) {
		goto error;
	}

	atom_size = atom->offset;
	while (atom_size > 0) {
		audio_buf_size = (atom_size < BAR_FLY_COPY_BLOCK_SIZE) ? (atom_size) :
					(BAR_FLY_COPY_BLOCK_SIZE);
		read_count = fread(audio_buffer, 1, audio_buf_size, tag->mp4_file);
		if (read_count != audio_buf_size) {
			BarUiMsg(settings, MSG_ERR,
					"Error reading from the MP4 file (%d:%s).\n",
					errno, strerror(errno));
			goto error;
		}

		write_count = fwrite(audio_buffer, 1, audio_buf_size, tmp_file);
		if (write_count != read_count) {
			BarUiMsg(settings, MSG_ERR,
					"Error writing to the tmp file (%d:%s).\n",
					errno, strerror(errno));
			goto error;
		}

		atom_size -= audio_buf_size;
	}

	/*
	 * Render the moov atom to the tmp file.
	 */
	status = _BarFlyMp4AtomRender(atom, tag->mp4_file, tmp_file, settings);
	if (status != 0) {
		goto error;
	}

	/*
	 * Copy everything after the moov atom to the tmp file.
	 */
	status = fseek(tag->mp4_file, atom->offset, SEEK_SET);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error seeking in the file (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	status = _BarFlyMp4FileParseAtomSize(tag->mp4_file, &atom_size, settings);
	if (status != 0) {
		goto error;
	}

	status = fseek(tag->mp4_file, atom_size - BAR_FLY_MP4_ATOM_SIZE_LENGTH,
			SEEK_CUR);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error seeking in the file (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	while (feof(tag->mp4_file) == 0) {
		read_count = fread(audio_buffer, 1, BAR_FLY_COPY_BLOCK_SIZE,
				tag->mp4_file);
		if ((read_count != BAR_FLY_COPY_BLOCK_SIZE) &&
			(feof(tag->mp4_file) == 0)) {
			BarUiMsg(settings, MSG_ERR,
					"Error reading from the MP4 file (%d:%s).\n",
					errno, strerror(errno));
			goto error;
		}

		write_count = fwrite(audio_buffer, 1, read_count, tmp_file);
		if (write_count != read_count) {
			BarUiMsg(settings, MSG_ERR,
					"Error writing to the tmp file (%d:%s).\n",
					errno, strerror(errno));
			goto error;
		}
	}

	/*
	 * Overwrite the audio file with the tmp file.
	 */
	fclose(tmp_file);
	tmp_file = NULL;

	fclose(tag->mp4_file);
	tag->mp4_file = NULL;

	status = rename(tmp_file_path, tag->file_path);
	if (status != 0) {
		BarUiMsg(settings, MSG_ERR, "Error overwriting the MP4 file (%d:%s).\n",
				errno, strerror(errno));
		goto error;
	}

	goto end;

error:
	exit_status = -1;

end:
	if (buffer != NULL) {
		free(buffer);
	}

	if (tmp_file != NULL) {
		fclose(tmp_file);
	}

	return exit_status;
}

#endif

// vim: set noexpandtab:
