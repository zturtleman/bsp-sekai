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

#ifndef __XBUF_H__
#define __XBUF_H__

#include <stddef.h>

/* expandable buffers */

typedef struct xbuf_s xbuf_t;

xbuf_t *xbuf_create_memory(size_t block_size, char **out_error);
xbuf_t *xbuf_create_file(size_t block_size, const char *filename, char **out_error);

int xbuf_free(xbuf_t *xbuf, char **out_error);
int xbuf_finish_memory(xbuf_t *xbuf, void **out_data, size_t *out_length, char **out_error);
int xbuf_finish_file(xbuf_t *xbuf, char **out_error);

int xbuf_write_to_file(xbuf_t *xbuf, const char *filename, char **out_error);

void xbuf_write_data(xbuf_t *xbuf, size_t length, const void *data);
void xbuf_write_byte(xbuf_t *xbuf, unsigned char byte);
void *xbuf_reserve_data(xbuf_t *xbuf, size_t length);

int xbuf_get_bytes_written(const xbuf_t *xbuf);

#endif /* __XBUF_H__ */