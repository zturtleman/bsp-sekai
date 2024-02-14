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

#ifndef IMAGE_H
#define IMAGE_H

#include "xbuf.h"

/* 32-bit image */
typedef struct image_rgba_s
{
    char name[256];

	int width, height;
	unsigned char *pixels;
	int num_nonempty_pixels;
    int num_alpha_pixels;

    struct image_rgba_s *next;
} image_rgba_t;

extern image_rgba_t *image_notexture;

void image_init();
void image_shutdown();

image_rgba_t *image_load_from_file(const char *filename, char **out_error);
int image_save(const char *filename, const image_rgba_t *image, char **out_error);

image_rgba_t *image_alloc(int width, int height);
void image_free(image_rgba_t **image);

image_rgba_t *image_tga_load(void *filedata, size_t filesize, char **out_error);
int image_tga_save(const image_rgba_t *image, xbuf_t *xbuf, char **out_error);

#endif
