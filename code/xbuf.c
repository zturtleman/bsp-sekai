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
#include <stdlib.h>
#include <string.h>
#include "xbuf.h"

/* expandable buffers */

FILE *openfile_write(const char *filename, char **out_error)
{
	int file_exists;
	FILE *fp;

	file_exists = 0;
	if ((fp = fopen(filename, "rb")))
	{
		file_exists = 1;
		fclose(fp);
	}

    /*
	if (file_exists)
	{
		printf("File %s already exists. Overwrite? [y/N] ", filename);
		if (!yesno())
			return (void)(out_error && (*out_error = msprintf("user aborted operation"))), NULL;
	}
    */

	fp = fopen(filename, "wb");
	if (!fp)
		return (void)(out_error && (*out_error = ("couldn't open file"))), NULL;

	return fp;
}

typedef struct xbuf_block_s
{
	unsigned char *memory;

	size_t bytes_written;

	struct xbuf_block_s *prev, *next;
} xbuf_block_t;

struct xbuf_s
{
	size_t block_size;

	xbuf_block_t *block_head, *block_tail;

	size_t bytes_written;

	FILE *fp;

	char *error;
};

/* if this is a file buffer, flush the block's contents into the file and reset the block */
static void xbuf_flush(xbuf_t *xbuf)
{
	if (xbuf->error || !xbuf->fp)
		return;

	if (fwrite(xbuf->block_head->memory, 1, xbuf->block_head->bytes_written, xbuf->fp) < xbuf->block_head->bytes_written)
	{
		xbuf->error = ("failed to write to file");
		return;
	}

	xbuf->block_head->bytes_written = 0;
}

static xbuf_block_t *xbuf_new_block(xbuf_t *xbuf)
{
	xbuf_block_t *block;

	block = (xbuf_block_t*)malloc(sizeof(xbuf_block_t) + xbuf->block_size);
	if (!block)
		return NULL;

	block->memory = (unsigned char*)(block + 1);
	block->bytes_written = 0;

	block->prev = xbuf->block_tail;
	block->next = NULL;

	if (block->prev)
		block->prev->next = block;

	if (!xbuf->block_head)
		xbuf->block_head = block;
	xbuf->block_tail = block;

	return block;
}

xbuf_t *xbuf_create_memory(size_t block_size, char **out_error)
{
	xbuf_t *xbuf;

	if (block_size < 4096)
		block_size = 4096;

	xbuf = (xbuf_t*)malloc(sizeof(xbuf_t));
	if (!xbuf)
		return (void)(out_error && (*out_error = ("out of memory"))), NULL;

	xbuf->block_size = block_size;
	xbuf->block_head = NULL;
	xbuf->block_tail = NULL;
	xbuf->bytes_written = 0;
	xbuf->fp = NULL;
	xbuf->error = NULL;

	if (!xbuf_new_block(xbuf)) /* create the first block */
	{
		free(xbuf);
		return (void)(out_error && (*out_error = ("out of memory"))), NULL;
	}

	return xbuf;
}

xbuf_t *xbuf_create_file(size_t block_size, const char *filename, char **out_error)
{
	xbuf_t *xbuf;

	if (!filename)
		return (void)(out_error && (*out_error = ("no filename specified"))), NULL;

	xbuf = xbuf_create_memory(block_size, out_error);
	if (!xbuf)
		return NULL;

	xbuf->fp = openfile_write(filename, out_error);
	if (!xbuf->fp)
	{
		xbuf_free(xbuf, NULL);
		return NULL;
	}

	return xbuf;
}

int xbuf_free(xbuf_t *xbuf, char **out_error)
{
	char *error = xbuf->error;
	xbuf_block_t *block, *nextblock;

	if (xbuf->fp)
		fclose(xbuf->fp);

	for (block = xbuf->block_head; block; block = nextblock)
	{
		nextblock = block->next;
		free(block);
	}

	free(xbuf);

	if (error)
	{
		if (out_error)
			*out_error = error;
		else
			free(error);
		return 0;
	}
	else
	{
		return 1;
	}
}

/* write a piece of data, which might end up spanning multiple blocks */
void xbuf_write_data(xbuf_t *xbuf, size_t length, const void *data)
{
	xbuf_block_t *block;

	if (xbuf->error)
		return;

	block = xbuf->block_tail;

	for (;;)
	{
		if (length < xbuf->block_size - block->bytes_written)
		{
		/* data fits in current block */
			memcpy(block->memory + block->bytes_written, data, length);

			xbuf->bytes_written += length;
			block->bytes_written += length;

			return;
		}
		else
		{
		/* fill the rest of the current block with as much data as fits */
			size_t amt = xbuf->block_size - block->bytes_written;

			memcpy(block->memory + block->bytes_written, data, amt);

			xbuf->bytes_written += amt;
			block->bytes_written += amt;

			length -= amt;
			data = (unsigned char*)data + amt;

		/* get more space */
			if (xbuf->fp)
				xbuf_flush(xbuf);
			else
				block = xbuf_new_block(xbuf);
		}
	}
}

void xbuf_write_byte(xbuf_t *xbuf, unsigned char byte)
{
	xbuf_write_data(xbuf, 1, &byte);
}

/* reserve a contiguous piece of memory */
void *xbuf_reserve_data(xbuf_t *xbuf, size_t length)
{
	xbuf_block_t *block;
	void *memory;

	if (xbuf->error)
		return NULL;
	if (xbuf->fp)
		return NULL; /* this won't work on file buffers currently (FIXME - is it possible?) */
	if (length > xbuf->block_size)
		return NULL;

/* get the current block */
	block = xbuf->block_tail;

/* if it won't fit in the current block, get more space */
	if (length >= xbuf->block_size - block->bytes_written)
	{
		if (xbuf->fp)
			xbuf_flush(xbuf);
		else
			block = xbuf_new_block(xbuf);
	}

/* return memory pointer and advance bytes_written */
	memory = block->memory + block->bytes_written;

	xbuf->bytes_written += length;
	block->bytes_written += length;

	return memory;
}

int xbuf_get_bytes_written(const xbuf_t *xbuf)
{
	return xbuf->bytes_written;
}

int xbuf_write_to_file(xbuf_t *xbuf, const char *filename, char **out_error)
{
	FILE *fp;
	xbuf_block_t *block;

	if (xbuf->error)
		return (void)(out_error && (*out_error = ("cannot write buffer to file: a previous error occurred"))), 0;
	if (xbuf->fp)
		return (void)(out_error && (*out_error = ("xbuf_write_to_file called on a file buffer"))), 0;

	fp = openfile_write(filename, out_error);
	if (!fp)
		return 0;

	for (block = xbuf->block_head; block; block = block->next)
	{
		if (fwrite(block->memory, 1, block->bytes_written, fp) < block->bytes_written)
		{
			if (out_error)
				*out_error = ("failed to write to file");
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);
	return 1;
}

/* return the data in a newly allocated contiguous block, then free the xbuf */
int xbuf_finish_memory(xbuf_t *xbuf, void **out_data, size_t *out_length, char **out_error)
{
	void *data;
	unsigned char *p;
	xbuf_block_t *block;

	if (xbuf->error)
		return xbuf_free(xbuf, out_error);

	data = malloc(xbuf->bytes_written);
	if (!data)
	{
		xbuf_free(xbuf, NULL);
		return (void)(out_error && (*out_error = ("out of memory"))), 0;
	}

	p = (unsigned char*)data;
	for (block = xbuf->block_head; block; block = block->next)
	{
		memcpy(p, block->memory, block->bytes_written);
		p += block->bytes_written;
	}

	*out_data = data;
	*out_length = xbuf->bytes_written;

	return xbuf_free(xbuf, out_error); /* this will always return true */
}

/* finish writing to file, then free the xbuf */
int xbuf_finish_file(xbuf_t *xbuf, char **out_error)
{
	xbuf_flush(xbuf);

	return xbuf_free(xbuf, out_error);
}
