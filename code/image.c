/*
    QShed <http://www.icculus.org/qshed>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "image.h"
#include "xbuf.h"

#include "sekai.h"

#define FILE_HASH_SIZE 256
#define MAX_IMAGES 1024

static	image_rgba_t*		hashTable[FILE_HASH_SIZE];
image_rgba_t *image_notexture;
static image_rgba_t images[MAX_IMAGES];
static int numImages;

static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

void image_init()
{
	long hash;

	Com_Memset(hashTable, 0, sizeof(hashTable));
	numImages = 0;
	Com_Memset(images, 0, sizeof(images));

	hash = generateHashValue( "notexture.tga" );
	image_notexture = image_alloc(8, 8);
	Com_Memset(image_notexture->pixels, 0xFF, image_notexture->width * image_notexture->height * 4);

	Q_strncpyz( image_notexture->name, "notexture.tga", sizeof ( image_notexture->name ) );
	image_notexture->next = hashTable[hash];
	hashTable[hash] = image_notexture;
}

void image_shutdown()
{
	int i;

	Com_Memset(hashTable, 0, sizeof(hashTable));
	image_notexture = NULL;
	for (i = 0; i < numImages; i++)
	{
		free(images[i].pixels);
	}
}


typedef enum image_format_e
{
	IMGFMT_MISSING,
	IMGFMT_UNRECOGNIZED,
	IMGFMT_TGA,
} image_format_t;

static image_format_t get_image_format(const char *filename)
{
	const char *ext = strrchr(filename, '.');

	if (!ext)
		return IMGFMT_MISSING;

	if (!strcasecmp(ext, ".tga"))
		return IMGFMT_TGA;

	return IMGFMT_UNRECOGNIZED;
}

image_rgba_t *image_load(const char *filename, void *filedata, size_t filesize, char **out_error)
{
	switch (get_image_format(filename))
	{
	default:
	case IMGFMT_MISSING:
		return (void)(out_error && (*out_error = ("missing file extension"))), NULL;
	case IMGFMT_UNRECOGNIZED:
		return (void)(out_error && (*out_error = ("unrecognized file extension"))), NULL;
	case IMGFMT_TGA: return image_tga_load(filedata, filesize, out_error);
	}
}

int loadfile(const char *filename, void **out_data, size_t *out_size, char **out_error)
{
	FILE *fp;
	long ftellret;
	unsigned char *filemem;
	size_t filesize, readsize;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		if (out_error)
			*out_error = ("Couldn't open file");
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	ftellret = ftell(fp);
	if (ftellret < 0) /* ftell returns -1L and sets errno on failure */
	{
		if (out_error)
			*out_error = ("Couldn't seek to end of file");
		fclose(fp);
		return 0;
	}
	filesize = (size_t)ftellret;
	fseek(fp, 0, SEEK_SET);

	filemem = (unsigned char*)malloc(filesize + 1);
	if (!filemem)
	{
		if (out_error)
			*out_error = ("Couldn't allocate memory to open file");
		fclose(fp);
		return 0;
	}

	readsize = fread(filemem, 1, filesize, fp);
	fclose(fp);

	if (readsize < filesize)
	{
		if (out_error)
			*out_error = ("Failed to read file");
		free(filemem);
		return 0;
	}

	filemem[filesize] = 0;

	*out_data = (void*)filemem;
	if (out_size)
		*out_size = filesize;

	return 1;
}

image_rgba_t *image_load_from_file(const char *filename, char **out_error)
{
	void *filedata;
	size_t filesize;
	image_rgba_t *image;
	long hash;
	int		i, s;

	hash = generateHashValue(filename);

	for (image=hashTable[hash]; image; image=image->next) {
		if ( !strcmp( filename, image->name ) ) {
			return image;
		}
	}

	if (!loadfile(filename, &filedata, &filesize, out_error))
		return NULL;

	image = image_load(filename, filedata, filesize, out_error);
	if ( out_error && *out_error ) {
		free(filedata);
		image_free(&image);
		return NULL;
	}

	s = image->width * image->height;
	for (i = 0; i < s; i++) {
		if (image->pixels[i * 4 + 3] != 255) {
			image->num_alpha_pixels++;
		}
	}

	Q_strncpyz( image->name, filename, sizeof ( image->name ) );
	image->next = hashTable[hash];
	hashTable[hash] = image;

	free(filedata);
	return image;
}

int image_save(const char *filename, const image_rgba_t *image, char **out_error)
{
	int (*savefunc)(const image_rgba_t *image, xbuf_t *xbuf, char **out_error) = NULL;
	xbuf_t *xbuf;

	switch (get_image_format(filename))
	{
	default:
	case IMGFMT_MISSING:
		return (void)(out_error && (*out_error = ("missing file extension"))), 0;
	case IMGFMT_UNRECOGNIZED:
		return (void)(out_error && (*out_error = ("unrecognized file extension"))), 0;
	case IMGFMT_TGA:
		savefunc = image_tga_save;
		break;
	}

/* allocate write buffer and set it up to flush directly to the file */
	xbuf = xbuf_create_file(262144, filename, out_error);
	if (!xbuf)
		return 0;

/* write image data */
	if (!(*savefunc)(image, xbuf, out_error))
	{
		xbuf_free(xbuf, NULL);
		return 0;
	}

/* flush any remaining data to file and free the buffer */
	return xbuf_finish_file(xbuf, out_error);
}

image_rgba_t *image_alloc(int width, int height)
{
	image_rgba_t *image;
	if (width < 1 || height < 1)
		return NULL;
	if (numImages >= MAX_IMAGES)
		return NULL;
	void *pixels = malloc(width * height * 4);
	if (pixels == NULL)
		return NULL;
	image = &images[numImages++];
	image->width = width;
	image->height = height;
	image->pixels = (unsigned char*)pixels;
	image->num_alpha_pixels = 0;
	image->num_nonempty_pixels = 0;
	image->next = NULL;
	return image;
}

void image_free(image_rgba_t **image)
{
	*image = NULL;
}

