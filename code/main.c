
#include "sekai.h"
#include "bsp.h"

int main( int argc, char **argv ) {
	bspFile_t *bsp;

	if ( argc < 2 ) {
		Com_Printf( "missing arguments\n" );
		return 0;
	}

	bsp = BSP_Load( argv[1] );

	if ( !bsp ) {
		Com_Printf( "Error: Could not read file '%s'\n", argv[1] );
		return 1;
	}

	Com_Printf( "BSP '%s' loaded successful!\n", bsp->name );

	BSP_Free( bsp );

	return 0;
}

long FS_ReadFile( const char *filename, void **buffer ) {
	FILE *f;
	long length;
	void *buf;

	*buffer = NULL;

	f = fopen( filename, "rb" );

	if ( !f ) {
		return 0;
	}

	fseek( f, 0, SEEK_END );
	length = ftell( f );
	fseek( f, 0, SEEK_SET );

	buf = malloc( length );

	if ( fread( buf, length, 1, f ) != 1 ) {
		free( buf );
		return 0;
	}

	*buffer = buf;
	return length;
}

void FS_FreeFile( void *buffer ) {
	if ( buffer ) {
		free( buffer );
	}
}
