
#include "sekai.h"
#include "bsp.h"

// convert_nsco.c
void ConvertNscoToNscoET( bspFile_t *bsp );
void ConvertNscoETToNsco( bspFile_t *bsp );

int main( int argc, char **argv ) {
	bspFile_t *bsp;
	int saveLength;
	void *saveData;
	char *conversion, *inputFile, *formatName, *outputFile;
	bspFormat_t *outFormat;
	void (*convertFunc)( bspFile_t *bsp );

	if ( argc < 5 ) {
		Com_Printf( "bspsekai <conversion> <input-BSP> <format> <output-BSP>\n" );
		Com_Printf( "BSP sekai - v0.1\n" );
		Com_Printf( "Convert a BSP for use on a different engine\n" );
		Com_Printf( "BSP conversion can lose data, keep the original BSP!\n" );
		Com_Printf( "\n" );
		Com_Printf( "<conversion> specifies how surface and content flags are remapped.\n" );
		Com_Printf( "Conversion list:\n" );
		Com_Printf( "  none      - No conversion.\n" );
		Com_Printf( "  nsco2et   - Convert Navy SEALS: Covert Operation surface/content flags to ET values.\n" );
		Com_Printf( "  et2nsco   - Convert ET surface/content flags to Navy SEALS: Covert Operation values.\n" );
		Com_Printf( "\n" );
		Com_Printf( "The format of <input-BSP> is automatically determined from the file.\n" );
		Com_Printf( "Input BSP formats: (not all are fully supported)\n" );
		Com_Printf( "  Quake 3, RTCW, ET, EF, EF2, FAKK, Alice, Dark Salvation, MOHAA, Q3Test 1.06 or later, SoF2, JK2, JA\n" );
		Com_Printf( "\n" );
		Com_Printf( "<format> is used to determine output BSP format.\n" );
		Com_Printf( "BSP format list:\n" );
		Com_Printf( "  quake3    - Quake 3.\n" );
		//Com_Printf( "  q3test106 - Q3Test 1.06/1.07/1.08. Later Q3Test use 'quake3' format.\n" );
		Com_Printf( "  rtcw      - Return to Castle Wolfenstein.\n" );
		Com_Printf( "  et        - Wolfenstein: Enemy Territory.\n" );
		Com_Printf( "  darks     - Dark Salvation.\n" );
		//Com_Printf( "  fakk      - Heavy Metal: FAKK2.\n" );
		//Com_Printf( "  alice     - American McGee's Alice.\n" );
		//Com_Printf( "  rbsp      - Raven's RBSP format used by SoF2, Jedi Knight 2, and Jedi Academy.\n" );
		Com_Printf( "  ef        - Elite Force.\n" );
		//Com_Printf( "  ef2       - Elite Force 2.\n" );
		//Com_Printf( "  mohaa     - Medal of Honor Allied Assult.\n" );
		return 0;
	}

	conversion = argv[1];
	inputFile = argv[2];
	formatName = argv[3];
	outputFile = argv[4];

	if ( Q_stricmp( conversion, "none" ) == 0 ) {
		convertFunc = NULL;
	} else if ( Q_stricmp( conversion, "nsco2et" ) == 0 ) {
		convertFunc = ConvertNscoToNscoET;
	} else if ( Q_stricmp( conversion, "et2nsco" ) == 0 ) {
		convertFunc = ConvertNscoETToNsco;
	} else {
		Com_Printf( "Error: Unknown conversion '%s'.\n", conversion );
		return 1;
	}

	if ( Q_stricmp( formatName, "quake3" ) == 0 || Q_stricmp( formatName, "ef" ) == 0 ) {
		outFormat = &quake3BspFormat;
	} else if ( Q_stricmp( formatName, "rtcw" ) == 0 ) {
		outFormat = &wolfBspFormat;
	} else if ( Q_stricmp( formatName, "et" ) == 0 ) {
		// ZTM: TODO: This need to be a different format than RTCW so that there is a different save function; so that converting et maps to rtcw can convert foliage
		outFormat = &wolfBspFormat;
	} else if ( Q_stricmp( formatName, "darks" ) == 0 ) {
		outFormat = &darksBspFormat;
	} else if ( Q_stricmp( formatName, "fakk" ) == 0 ) {
		outFormat = &fakkBspFormat;
	} else if ( Q_stricmp( formatName, "alice" ) == 0 ) {
		outFormat = &aliceBspFormat;
	} else if ( Q_stricmp( formatName, "rbsp" ) == 0 ) { // SoF2, JK2, JA
		outFormat = &sof2BspFormat;
	} else if ( Q_stricmp( formatName, "ef2" ) == 0 ) {
		outFormat = &ef2BspFormat;
	} else if ( Q_stricmp( formatName, "mohaa" ) == 0 ) {
		outFormat = &mohaaBspFormat;
	} else if ( Q_stricmp( formatName, "q3test106" ) == 0 || Q_stricmp( formatName, "q3test107" ) == 0 || Q_stricmp( formatName, "q3test108" ) == 0 ) {
		outFormat = &q3Test106BspFormat;
	} else {
		Com_Printf( "Error: Unknown format '%s'.\n", formatName );
		return 1;
	}

	if ( Q_stricmp( inputFile, "-" ) == 0 || Q_stricmp( outputFile, "-" ) == 0 ) {
		Com_Printf( "Error: reading / writing to stdout is not supported.\n" );
		return 1;
	}

	// this will work, but might result in user overwritting original BSP without backup. so let's baby the user. >.>
	if ( Q_stricmp( inputFile, outputFile ) == 0 ) {
		Com_Printf( "Error: same input and output file (exiting to avoid data lose)\n" );
		return 1;
	}

	bsp = BSP_Load( inputFile );

	if ( !bsp ) {
		Com_Printf( "Error: Could not read file '%s'\n", inputFile );
		return 1;
	}

	Com_Printf( "Loaded BSP '%s' successfully.\n", inputFile );

	if ( outFormat->saveFunction ) {
		if ( convertFunc ) {
			convertFunc( bsp );
		}

		saveData = NULL;
		saveLength = outFormat->saveFunction( outFormat, outputFile, bsp, &saveData );

		if ( saveData && FS_WriteFile( outputFile, saveData, saveLength ) == saveLength ) {
			Com_Printf( "Saved BSP '%s' successfully.\n", outputFile );
		} else {
			Com_Printf( "Saving BSP '%s' failed.\n", outputFile );
		}

		if ( saveData ) {
			free( saveData );
		}
	} else {
		Com_Printf( "BSP format for '%s' does not support saving.\n", outFormat->gameName );
	}

	BSP_Free( bsp );

	return 0;
}

long FS_WriteFile( const char *filename, void *buf, long length ) {
	FILE *f;

	f = fopen( filename, "wb" );

	if ( !f ) {
		return 0;
	}

	if ( fwrite( buf, length, 1, f ) != 1 ) {
		fclose( f );
		return 0;
	}

	fclose( f );

	return length;
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
		fclose( f );
		free( buf );
		return 0;
	}

	fclose( f );

	*buffer = buf;
	return length;
}

void FS_FreeFile( void *buffer ) {
	if ( buffer ) {
		free( buffer );
	}
}
