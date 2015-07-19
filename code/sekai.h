// GPLv2+

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

#define Com_Error( err, ... ) do { Com_Printf( __VA_ARGS__ ); exit( 1 ); } while (0)
#define Q_strncpyz( dst, src, size ) do { strncpy( dst, src, size-1 ); dst[size-1] = 0; } while (0)
#define ARRAY_LEN( x ) ( sizeof ( x ) / sizeof ( x[0] ) )

#ifdef WIN32
#define Q_stricmp stricmp
#else
#define Q_stricmp strcasecmp
#endif

// FIXME: assumes host is little endiness
#define LittleShort(x) (x)
#define LittleLong(x) (x)
#define LittleFloat(x) (x)

#define VectorSet( v, a, b, c ) do { v[0] = a; v[1] = b; v[2] = c; } while (0)
#define VectorCopy( src, dst ) do { dst[0] = src[0]; dst[1] = src[2]; dst[2] = src[2]; } while (0)


// main.c
long FS_WriteFile( const char *filename, void *buf, long length );
long FS_ReadFile( const char *filename, void **buffer );
void FS_FreeFile( void *buffer );

