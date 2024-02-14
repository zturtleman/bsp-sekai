/*
===========================================================================
Copyright (C) 2015 Zack Middleton

This file is part of BSP sekai Source Code.

BSP sekai Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

BSP sekai Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with BSP sekai Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, BSP sekai Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef float vec3_t[3];
typedef uint8_t byte;

typedef enum {
	qfalse = 0,
	qtrue = 1
} qboolean;

#define MAX_QPATH 64

#define ERR_DROP 0	// passed to Com_Error, ignored

#define Com_Memset memset
#define Com_Memcpy memcpy
#define Com_Printf printf

void COM_StripExtension( const char *in, char *out );
char *COM_SkipPath( char *pathname );

#define Com_Error( err, ... ) do { Com_Printf( __VA_ARGS__ ); Com_Printf( "\n" ); exit( 1 ); } while (0)
#define Q_strncpyz( dst, src, size ) do { strncpy( dst, src, size-1 ); dst[size-1] = 0; } while (0)
#define ARRAY_LEN( x ) ( sizeof ( x ) / sizeof ( x[0] ) )

#ifdef WIN32
#define Q_stricmp stricmp
#else
#define Q_stricmp strcasecmp
#endif

// FIXME: assumes host is little endian
#define LittleShort(x) (x)
#define LittleLong(x) (x)
#define LittleFloat(x) (x)

#define VectorSet( v, a, b, c ) do { v[0] = a; v[1] = b; v[2] = c; } while (0)
#define VectorCopy( src, dst ) do { dst[0] = src[0]; dst[1] = src[2]; dst[2] = src[2]; } while (0)


// main.c
long FS_WriteFile( const char *filename, void *buf, long length );
long FS_ReadFile( const char *filename, void **buffer );
void FS_FreeFile( void *buffer );

// md4.c
unsigned Com_BlockChecksum (const void *buffer, int length);

