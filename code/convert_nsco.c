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

#include "sekai.h"
#include "bsp.h"

// Navy seals contents not in Q3 (unused by the nsco source code)
#define NSCO_CONTENTS_SEWER          0x80 // conflicts with ET's MISSILECLIP and Q3's NOTTEAM1
#define NSCO_CONTENTS_HALLWAY        0x100 // Q3: NOTTEAM2
#define NSCO_CONTENTS_ARENA          0x200 // Q3: NOBOTCLIP
#define NSCO_CONTENTS_ROOM           0x400
#define NSCO_CONTENTS_ALLEY          0x800
#define NSCO_CONTENTS_PLAIN          0x1000
#define NSCO_CONTENTS_CITY           0x2000

#define NSCO_CONTENTS ( NSCO_CONTENTS_SEWER | NSCO_CONTENTS_HALLWAY | NSCO_CONTENTS_ARENA \
						| NSCO_CONTENTS_ROOM | NSCO_CONTENTS_ALLEY | NSCO_CONTENTS_PLAIN \
						| NSCO_CONTENTS_CITY )

// NSCO (Q3) surfaces types
#define NSCO_SURF_DUST				0x40000
#define NSCO_SURF_WOODSTEPS			0x100000
#define NSCO_SURF_DIRTSTEPS			0x200000
#define NSCO_SURF_SNOWSTEPS			0x400000
#define NSCO_SURF_SANDSTEPS			0x800000
#define NSCO_SURF_GLASS				0x1000000
#define NSCO_SURF_SOFTSTEPS			0x2000000

// ET contents types not in Q3
#define ET_CONTENTS_MISSILECLIP			0x00000080	// Q3: NOTTEAM1
#define ET_CONTENTS_DONOTENTER_LARGE	0x00400000	// Q3: BOTCLIP

#define ET_CONTENTS ( ET_CONTENTS_MISSILECLIP | ET_CONTENTS_DONOTENTER_LARGE )

// NSCO (ET) / ET surface types
#define ET_SURF_WOOD				0x00040000 // NSCO_SURF_WOODSTEPS
#define ET_SURF_GRASS				0x00080000 // NSCO_SURF_DIRTSTEPS
#define ET_SURF_GRAVEL				0x00100000 // NSCO_SURF_SANDSTEPS
#define ET_SURF_GLASS				0x00200000 // NSCO_SURF_GRASS
#define ET_SURF_SNOW				0x00400000 // NSCO_SURF_SNOWSTEPS
#define ET_SURF_ROOF				0x00800000
#define ET_SURF_RUBBLE				0x01000000 // NSCO_SURF_DUST
#define ET_SURF_CARPET				0x02000000 // NSCO_SURF_SOFTSTEPS

struct {
	int nscoFlag;
	int etFlag;
} nscoSurfaceFlags[] = {
	{ NSCO_SURF_DUST,		ET_SURF_RUBBLE },
	{ NSCO_SURF_WOODSTEPS,	ET_SURF_WOOD },
	{ NSCO_SURF_DIRTSTEPS,	ET_SURF_GRASS },
	{ NSCO_SURF_SNOWSTEPS,	ET_SURF_SNOW },
	{ NSCO_SURF_SANDSTEPS,	ET_SURF_GRAVEL },
	{ NSCO_SURF_GLASS,		ET_SURF_GLASS },
	{ NSCO_SURF_SOFTSTEPS,	ET_SURF_CARPET },
	{ 0,					ET_SURF_ROOF },
};

int numNscoSurfaceFlags = ARRAY_LEN( nscoSurfaceFlags );

// ZTM: TODO: strip out flares if they exist?
void ConvertNscoToNscoET( bspFile_t *bsp ) {
	int i, j;

	for ( i = 0; i < bsp->numShaders; i++ ) {
		bsp->shaders[i].contentFlags &= ~NSCO_CONTENTS;

		for ( j = 0; j < numNscoSurfaceFlags; j++ ) {
			if ( bsp->shaders[i].surfaceFlags & nscoSurfaceFlags[j].nscoFlag ) {
				bsp->shaders[i].surfaceFlags &= ~nscoSurfaceFlags[j].nscoFlag;
				bsp->shaders[i].surfaceFlags |= nscoSurfaceFlags[j].etFlag;
			}
		}
	}

	Com_Printf( "Modified NSCO Q3 surface and content flags for NSCO ET.\n" );
}

void ConvertNscoETToNsco( bspFile_t *bsp ) {
	int i, j;

	for ( i = 0; i < bsp->numShaders; i++ ) {
		bsp->shaders[i].contentFlags &= ~ET_CONTENTS;

		for ( j = 0; j < numNscoSurfaceFlags; j++ ) {
			if ( bsp->shaders[i].surfaceFlags & nscoSurfaceFlags[j].etFlag ) {
				bsp->shaders[i].surfaceFlags &= ~nscoSurfaceFlags[j].etFlag;
				bsp->shaders[i].surfaceFlags |= nscoSurfaceFlags[j].nscoFlag;
			}
		}
	}

	Com_Printf( "Modified NSCO ET surface and content flags for NSCO Q3.\n" );
}

