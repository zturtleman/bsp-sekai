#include "q_shared.h"
#include "qcommon.h"
#include "bsp.h"
#include <math.h>
#include "image.h"
#include "entity.h"
#include <ctype.h> // tolower
#include "strbuf.h"

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
		// little-endian "IBSP"

#define	BSPVERSION				41	//	planepolys lump (only adjusted so -planypolys BSP 
									//	switch can detect versions and update the header
									//	appropriately
#define BSPVERSION_COLORLUMP	40	//	color lump
#define BSPVERSION_TEXINFOOLD	39	//	color info in texinfo struct
#define	BSPVERSION_Q2			38	//	original Quake 2 format BSP


// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_MAP_MODELS		1024
#define	MAX_MAP_BRUSHES		8192
#define	MAX_MAP_ENTITIES	2048
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_TEXINFO		8192

#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
#define	MAX_MAP_LEAFS		65536
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
//	For development only!!  Change before release!
#define	MAX_MAP_LIGHTING	0x600000
//#define	MAX_MAP_LIGHTING	0x400000

//	For development only!!  Change before release!
#define	MAX_MAP_VISIBILITY	0x600000
//#define	MAX_MAP_VISIBILITY	0x400000
//#define	MAX_MAP_VISIBILITY	0x200000

#define MAX_MAP_SURFCOLORINFO 8192

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024

//=============================================================================

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			0x00000001		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			0x00000002		// translucent, but not watery
#define	CONTENTS_AUX			0x00000004
#define	CONTENTS_LAVA			0x00000008
#define	CONTENTS_SLIME			0x00000010
#define	CONTENTS_WATER			0x00000020
#define	CONTENTS_MIST			0x00000040
//#define	LAST_VISIBLE_CONTENTS	0x00000040
#define	CONTENTS_CLEAR			0x00000080
#define	CONTENTS_NOTSOLID		0x00000100
#define	CONTENTS_NOSHOOT		0x00000200
#define	CONTENTS_FOG			0x00000400
#define	CONTENTS_NITRO			0x00000800
#define	LAST_VISIBLE_CONTENTS	0x00000800

// remaining contents are non-visible, and don't eat brushes

#define	CONTENTS_AREAPORTAL		0x00008000

#define	CONTENTS_PLAYERCLIP		0x00010000
#define	CONTENTS_MONSTERCLIP	0x00020000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x00040000
#define	CONTENTS_CURRENT_90		0x00080000
#define	CONTENTS_CURRENT_180	0x00100000
#define	CONTENTS_CURRENT_270	0x00200000
#define	CONTENTS_CURRENT_UP		0x00400000
#define	CONTENTS_CURRENT_DOWN	0x00800000

#define	CONTENTS_ORIGIN			0x01000000	// removed before bsping an entity

#define	CONTENTS_MONSTER		0x02000000	// should never be on a brush, only in game
#define	CONTENTS_DEADMONSTER	0x04000000
#define	CONTENTS_DETAIL			0x08000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000

//	Nelno:	FL_BOT considers this solid
#define	CONTENTS_NPCCLIP		0x40000000

#define	SURF_LIGHT		0x00000001	// value will hold the light strength

#define SURF_FULLBRIGHT	0x00000002	// lightmaps are not generated for this surface

#define	SURF_SKY		0x00000004	// don't draw, but add to skybox
#define	SURF_WARP		0x00000008	// turbulent water warp
#define	SURF_TRANS33	0x00000010
#define	SURF_TRANS66	0x00000020
#define	SURF_FLOWING	0x00000040	// scroll towards angle
#define	SURF_NODRAW		0x00000080	// don't bother referencing the texture

#define	SURF_HINT		0x00000100	// make a primary bsp splitter
#define	SURF_SKIP		0x00000200	// completely ignore, allowing non-closed brushes

#define	SURF_WOOD 		0x00000400	//	The following surface flags affect game physics and sounds
#define	SURF_METAL		0x00000800
#define	SURF_STONE		0x00001000
#define	SURF_GLASS		0x00002000
#define	SURF_ICE		0x00004000
#define	SURF_SNOW		0x00008000

#define	SURF_MIRROR		0x00010000	//	a mirror surface.  Not implemented yet.
#define	SURF_TRANSTHING	0x00020000	//	???
#define	SURF_ALPHACHAN	0x00040000	//	Nelno: 32 bit alpha channel (GL) or mid-texture with transparency (software)
#define SURF_MIDTEXTURE	0x00080000

#define	SURF_PUDDLE		0x00100000	//	for puddle sounds when walking
#define	SURF_SURGE		0x00200000  //  for water that surges (slow surge)
#define	SURF_BIGSURGE	0x00400000  //  faster, bigger surge
#define	SURF_BULLETLIGHT	0x00800000	//	light comes out of bullet holes
#define	SURF_FOGPLANE	0x01000000	// SCG[3/30/99]: for volumetric fog
#define	SURF_SAND		0x02000000	//	for puddle sounds when walking

//=============================================================================

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	LUMP_EXTSURFINFO	19
#define	LUMP_PLANEPOLYS		20
#define	HEADER_LUMPS		21

typedef struct
{
	int			ident;
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		origin;		// for sounds or lights
	int			headnode;
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} realDmodel_t;


typedef struct
{
	float	point[3];
} realDvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

// planes (x&~1) and (x&~1)+1 are allways opposites

typedef struct
{
	vec3_t	normal;
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} realDplane_t;


// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

#define	Q3_CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	Q3_CONTENTS_LAVA			8
#define	Q3_CONTENTS_SLIME			16
#define	Q3_CONTENTS_WATER			32
#define	Q3_CONTENTS_FOG			64

#define Q3_CONTENTS_NOTTEAM1		0x0080
#define Q3_CONTENTS_NOTTEAM2		0x0100
#define Q3_CONTENTS_NOBOTCLIP		0x0200

#define	Q3_CONTENTS_AREAPORTAL		0x8000

#define	Q3_CONTENTS_PLAYERCLIP		0x10000
#define	Q3_CONTENTS_MONSTERCLIP	0x20000
//bot specific contents types
#define	Q3_CONTENTS_TELEPORTER		0x40000
#define	Q3_CONTENTS_JUMPPAD		0x80000
#define Q3_CONTENTS_CLUSTERPORTAL	0x100000
#define Q3_CONTENTS_DONOTENTER		0x200000
#define Q3_CONTENTS_BOTCLIP		0x400000
#define Q3_CONTENTS_MOVER			0x800000

#define	Q3_CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	Q3_CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	Q3_CONTENTS_CORPSE			0x4000000
#define	Q3_CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	Q3_CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	Q3_CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	Q3_CONTENTS_TRIGGER		0x40000000
#define	Q3_CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

#define	Q3_SURF_NODAMAGE			0x1		// never give falling damage
#define	Q3_SURF_SLICK				0x2		// effects game physics
#define	Q3_SURF_SKY				0x4		// lighting from environment map
#define	Q3_SURF_LADDER				0x8
#define	Q3_SURF_NOIMPACT			0x10	// don't make missile explosions
#define	Q3_SURF_NOMARKS			0x20	// don't leave missile marks
#define	Q3_SURF_FLESH				0x40	// make flesh sounds and effects
#define	Q3_SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	Q3_SURF_HINT				0x100	// make a primary bsp splitter
#define	Q3_SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	Q3_SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	Q3_SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	Q3_SURF_METALSTEPS			0x1000	// clanking footsteps
#define	Q3_SURF_NOSTEPS			0x2000	// no footstep sounds
#define	Q3_SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define	Q3_SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	Q3_SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	Q3_SURF_NODLIGHT			0x20000	// don't dlight even if solid (solid lava, skies)
#define Q3_SURF_DUST				0x40000 // leave a dust trail when walking on this surface

#define JKA_SURF_NONE				(0x00000000u)
#define	JKA_SURF_SKY				(0x00002000u)	// lighting from environment map
#define	JKA_SURF_SLICK				(0x00004000u)	// affects game physics
#define	JKA_SURF_METALSTEPS			(0x00008000u)	// CHC needs this since we use same tools (though this flag is temp?)
#define JKA_SURF_FORCEFIELD			(0x00010000u)	// CHC ""			(but not temp)
#define	JKA_SURF_NODAMAGE			(0x00040000u)	// never give falling damage
#define	JKA_SURF_NOIMPACT			(0x00080000u)	// don't make missile explosions
#define	JKA_SURF_NOMARKS			(0x00100000u)	// don't leave missile marks
#define	JKA_SURF_NODRAW				(0x00200000u)	// don't generate a drawsurface at all
#define	JKA_SURF_NOSTEPS			(0x00400000u)	// no footstep sounds
#define	JKA_SURF_NODLIGHT			(0x00800000u)	// don't dlight even if solid (solid lava, skies)
#define	JKA_SURF_NOMISCENTS			(0x01000000u)	// no client models allowed on this surface
#define	JKA_SURF_FORCESIGHT			(0x02000000u)	// not visible without Force Sight

#define JKA_CONTENTS_NONE			(0x00000000u)
#define	JKA_CONTENTS_SOLID			(0x00000001u)	// Default setting. An eye is never valid in a solid
#define	JKA_CONTENTS_LAVA			(0x00000002u)	//
#define	JKA_CONTENTS_WATER			(0x00000004u)	//
#define	JKA_CONTENTS_FOG			(0x00000008u)	//
#define	JKA_CONTENTS_PLAYERCLIP		(0x00000010u)	// Player physically blocked
#define	JKA_CONTENTS_MONSTERCLIP	(0x00000020u)	// NPCs cannot physically pass through
#define JKA_CONTENTS_BOTCLIP		(0x00000040u)	// do not enter - NPCs try not to enter these
#define JKA_CONTENTS_SHOTCLIP		(0x00000080u)	// shots physically blocked
#define	JKA_CONTENTS_BODY			(0x00000100u)	// should never be on a brush, only in game
#define	JKA_CONTENTS_CORPSE			(0x00000200u)	// should never be on a brush, only in game
#define	JKA_CONTENTS_TRIGGER		(0x00000400u)	//
#define	JKA_CONTENTS_NODROP			(0x00000800u)	// don't leave bodies or items (death fog, lava)
#define JKA_CONTENTS_TERRAIN		(0x00001000u)	// volume contains terrain data
#define JKA_CONTENTS_LADDER			(0x00002000u)	//
#define JKA_CONTENTS_ABSEIL			(0x00004000u)	// used like ladder to define where an NPC can abseil
#define JKA_CONTENTS_OPAQUE			(0x00008000u)	// defaults to on, when off, solid can be seen through
#define JKA_CONTENTS_OUTSIDE		(0x00010000u)	// volume is considered to be in the outside (i.e. not indoors)
#define JKA_CONTENTS_SLIME			(0x00020000u)	// CHC needs this since we use same tools
#define JKA_CONTENTS_LIGHTSABER		(0x00040000u)	// ""
#define JKA_CONTENTS_TELEPORTER		(0x00080000u)	// ""
#define JKA_CONTENTS_ITEM			(0x00100000u)	// ""
#define JKA_CONTENTS_UNUSED00200000	(0x00200000u)	//
#define JKA_CONTENTS_UNUSED00400000	(0x00400000u)	//
#define JKA_CONTENTS_UNUSED00800000	(0x00800000u)	//
#define JKA_CONTENTS_UNUSED01000000	(0x01000000u)	//
#define JKA_CONTENTS_UNUSED02000000	(0x02000000u)	//
#define JKA_CONTENTS_UNUSED04000000	(0x04000000u)	//
#define	JKA_CONTENTS_DETAIL			(0x08000000u)	// brushes not used for the bsp
#define	JKA_CONTENTS_INSIDE			(0x10000000u)	// volume is considered to be inside (i.e. indoors)
#define JKA_CONTENTS_UNUSED20000000	(0x20000000u)	//
#define JKA_CONTENTS_UNUSED40000000	(0x40000000u)	//
#define	JKA_CONTENTS_TRANSLUCENT	(0x80000000u)	// don't consume surface fragments inside
#define JKA_CONTENTS_ALL			(0xFFFFFFFFu)

typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for frustom culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} realDnode_t;

typedef struct
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/*.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} realDtexinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short	v[2];		// vertex numbers
} realDedge_t;

#define	DK_MAXLIGHTMAPS	4
#if DK_MAXLIGHTMAPS == MAXLIGHTMAPS
#else
#error "DK_MAXLIGHTMAPS == MAXLIGHTMAPS failed"
#endif
typedef struct
{
	unsigned short	planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;	
	short		texinfo;

// lighting info
	byte		styles[DK_MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} realDface_t;

typedef struct
{
	int				contents;			// OR of all brushes (not needed?)

	short			cluster;
	short			area;

	short			mins[3];			// for frustum culling
	short			maxs[3];

	unsigned short	firstleafface;
	unsigned short	numleaffaces;

	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;

	int				brushnum;
} realDleaf_t;

typedef struct
{
	unsigned short	planenum;		// facing out of the leaf
	short	texinfo;
} realDbrushside_t;

typedef struct
{
	int			firstside;
	int			numsides;
	int			contents;
} realDbrush_t;

#define	ANGLE_UP	-1
#define	ANGLE_DOWN	-2


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	DVIS_PVS	0
#define	DVIS_PHS	1
typedef struct
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} realDvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	int		portalnum;
	int		otherarea;
} realDareaportal_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
} realDarea_t;

typedef struct
{
	vec3_t	color;
} extsurfinfo_t;

/****************************************************
*/

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	vec3_t s, t;
	float s_offset, t_offset;
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	image_rgba_t		*image;
	byte	color[3];
	int value;
	int					duplicate;
	struct mtexinfo_s	*hash_next;
	qboolean			skip; // this should not be rendered at all (e.g. clip)
	int					shaderNum;
} mtexinfo_t;

typedef struct glpoly_s
{
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;
	int		numverts;
	int		flags;			// for SURF_UNDERWATER (not needed anymore?)
	drawVert_t	*verts;	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	dplane_t	*plane;
	int			side;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates
	int			dlight_s, dlight_t; // gl lightmap coordinates for dynamic lightmaps

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;
	struct  msurface_s	*lightmapchain;

	mtexinfo_t	*texinfo;
	int shaderNum;
	int fogNum;
	
// lighting info
	int			dlightframe;
	int			dlightbits;

	int			lightmaptexturenum;
	byte		styles[DK_MAXLIGHTMAPS];
	float		cached_light[DK_MAXLIGHTMAPS];	// values currently used in lightmap
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

#define LIGHTMAP_SIZE 128

typedef struct mnode_s
{
// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		mins[3];		// for bounding box culling
	float		maxs[3];

	struct mnode_s	*parent;

// node specific
	dplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		mins[3];		// for bounding box culling
	float		maxs[3];

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	int			firstmarksurface;
	int			nummarksurfaces;

	int			firstleafbrush;
	int			numleafbrushes;
} mleaf_t;

typedef struct
{
	dplane_t	*plane;
	msurface_t	*surface;
} mbrushside_t;


typedef struct mbrush_s
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} mbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} marea_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} mmodel_t;

#define VIEWFOGS 0

typedef struct
{
	char		shader[MAX_QPATH];
	int			brushNum;
	int			visibleSide;	// the brush side that ray tests need to clip against (-1 == none)

	mtexinfo_t	*texinfo;
} mfog_t;

/****************************************************
*/

static int GetLumpElements( dheader_t *header, int lump, int size ) {
	/* check for odd size */
	if ( header->lumps[ lump ].filelen % size ) {
		Com_Printf( "GetLumpElements: odd lump size (%d) in lump %d\n", header->lumps[ lump ].filelen, lump );
		return 0;
	}

	/* return element count */
	return header->lumps[ lump ].filelen / size;
}

static void CopyLump( dheader_t *header, int lump, const void *src, void *dest, int size, qboolean swap ) {
	int length;

	length = GetLumpElements( header, lump, size ) * size;

	/* handle erroneous cases */
	if ( length <= 0 ) {
		return;
	}

	if ( swap ) {
		BSP_SwapBlock( dest, (int *)((byte*) src + header->lumps[lump].fileofs), length );
	} else {
		Com_Memcpy( dest, (byte*) src + header->lumps[lump].fileofs, length );
	}
}

static void *GetLump( dheader_t *header, const void *src, int lump ) {
	return (void*)( (byte*) src + header->lumps[ lump ].fileofs );
}

/****************************************************
*/

float DotProduct(vec3_t a, vec3_t b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

glpoly_t *AllocGLPoly( int numverts ){
	glpoly_t	*poly;
	int			allocSize;
	byte*		ptr;

	if ( numverts < 3 )
	{
		Com_Error(ERR_DROP, "something is wrong here");
	}

	// allocate once.. assign pointers..
	allocSize = sizeof(glpoly_t) + numverts * sizeof(drawVert_t);

	ptr = (byte*)malloc( allocSize );

	memset( ptr, 0, sizeof( glpoly_t ) );

	poly = (glpoly_t *)	ptr;
	poly->verts = (drawVert_t*)(ptr + sizeof(glpoly_t));

	return poly;
}

void CalcSurfaceExtents( float *vertexes, medge_t *edges, int *surfEdges, msurface_t *s )
{
	float	mins[2], maxs[2], val;
	int		i, e;
	float	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for( i = 0; i < s->numedges; i++ )
	{
		e = surfEdges[s->firstedge+i];
		if( e >= 0 )
		{
			v = &vertexes[edges[e].v[0] * 3];
		}
		else
		{
			v = &vertexes[edges[-e].v[1] * 3];
		}
		
		val = v[0] * tex->s[0] + 
			v[1] * tex->s[1] + 
			v[2] * tex->s[2] + 
			tex->s_offset;

		if(val < mins[0] )
		{
			mins[0] = val;
		}
		if( val > maxs[0] )
		{
			maxs[0] = val;
		}

		val = v[0] * tex->t[0] + 
			v[1] * tex->t[1] + 
			v[2] * tex->t[2] + 
			tex->t_offset;

		if(val < mins[1])
		{
			mins[1] = val;
		}
		if( val > maxs[1] )
		{
			maxs[1] = val;
		}
	}

	for( i = 0; i < 2; i++ )
	{	
		bmins[i] = floor( mins[i]/16 );
		bmaxs[i] = ceil( maxs[i]/16 );

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = ( bmaxs[i] - bmins[i] ) * 16;
		if( s->extents[i] < 16 )
		{
			s->extents[i] = 16;	// take at least one cache block
		}
		if ( !(tex->flags & (SURF_WARP|SURF_SKY)) && s->extents[i] > 256)
			Com_Error (ERR_DROP,"Bad surface extents");
	}
}

void GL_BuildPolygonFromSurface( float *vertexes, medge_t *pedges, int *surfedges, msurface_t *pSurf )
{
	int			i, lindex, lnumverts;
	medge_t		*r_pedge;
//	int			vertpage;
	vec3_t		vec;
	float		s, t;
	glpoly_t	*poly;
	vec3_t		total;

// reconstruct the polygon
	lnumverts = pSurf->numedges;
//	vertpage = 0;

	total[0] = total[1] = total[2] = 0.0f;
	//
	// draw texture
	//

	poly = AllocGLPoly( lnumverts );

	poly->next = pSurf->polys;
//	poly->flags = pSurf->flags;
	pSurf->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = surfedges[pSurf->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec[0] = vertexes[r_pedge->v[0] * 3 + 0];
			vec[1] = vertexes[r_pedge->v[0] * 3 + 1];
			vec[2] = vertexes[r_pedge->v[0] * 3 + 2];
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec[0] = vertexes[r_pedge->v[1] * 3 + 0];
			vec[1] = vertexes[r_pedge->v[1] * 3 + 1];
			vec[2] = vertexes[r_pedge->v[1] * 3 + 2];
		}

		// SCG[6/3/99]: Find the s coord for this vert
		s = DotProduct( vec, pSurf->texinfo->s ) + pSurf->texinfo->s_offset;

		// SCG[6/3/99]: normalize it
		s /= pSurf->texinfo->image->width;

		// SCG[6/3/99]: Find the t coord for this vert
		t = DotProduct( vec, pSurf->texinfo->t ) + pSurf->texinfo->t_offset;

		// SCG[6/3/99]: normalize it
		t /= pSurf->texinfo->image->height;

		total[0] = total[0] + vec[0];
		total[1] = total[1] + vec[1];
		total[2] = total[2] + vec[2];
		poly->verts[i].xyz[0] = vec[0];
		poly->verts[i].xyz[1] = vec[1];
		poly->verts[i].xyz[2] = vec[2];
		poly->verts[i].st[0] = s;
		poly->verts[i].st[1] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct( vec, pSurf->texinfo->s ) + pSurf->texinfo->s_offset;
		s -= pSurf->texturemins[0];
		s += pSurf->light_s * 16;
		s += 8;
		s /= 2048;	// SCG[6/9/99]: 2048 = BLOCK_WIDTH * 16
		//s /= (pSurf->extents[0] - pSurf->texturemins[0]);
	
		t = DotProduct( vec, pSurf->texinfo->t ) + pSurf->texinfo->t_offset;
		t -= pSurf->texturemins[1];
		t += pSurf->light_t * 16;
		t += 8;
		t /= 2048;	// SCG[6/9/99]: 2048 = BLOCK_WIDTH * 16
		//t /= (pSurf->extents[1] - pSurf->texturemins[1]);

		poly->verts[i].lightmap[0][0] = s;
		poly->verts[i].lightmap[0][1] = t;
		
		poly->verts[i].normal[0] = pSurf->plane->normal[0];
		poly->verts[i].normal[1] = pSurf->plane->normal[1];
		poly->verts[i].normal[2] = pSurf->plane->normal[2];
	}
}

static void DecompressVis (byte *visibility, int numvisibility, realDvis_t *vis, int clusterBytes, byte * out_real) {
	int c;
	byte *out_p;
	int row;
	int i;

	if (!numvisibility) {	// no vis info, so make all visible
		row = vis->numclusters * clusterBytes;
		while (row) {
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	for (i = 0; i < vis->numclusters; i++)
	{
		byte *out = out_real + i * clusterBytes;
		byte *in = visibility + vis->bitofs[i][DVIS_PVS];

		row = clusterBytes;
		out_p = out;

		do {
			if (*in) {
				*out_p++ = *in++;
				continue;
			}

			c = in[1];
			in += 2;
			if ((out_p - out) + c > row) {
				c = row - (out_p - out);
				Com_Error (ERR_DROP, "Vis decompression overrun\n");
			}
			while (c) {
				*out_p++ = 0;
				c--;
			}
		} while (out_p - out < row);
	}
}

static void ConvertVisibility( bspFile_t *bsp, const void *data, dheader_t *header, realDvis_t *visibility, int visibilityLength ) {
	int		i;
	byte	*in;

	if ( !visibilityLength ) {
		return;
	}

	in = GetLump( header, data, LUMP_VISIBILITY );

	memcpy (visibility, in, visibilityLength);
	visibility->numclusters = LittleLong( visibility->numclusters );
	for (i=0 ; i<visibility->numclusters ; i++)
	{
		visibility->bitofs[i][0] = LittleLong (visibility->bitofs[i][0]);
		visibility->bitofs[i][1] = LittleLong (visibility->bitofs[i][1]);
	}

	bsp->clusterBytes = ( visibility->numclusters + 7 ) >> 3;
	bsp->visibilityLength = visibility->numclusters * bsp->clusterBytes;
	bsp->visibility = malloc( bsp->visibilityLength );
	
	bsp->numClusters = visibility->numclusters;
	DecompressVis( in, visibilityLength, visibility, bsp->clusterBytes, bsp->visibility );
}

/****************************************************
*/

#define	LIGHTMAP_SIZE		128
#define LM_BLOCKLIGHTS_SIZE LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3

typedef struct mlightmap_s {
	byte buffer[LM_BLOCKLIGHTS_SIZE];
	struct mlightmap_s *next;
} mlightmap_t;

typedef struct {
	int		texnum;

	mlightmap_t *lightmaps;

	byte lightmap_buffer[LM_BLOCKLIGHTS_SIZE];
	int allocated[LIGHTMAP_SIZE];
} gllightmapstate_t;

void GL_BeginBuildingLightmaps(gllightmapstate_t *lms)
{
	lms->texnum = 0;
	lms->lightmaps = NULL;
	memset (lms->allocated, 0, sizeof(lms->allocated));
}

/*
===============
R_BuildLightMap

Combine & scale multiple lightmaps into the floating-point format in s_blocklights,
then normalize into GL format in gl_lms.lightmap_buffer.
===============
*/

static float s_blocklights[LM_BLOCKLIGHTS_SIZE];		// intermediate RGB buffer for combining multiple lightmaps

void R_BuildLightMap (gllightmapstate_t *lms, msurface_t *surf, int stride) {
	int smax, tmax;
	int r, g, b, cmax;
	int i, j, k, size;
	vec3_t scale;
	float *bl;
	byte *lm, *dest;
	int numlightmaps;

	smax = (surf->extents[0] / 16) + 1;
	tmax = (surf->extents[1] / 16) + 1;

	size = smax * tmax;
	stride -= smax * 3;

	if (size > ((sizeof(float) * LM_BLOCKLIGHTS_SIZE) / 16))
		Com_Error(ERR_DROP, "R_BuildLightMap(): bad lms->blocklights size.");

	if ( surf->samples ) {
		for ( k = 0; k < DK_MAXLIGHTMAPS && surf->styles[k] != 255; k++ )
			;
		numlightmaps = k;
		if ( numlightmaps > 1) {
			// woah
			// so samples is actually numlightmaps * size array
			// and we need to upload all of those, assigning to each their own coordinates in a possibly the same atlas
		}
	}

	k = 0;
	if (surf->samples)
	{
		lm = surf->samples;
		bl = s_blocklights;
		for (i = 0; i < size; i++, bl += 3, lm += 3)
		{
			bl[0] = lm[0];
			bl[1] = lm[1];
			bl[2] = lm[2];
		}
	}
	else
	{
		// set to full bright if no light data
		for (i = 0; i < size * 3; i++)
			s_blocklights[i] = 255.f;
	}

	//
	// put into texture format
	//

	bl = s_blocklights;

	dest = lms->lightmap_buffer;
	dest += (surf->light_t * LIGHTMAP_SIZE + surf->light_s) * 3;

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++, bl += 3, dest += 3) {
			r = (long) (bl[0]);
			g = (long) (bl[1]);
			b = (long) (bl[2]);

			// catch negative lights
			r = r > 0 ? r : 0;
			g = g > 0 ? g : 0;
			b = b > 0 ? b : 0;

			// determine the brightest of the three color components
			cmax = g > b ? g : b;
			cmax = r > cmax ? r : cmax;

			// rescale all the color components if the intensity of the greatest channel exceeds 1.0
			if (cmax > 255) {
				float t = 255.f / cmax;

				r *= t;
				g *= t;
				b *= t;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
		}
	}
}

static void LM_InitBlock (gllightmapstate_t *lms)
{
	memset( lms->allocated, 0, sizeof( lms->allocated ) );
}

static void LM_UploadBlock (gllightmapstate_t *lms)
{
	mlightmap_t *map = malloc( sizeof(*map) );
	memcpy(map->buffer, lms->lightmap_buffer, LM_BLOCKLIGHTS_SIZE);
	if (lms->lightmaps == NULL)
	{
		map->next = NULL;
		lms->lightmaps = map;
	}
	else
	{
		map->next = lms->lightmaps;
		lms->lightmaps = map;
	}
	lms->texnum++;
}

// returns a texture number and the position inside it
static qboolean LM_AllocBlock (gllightmapstate_t *lms, int w, int h, int *x, int *y) {
	int i, j;
	int best, best2;

	best = LIGHTMAP_SIZE;

	for (i = 0; i < LIGHTMAP_SIZE - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (lms->allocated[i + j] >= best)
				break;
			if (lms->allocated[i + j] > best2)
				best2 = lms->allocated[i + j];
		}

		if (j == w) {			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_SIZE)
		return qfalse;

	for (i = 0; i < w; i++)
		lms->allocated[*x + i] = best + h;

	return qtrue;
}

void GL_CreateSurfaceLightmap (gllightmapstate_t *lms, msurface_t * surf)
{
	int smax, tmax;

	smax = (surf->extents[0] / 16) + 1;
	tmax = (surf->extents[1] / 16) + 1;

	if (!LM_AllocBlock (lms, smax, tmax, &surf->light_s, &surf->light_t))
	{
		LM_UploadBlock(lms);
		LM_InitBlock(lms);
		if (!LM_AllocBlock(lms, smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error (ERR_DROP, "GL_CreateSurfaceLightmap(): consecutive calls to LM_AllocBlock(%d, %d) failed.\n", smax, tmax);
	}
	
	surf->lightmaptexturenum = lms->texnum;

	R_BuildLightMap (lms, surf, LIGHTMAP_SIZE * 3);
}

/* ******************************************** */

// sky dome is a rectangular grid with this many verts to the edge
#define SKY_BOX_SIZE		8192.0f
#define NUM_SKYBOX_LAYERS	2

typedef struct {
	int		used;
	float	scale[2];
	float	scroll[2];
	float	alphaGenConst;
} skyboxLayer_t;

typedef struct {
	char			*sky;
	char			*cloudname;

	float			cloudxdir, cloudydir;
	float			cloud1tile, cloud1speed;
	float			cloud2tile, cloud2speed;
	float			cloud2alpha;

	float			curSkyScale;
	float			texScale;

	float			height;

	skyboxLayer_t	layers[NUM_SKYBOX_LAYERS];
} skybox_t;

static void SkyBoxCalcTexCoords(skybox_t *box, float skyScale) {
    box->curSkyScale = skyScale;
    box->texScale = box->curSkyScale/(2*SKY_BOX_SIZE);
}

static void SkyBoxSetupLayer(skybox_t *box, int layerNum, float texscale, float scrollval, float a)
{
    int i, j, pi, pj;
    float du, dv;
	skyboxLayer_t *layer;

    if (box->cloud1tile!=box->curSkyScale)
        SkyBoxCalcTexCoords(box, box->cloud1tile);
    
    du = box->cloudxdir*scrollval*box->cloud1tile;
    dv = box->cloudydir*scrollval*box->cloud1tile;

	layer = &box->layers[layerNum];
	layer->scale[0] = texscale;
	layer->scale[1] = texscale;
	layer->scroll[0] = du / 256;
	layer->scroll[1] = dv / 256;
	layer->alphaGenConst = a;
	layer->used = 1;
}

static void SkyBoxPrint( const char *mapfilename, skybox_t *box, strbuf_t *strbuf ) {
	int		i;
	int		len;
	char	line[2048];
	skyboxLayer_t *layer;

	sprintf( line, "textures/%s/sky\n", mapfilename );
	AppendToBuffer( strbuf, line );

	sprintf( line, "{\n" "\tqer_editorimage env/32bit/%s_up.tga\n", box->sky );
	AppendToBuffer( strbuf, line );
	AppendToBuffer( strbuf, "\tsurfaceparm noimpact\n" "\tsurfaceparm nolightmap\n" "\tsurfaceparm sky\n");
	sprintf( line, "\tskyparms - %f env/32bit/%s\n", box->height, box->sky );
	AppendToBuffer( strbuf, line );

	layer = box->layers;
	for ( i = 0; i < NUM_SKYBOX_LAYERS ; i++, layer++ ) {
		if ( !layer->used )
			continue;

		AppendToBuffer( strbuf, "\t\t{\n" );
		sprintf( line, "\t\t\tmap env/32bit/%s.tga\n", box->cloudname );
		AppendToBuffer( strbuf, line );

		if ( layer->alphaGenConst != 1) {
			sprintf( line, "\t\t\talphaGen const %f\n", layer->alphaGenConst );
			AppendToBuffer( strbuf, line );
			AppendToBuffer( strbuf, "\t\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n" );
		}
		//Com_Printf( "\t\t\trgbGen const ( %f %f %f )\n", layer->rgbGenConst[0], layer->rgbGenConst[1], layer->rgbGenConst[2] );

		sprintf( line, "\t\t\ttcmod scale %f %f\n", layer->scale[0], layer->scale[1] );
		AppendToBuffer( strbuf, line );
		sprintf( line, "\t\t\ttcmod scroll %f %f\n", layer->scroll[0], layer->scroll[1] );
		AppendToBuffer( strbuf, line );

		AppendToBuffer( strbuf, "\t\t}\n" );
	}

	AppendToBuffer( strbuf, "}\n" );
}

static void SkyBoxFree( skybox_t *box ) {
	if ( box->sky )
		free( box->sky );
	if ( box->cloudname )
		free( box->cloudname );
}

static void SkyboxFromEntity( skybox_t *box, entity_t *ent ) {
	const char *sky;
	const char *cloudname;

	memset( box, 0, sizeof( *box ) );

	sky = ValueForKey( ent, "sky" );
	cloudname = ValueForKey( ent, "cloudname" );

	if ( !*sky )
		return;

	box->sky = copystring( sky );

	box->cloudxdir = FloatForKey( ent, "cloudxdir" );
	box->cloudydir = FloatForKey( ent, "cloudydir" );
	box->cloud1tile = FloatForKey( ent, "cloud1tile" );
	box->cloud1speed = FloatForKey( ent, "cloud1speed" );
	box->cloud2tile = FloatForKey( ent, "cloud2tile" );
	box->cloud2speed = FloatForKey( ent, "cloud2speed" );
	box->cloud2alpha = FloatForKey( ent, "cloud2alpha" );

	float timeval = 1.0f/256.0f;
	box->height = SKY_BOX_SIZE * 0.5;

	if ( !*cloudname )
		return;

	box->cloudname = copystring( cloudname );

	SkyBoxSetupLayer( box, 0, 1, timeval * box->cloud1tile / box->cloud1speed, 1);
	if ( box->cloud2alpha > 0 ) {
		float alpha = (box->cloud2alpha < 1 ? box->cloud2alpha : 1);
		SkyBoxSetupLayer( box, 1, box->cloud2tile/box->cloud1tile, timeval * box->cloud2tile/box->cloud2speed, alpha );
	}
}

static void dkent_trigger_hurt( entity_t *ent )
{
	char	buf[32];
	int		spawnFlags, q3SpawnFlags;

// "spawnflags" "4"
// DK flags:
//#define		ALLOW_TOGGLE				0x01
//#define		START_DISABLED				0x02
//#define		FLOWTHRU_DMG				0x04
// Q3 flags?
// START_OFF 2
// SILENT 4
// SLOW 16
// NO_PROTECTION 8
// no customizable sound!
	spawnFlags = IntegerForKey( ent, "spawnflags" );
	q3SpawnFlags = 0;

	if (spawnFlags & 0x01/*ALLOW_TOGGLE*/)
		q3SpawnFlags |= 1;
	if (spawnFlags & 0x02/*START_DISABLED*/)
		q3SpawnFlags |= 2/*START_OFF*/;

	sprintf( buf, "%d", q3SpawnFlags );

	SetKeyValue( ent, "spawnflags", buf );
}

static void dkent_target_speaker( entity_t *ent )
{
	const char *sound1;

	// add "noise" key
	// for JK: map sound1..sound6 to soundGroup:
	//
	// "sounds" va() min max, so if your sound string is borgtalk%d.wav, and you set a "sounds" value of 4, it will randomly play borgtalk1.wav - borgtalk4.wav when triggered
	// to use this, you must store the wav name in "soundGroup", NOT "noise"
	// MISSING: volume support in quake3

	// fixup paths for all sounds
	AppendKeyValueMatching( ent, "sound", "sounds\\" );

	// set the noise key for compat with quake3 and jka
	sound1 = ValueForKey( ent, "sound1" );
	if ( !*sound1 )
	{
		sound1 = ValueForKey( ent, "sound" );
	}
	SetKeyValue( ent, "noise", sound1 );

	sound1 = ValueForKey( ent, "noise" );
	if ( !*sound1 ) {
		// remove useless speaker
		FreeEntity( ent );
	}
}

static void dkent_sound_ambient( entity_t *ent )
{
	// classname: target_speaker
	SetKeyValue( ent, "classname", "target_speaker" );
	SetKeyValue( ent, "spawnflags", "1" ); // looped

	dkent_target_speaker( ent );
}

typedef enum
{
	SOLID_NOT,			// no interaction with other objects
	SOLID_BBOX,			// touch on edge
} solid_t;

typedef enum
{
	MOVETYPE_NONE,			// never moves
	MOVETYPE_TOSS,			// gravity
	MOVETYPE_BOUNCE,
	MOVETYPE_FLOAT,			//	acts like MOVETYPE_BOUNCE until it hits a liquid, then mass/density takes over
} movetype_t;

#define	MAX_DECO_ANIM_SEQS	5		// maximum number of deco animation sequences


#define		FRAME_LOOP			0x0001		// loop from startFrame to endFrame
#define		FRAME_ONCE			0x0002		// go from startFrame to endFrame then stop

typedef struct {
	int		startFrame;
	int		endFrame;
	int		frameFlags;
} decoAnimSeq_t;

typedef struct decoInfo_s {
	char				name[MAX_QPATH];

	char				path[MAX_QPATH];
	movetype_t			moveType;
	solid_t				solidType;
	float				mass;
	qboolean			exploding;
	float				hitPoints;
	float				mins[3];
	float				maxs[3];
	int					gibType;
	int					numAnimSequences;
	decoAnimSeq_t		animSequences[MAX_DECO_ANIM_SEQS];

    struct decoInfo_s	*next;
} decoInfo_t;


//strips leading and trailing spaces from a string.
static void CsvStripSpaces(char *str) {
    if (str == NULL) return;

    //the src and dest positions we read/write to.
    int dest = 0;
    int src = 0;
    
    //the length of the string without the spaces.
    int len = 0;

    //skip past any leading spaces.
    while (str[src] == ' ') {
        src++;
    }

    //copy the string 
    while (str[src] != '\0') {
        //copy the character.
        str[dest] = str[src];

        //increment src and dest.
        dest++;
        src++;

        //increment the length
        len++;
    }

    //null terminate the string.
    str[len] = '\0';

    //remove any spaces at the end of the string.
    while (str[len - 1] == ' ') {
        //get rid of the space.
        str[len - 1] = '\0';
        //decrement the length.
        len--;
    }
}

// DK deco spawnflags
#define		DK_DECO_FLAGS_EXPLODE		0x0001
#define		DK_DECO_FLAGS_NO_BREAK		0x0002
#define		DK_DECO_FLAGS_PUSHABLE		0x0004
#define		DK_DECO_FLAGS_WOOD     		0x0008
#define		DK_DECO_FLAGS_METAL    		0x0010
#define		DK_DECO_FLAGS_GLASS    		0x0020
#define		DK_DECO_FLAGS_GIB    		0x0040
#define		DK_DECO_FLAGS_ROTATE			0x0080
#define     DK_DECO_FLAGS_TRANSLUCENT    0x0100      // 1.19 dsn

#define NUM_DECO_FIELDS 22

#define MAX_CSV_FIELDS 32
#define MAX_CSV_LINE_LENGTH 512

void ParseCsvLine( const char *line, int length, char *fields, int numFields ) {
	int		cur_field;
	char	*field_ptr;

	if ( length >= MAX_CSV_LINE_LENGTH ) {
		Com_Error( ERR_DROP, "CSV line too long" );
	}

	cur_field = 0;
	field_ptr = &fields[cur_field * MAX_CSV_LINE_LENGTH];
	while ( length ) {
		char c = *line++;
		length--;

		if (c == ',') {
			if (cur_field >= numFields) {
				Com_Error( ERR_DROP, "too many fields in a csv" );
			}
			*field_ptr++ = 0;
			cur_field++;
			field_ptr = &fields[cur_field * MAX_CSV_LINE_LENGTH];
			continue;
		} else if (c == '\n' || c == '\r') {
			*field_ptr++ = 0;
			break;
		}

		*field_ptr++ = c;
	}

	for (int i = 0; i < numFields; i++) {
		CsvStripSpaces(&fields[i * MAX_CSV_LINE_LENGTH]);
	}
}

int ParseCsvFromMemory( const char *buffer, int bufferSize, int numFields, void *env, void (*cb)(void *, char *) )
{
	char	fields[MAX_CSV_FIELDS * MAX_CSV_LINE_LENGTH];
	int		line_pos;
	int		in_comment;
	int		cur_field;
	int		numDecos = 0;
	int		exploding;
	int		i;
	const char	*line_start;
	const char	*end;

	if (numFields > MAX_CSV_FIELDS) {
		Com_Error( ERR_DROP, "Please increase MAX_CSV_FIELDS\n" );
	}

	line_start = buffer;
	end = buffer + bufferSize;
	while ( buffer < end && *buffer ) {
		char c = *buffer++;

		if (c == '\n' || c == '\r') {
			if ( buffer > end ) {
				Com_Error( ERR_DROP, "csv file line is unterminated" );
			}

			// end of line
			if (buffer - line_start > MAX_CSV_LINE_LENGTH ) {
				Com_Error( ERR_DROP, "csv file line too long" );
			}
			if ( *line_start == ';' ) {
				// comment line
				line_start = buffer;
				continue;
			}

			// parse the line
			ParseCsvLine( line_start, buffer - line_start, fields, numFields );
			line_start = buffer;

			// process the line
			cb(env, fields);
		}
	}
}

typedef struct {
	char	name[MAX_QPATH];
	char	musicfile[MAX_QPATH];
} mapMusic_t;

typedef struct {
	mapMusic_t *maps;
	int		maxMaps;
	int		numMaps;
} mapMusicEnv_t;

void ParseMusicInfoFromLine( void *env, char *fields ) {
	mapMusicEnv_t	*denv = env;
	char			*input;

	// save the map music info
	if ( denv->numMaps == denv->maxMaps ) {
		Com_Error( ERR_DROP, "Too many map infos!" );
	}

	mapMusic_t *map = &denv->maps[denv->numMaps++];

	Q_strncpyz( map->name, &fields[0 * MAX_CSV_LINE_LENGTH], sizeof( map->name ) );

	input = &fields[1 * MAX_CSV_LINE_LENGTH];

	if ( strlen(input) + strlen("music/.mp3") > MAX_QPATH ) {
		Com_Error( ERR_DROP, "music filename is too long\n" );
	}
	sprintf( map->musicfile, "music/%s.mp3", input );
}

#define MAX_MAPS_PER_MUSIC_FILE 256
#define MAP_MUSIC_FIELDS 2

static mapMusic_t	mapMusic[MAX_MAPS_PER_MUSIC_FILE];
static int			numMapMusic;
static int			mapMusicLoaded = 0;

int ParseMusicInfoFromFile( const char *qpath ) {
	void	*buffer;
	int		length;
	mapMusicEnv_t env;

	env.maps = &mapMusic[0];
	env.numMaps = 0;
	env.maxMaps = MAX_MAPS_PER_MUSIC_FILE;

	length = FS_ReadFile( qpath, &buffer );
	ParseCsvFromMemory( (char *)buffer, length, MAP_MUSIC_FIELDS, &env, &ParseMusicInfoFromLine );
	return env.numMaps;
}

typedef struct {
	decoInfo_t *decos;
	int maxDecos;
	int	numDecos;
} decoInfoEnv_t;

void ParseDecoInfoFromLine( void *env, char *fields ) {
	int				i, exploding;
	decoInfoEnv_t	*denv = env;

	// save the deco info
	if ( denv->numDecos == denv->maxDecos ) {
		Com_Error( ERR_DROP, "Too many decos!" );
	}

	decoInfo_t *deco = &denv->decos[denv->numDecos++];
	// TODO: add to hash?

	Q_strncpyz( deco->name, &fields[0 * MAX_CSV_LINE_LENGTH], sizeof( deco->name ) );
	Q_strncpyz( deco->path, &fields[1 * MAX_CSV_LINE_LENGTH], sizeof( deco->path ) );

	if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "toss" ) ) {
		deco->moveType = MOVETYPE_TOSS;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "none" ) ) {
		deco->moveType = MOVETYPE_NONE;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "bounce" ) ) {
		deco->moveType = MOVETYPE_BOUNCE;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "float" ) ) {
		deco->moveType = MOVETYPE_FLOAT;
	} else {
		deco->moveType = MOVETYPE_TOSS;
	}

	if ( !Q_stricmp( &fields[3 * MAX_CSV_LINE_LENGTH], "bbox" ) ) {
		deco->solidType = SOLID_BBOX;
	} else if ( !Q_stricmp( &fields[3 * MAX_CSV_LINE_LENGTH], "not" ) ) {
		deco->solidType = SOLID_NOT;
	} else {
		deco->solidType = SOLID_BBOX;
	}

	deco->mass = atof( &fields[4 * MAX_CSV_LINE_LENGTH] );

	exploding = atoi( &fields[5 * MAX_CSV_LINE_LENGTH] );
	if ( exploding == 0 ) {
		deco->exploding = 0;
	} else {
		deco->exploding = 1;
	}

	deco->hitPoints = atof( &fields[6 * MAX_CSV_LINE_LENGTH] );

	deco->mins[0] = atof( &fields[7 * MAX_CSV_LINE_LENGTH] );
	deco->mins[1] = atof( &fields[8 * MAX_CSV_LINE_LENGTH] );
	deco->mins[2] = atof( &fields[9 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[0] = atof( &fields[10 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[1] = atof( &fields[11 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[2] = atof( &fields[12 * MAX_CSV_LINE_LENGTH] );

	if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "WOOD" ) ) {
		deco->gibType = DK_DECO_FLAGS_WOOD;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "METAL" ) ) {
		deco->gibType = DK_DECO_FLAGS_METAL;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "GLASS" ) ) {
		deco->gibType = DK_DECO_FLAGS_GLASS;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "GIBS" ) ) {
		deco->gibType = DK_DECO_FLAGS_GIB;
	} else {
		deco->gibType = DK_DECO_FLAGS_NO_BREAK;
	}

	deco->numAnimSequences = atoi( &fields[14 * MAX_CSV_LINE_LENGTH] );
	if ( deco->numAnimSequences < 0 ) {
		deco->numAnimSequences = 0;
	}
	if ( deco->numAnimSequences > MAX_DECO_ANIM_SEQS ) {
		deco->numAnimSequences = MAX_DECO_ANIM_SEQS;
	}

	for ( i = 0; i < deco->numAnimSequences; i++ ) {
		// parse anim seq
		decoAnimSeq_t *seq = deco->animSequences + i;

		if ( sscanf( &fields[(15+i) * MAX_CSV_LINE_LENGTH], "%d~%d", &seq->startFrame, &seq->endFrame) == 2 ) {
			seq->frameFlags = FRAME_LOOP;
		} else if ( sscanf( &fields[(15+i) * MAX_CSV_LINE_LENGTH], "%d %d", &seq->startFrame, &seq->endFrame ) == 2 ) {
			seq->frameFlags = FRAME_ONCE;
		} else {
			seq->startFrame = atoi( &fields[(15+i) * MAX_CSV_LINE_LENGTH] );
			seq->endFrame = seq->startFrame;
			seq->frameFlags = FRAME_LOOP;
		}
	}
}

#define MAX_DECOS_PER_FILE 256

static decoInfo_t	decos[4][MAX_DECOS_PER_FILE];
static int			num_decos[4];
static int			decos_loaded = 0;

int ParseDecoInfoFromFile( const char *qpath, int episode ) {
	void	*buffer;
	int		length;
	decoInfoEnv_t env;

	if ( episode < 0 || episode > 3 ) {
		Com_Error( ERR_DROP, "wrong episode" );
	}

	env.decos = &decos[episode][0];
	env.numDecos = 0;
	env.maxDecos = MAX_DECOS_PER_FILE;

	length = FS_ReadFile( qpath, &buffer );
	ParseCsvFromMemory( (char *)buffer, length, NUM_DECO_FIELDS, &env, &ParseDecoInfoFromLine );
	return env.numDecos;
}

decoInfo_t *GetDeco( int episode, const char *name ) {
	if ( episode < 0 || episode > 3 ) {
		Com_Error( ERR_DROP, "wrong episode" );
	}

	for ( int i = 0; i < num_decos[episode]; i++ ) {
		decoInfo_t *info = &decos[episode][i];

		if ( !Q_stricmp( name, info->name ) ) {
			return info;
		}
	}

	return NULL;	
}

// JKA misc_model_breakable spawnflags
#define BR_FLAGS_SOLID			1 // Movement is blocked by it, if not set, can still be broken by explosions and shots if it has health
#define BR_FLAGS_AUTOANIMATE	2 // Will cycle it's anim
#define BR_FLAGS_DEADSOLID		4 // Stay solid even when destroyed (in case damage model is rather large).
#define BR_FLAGS_NO_DMODEL		8 // Makes it NOT display a damage model when destroyed, even if one exists
#define BR_FLAGS_USE_MODEL		16 // When used, will toggle to it's usemodel (model + "_u1.md3")... this obviously does nothing if USE_NOT_BREAK is not checked
#define BR_FLAGS_USE_NOT_BREAK	32 // Using it, doesn't make it break, still can be destroyed by damage
#define BR_FLAGS_PLAYER_USE		64 // Player can use it with the use button
#define BR_FLAGS_NO_EXPLOSION	128 // By default, will explode when it dies...this is your override.
#define BR_FLAGS_START_OFF		256 // Will start off and will not appear until used.

static void dkent_deco_e1( entity_t *ent )
{
	// NOTE: this entity is only for JKA

	epair_t pairs[] = {
		{ NULL, "scale", "modelscale" }
	};

	int		spawnflags = IntegerForKey( ent, "spawnflags" );

	RenameKeys( ent, sizeof( pairs ) / sizeof( *pairs ), pairs );

	SetKeyValue( ent, "classname", "misc_model_breakable" );

	int		jkaSpawnFlags = 0;

/*
"health"	how much health to have - default is zero (not breakable)  If you don't set the SOLID flag, but give it health, it can be shot but will not block NPCs or players from moving
"targetname" when used, dies and displays damagemodel (model + "_d1.md3"), if any (if not, removes itself)
"target" What to use when it dies
"target2" What to use when it's repaired
"target3" What to use when it's used while it's broken
"paintarget" target to fire when hit (but not destroyed)
"count"  the amount of armor/health/ammo given (default 50)
"radius"  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"NPC_targetname" - Only the NPC with this name can damage this
"forcevisible" - When you turn on force sight (any level), you can see these draw through the entire level...
"redCrosshair" - crosshair turns red when you look at this

"gravity"	if set to 1, this will be affected by gravity
"throwtarget" if set (along with gravity), this thing, when used, will throw itself at the entity whose targetname matches this string
"mass"		if gravity is on, this determines how much damage this thing does when it hits someone.  Default is the size of the object from one corner to the other, that works very well.  Only override if this is an object whose mass should be very high or low for it's size (density)

Damage: default is none
"splashDamage" - damage to do (will make it explode on death)
"splashRadius" - radius for above damage

"team" - This cannot take damage from members of this team:
	"player"
	"neutral"
	"enemy"
*/

	const char *name;
	char		modelname[MAX_QPATH];
	decoInfo_t *deco;

	name =  ValueForKey( ent, "model" );
	deco = GetDeco( 0, name );
	if ( deco == NULL ) {
		Com_Error( ERR_DROP, "Unknown deco %s", name );
	}

	COM_StripExtension( deco->path, modelname );
	strcat( modelname, ".md3" );
	SetKeyValue( ent, "model", modelname );

	// set other properties

	if ( deco->solidType == SOLID_BBOX ) {
		jkaSpawnFlags |= BR_FLAGS_SOLID;
		jkaSpawnFlags |= BR_FLAGS_DEADSOLID;
	}

	if ( deco->moveType == MOVETYPE_NONE ) {
		
	} else {
		SetKeyInteger( ent, "gravity", 1 );
	}

	float health = FloatForKey( ent, "health" );
	if ( !health ) {
		health = deco->hitPoints;
	}
	if ( spawnflags & DK_DECO_FLAGS_NO_BREAK ) {
		health = 0;
	}

    if( ( ( spawnflags & ( DK_DECO_FLAGS_WOOD | DK_DECO_FLAGS_METAL | DK_DECO_FLAGS_GLASS | DK_DECO_FLAGS_GIB ) )
		 || deco->exploding == 1
		 || spawnflags & DK_DECO_FLAGS_EXPLODE )
		&& !( spawnflags & DK_DECO_FLAGS_NO_BREAK ) ) {
		// it explodes
		// TODO: set chunk material
		/*
		"material" - default is "8 - MAT_NONE" - choose from this list:
		0 = MAT_METAL		(grey metal)
		1 = MAT_GLASS		
		2 = MAT_ELECTRICAL	(sparks only)
		3 = MAT_ELEC_METAL	(METAL chunks and sparks)
		4 =	MAT_DRK_STONE	(brown stone chunks)
		5 =	MAT_LT_STONE	(tan stone chunks)
		6 =	MAT_GLASS_METAL (glass and METAL chunks)
		7 = MAT_METAL2		(blue/grey metal)
		8 = MAT_NONE		(no chunks-DEFAULT)
		9 = MAT_GREY_STONE	(grey colored stone)
		10 = MAT_METAL3		(METAL and METAL2 chunk combo)
		11 = MAT_CRATE1		(yellow multi-colored crate chunks)
		12 = MAT_GRATE1		(grate chunks--looks horrible right now)
		13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
		14 = MAT_CRATE2		(red multi-colored crate chunks)
		15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
		*/
		jkaSpawnFlags |= BR_FLAGS_NO_DMODEL;
	} else {
		jkaSpawnFlags |= BR_FLAGS_NO_EXPLOSION;
	}

	// set up the deco's frame animation
#if 1
	/*
	if ( ( deco->animSequences > 0 ) ) {
		// TODO: add the necessary logic to JKA
		jkaSpawnFlags |= BR_FLAGS_AUTOANIMATE;
	}
	*/
#elif 0
	if ((info.animSequences > 0) && (hook->animseq != -1))
	{
		if (hook->animseq > (info.animSequences - 1))
		{
	        gstate->Con_Printf ("animseq specified for deco at %s is invalid!!\n", com->vtos(self->s.origin));
			self->s.frame = 0;
		}
		else
		{
			LPANIMSEQ lpSeq = &info.seq[hook->animseq];
			com->AnimateEntity(self, lpSeq->start, lpSeq->end, lpSeq->flags, 0.10);
		}
	}
	else
	{
		//set the frame
		//self->s.frame = hook->frame;
		entAnimate(self,0,hook->frame,FRAME_ONCE,0.05);
	}
#endif

	SetKeyInteger( ent, "health",  health );
	SetKeyFloat( ent, "mass", deco->mass );
	SetVectorForKey( ent, "mins", deco->mins );
	SetVectorForKey( ent, "maxs", deco->maxs );
	SetKeyInteger( ent, "spawnflags", jkaSpawnFlags );
}

static void dkent_effect_rain( entity_t *ent ) {
	// NOTE: this is only for JKA
	// TODO: add misc_weather_zone also!
	SetKeyValue( ent, "classname", "fx_rain" );

	// TODO: the brushmodel needs to have CONTENTS_OUTSIDE
	// - actually no. the leaves of the worldmodel have to be modified:
	//   - add brushes of the submodel with CONTENTS_OUTSIDE to them.
	//   - if these brushes are ONLY CONTENTS_OUTSIDE then we shouldn't break anything.
	SetKeyValue( ent, "spawnflags", "1" ); // light
}

static void dkent_worldspawn( const char *mapname, entity_t *ent )
{
	int			i;
	mapMusic_t	*music;
	epair_t		pairs[] = {
		{ NULL, "mapname", "message" }
	};

	RenameKeys( ent, sizeof( pairs ) / sizeof( *pairs ), pairs );

	// loadscreen: says which load screen to use
	// but Q3 always picks map name instead

	for ( i = 0; i < numMapMusic; i++ ) {
		music = &mapMusic[i];

		if ( Q_stricmp( mapname, music->name ) ) {
			continue;
		}

		SetKeyValue( ent, "music", music->musicfile );
		break;
	}

	// ignore fog_value, fog_color, fog_start, fog_end
	// JKA supports "global fog", does it not?
}

static void ProcessEntity( const char *mapname, int entityNum, entity_t *ent )
{
	const char *className = ValueForKey(ent, "classname");

	if (!strcmp(className, "worldspawn")) {
		dkent_worldspawn( mapname, ent );
	}
	else if (!strcmp(className, "info_player_start")) {
		// keep it
	}
	else if (!strcmp(className, "effect_rain")) {
		dkent_effect_rain( ent );
	}
	else if (!strcmp(className, "deco_e1")) {
		dkent_deco_e1( ent );
	}
	else if (!strcmp(className, "light")) {
		// keep it
	}
	else if (!strcmp(className, "trigger_hurt")) {
		dkent_trigger_hurt( ent );
	}
	else if (!strcmp(className, "target_speaker")) {
		dkent_target_speaker( ent );
	}
	else if (!strcmp(className, "sound_ambient")) {
		dkent_sound_ambient( ent );
	}
	else if (!strcmp(className, "info_null")) {
		// keep it
	}
	else if (!strcmp(className, "info_not_null")) {
		SetKeyValue( ent, "classname", "info_notnull" );
		// keep it
	}
	//else if (!strcmp(className, "func_areaportal")) {
	//	// keep it
	//}
	else if (!strcmp(className, "func_button")) {
		// keep it
	}
	else if (!strcmp(className, "func_door")) {
		// keep it
		// TODO: quake3 does not support custom sounds!
		// TODO: in DK touching doors will be linked unless turned off via a flag..
		char door_name[32];

		door_name[0] = '\0';
		sprintf( door_name, "door%d", entityNum );

		SetKeyValue( ent, "team", door_name );
	}
	else if (!strcmp(className, "func_timer")) {
		// keep it
		/* DK:
///////////////////////////////////////////////////////////////////////////////
//	func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) FUNC_DYNALIGHT_START_ON
//	wait		- base time between triggering all targets, default is 1
//	random		- wait variance, default is 0
//
//	so, the basic time between firing is a random time between
//	(wait - random) and (wait + random)
//
//	delay		- delay before first firing when turned on, default is 0
//
//	pausetime	- additional delay used only the very first time
//				  and only if spawned with FUNC_DYNALIGHT_START_ON
//
///////////////////////////////////////////////////////////////////////////////
		*/
	}
	else {
		// remove it
		FreeEntity( ent );
	}
}

static void ConvertEntityString(const char *mapname, bspFile_t *bsp, const void *data, dheader_t *header, skybox_t *skybox)
{
	static char mapEntityString[MAX_MAP_ENTSTRING];
	static int mapEntityStringSize;

	mapEntityStringSize = GetLumpElements( header, LUMP_ENTITIES, 1 );
	if ( mapEntityStringSize >= MAX_MAP_ENTSTRING ) {
		Com_Error( ERR_DROP, "Entity string too long" );
	}

	CopyLump( header, LUMP_ENTITIES, data, mapEntityString, mapEntityStringSize, qfalse ); /* NO SWAP */	

#if 1
	FILE *fp = fopen("/home/deck/Documents/proj/dk-restoration/dkres/maps/entities.txt", "wb");
	fwrite(mapEntityString, 1, mapEntityStringSize, fp);
	fclose(fp);
#endif

	ParseEntities( mapEntityString, mapEntityStringSize );

	int numEntities = NumEntities();
	for (int i = 0; i < numEntities; i++) {
		ProcessEntity( mapname, i, GetEntityNum( i ) );
		// PrintEntity( GetEntityNum( i ) );
	}
	if ( numEntities >= 1)
	{
		SkyboxFromEntity( skybox, GetEntityNum( 0 ) );
	}

	// TODO: JKA only. add this entity IF there's at least one effect_rain on the map
	/*{
		entity_t *fx_rain_ent = GetEmptyEntity();
		SetKeyValue( fx_rain_ent, "classname", "fx_rain" );
		SetKeyValue( fx_rain_ent, "spawnflags", "1" );
	}*/

	UnparseEntities( mapEntityString, &mapEntityStringSize, MAX_MAP_ENTSTRING );

	bsp->entityStringLength = mapEntityStringSize;
	bsp->entityString = malloc( bsp->entityStringLength );
	memcpy( bsp->entityString, mapEntityString, bsp->entityStringLength );
}

int DK_SurfaceFlagsToJKA( int flags )
{
	int result = 0;

	if (flags & SURF_SKY)
		result |= JKA_SURF_SKY;
	if (flags & SURF_WARP)
		result |= JKA_SURF_NOMARKS;
	
	if (flags & SURF_NODRAW)
		result |= JKA_SURF_NODRAW;
	
	return result;
}

int DK_SurfaceFlagsToQuake3( int flags )
{
	int result = 0;

	if (flags & SURF_FULLBRIGHT)
		result |= Q3_SURF_NOLIGHTMAP;
	if (flags & SURF_SKY)
		result |= Q3_SURF_SKY;
	if (flags & SURF_WARP)
		result |= Q3_SURF_NONSOLID | Q3_SURF_NOMARKS | Q3_SURF_NOLIGHTMAP;
	
	if (flags & SURF_NODRAW)
		result |= Q3_SURF_NODRAW;
	if (flags & SURF_HINT)
		result |= Q3_SURF_HINT;
	if (flags & SURF_SKIP)
		result |= Q3_SURF_SKIP;
	
	return result;
}

int DK_ContentsToJKA( int contents ) {
	int result = 0;

	if (contents & CONTENTS_SOLID)
		result |= JKA_CONTENTS_SOLID;
//WINDOW
//AUX
	if (contents & CONTENTS_LAVA)
		result |= JKA_CONTENTS_LAVA;
	if (contents & CONTENTS_SLIME)
		result |= JKA_CONTENTS_SLIME;
	if (contents & CONTENTS_WATER)
		result |= JKA_CONTENTS_WATER;
	if (contents & CONTENTS_FOG)
		result |= JKA_CONTENTS_FOG;

	if (contents & CONTENTS_PLAYERCLIP)
		result |= JKA_CONTENTS_PLAYERCLIP;
	
	if (contents & CONTENTS_MONSTERCLIP)
		result |= JKA_CONTENTS_MONSTERCLIP;

	if (contents & CONTENTS_NPCCLIP)
		result |= JKA_CONTENTS_BOTCLIP;

	if (contents & CONTENTS_TRANSLUCENT)
		result |= JKA_CONTENTS_TRANSLUCENT;
	if (contents & CONTENTS_DETAIL)
		result |= JKA_CONTENTS_DETAIL;

	return result;
}

int DK_ContentsToQuake3( int contents )
{
	int result = 0;

	if (contents & CONTENTS_SOLID)
		result |= Q3_CONTENTS_SOLID;
//WINDOW
//AUX
	if (contents & CONTENTS_LAVA)
		result |= Q3_CONTENTS_LAVA;
	if (contents & CONTENTS_SLIME)
		result |= Q3_CONTENTS_SLIME;
	if (contents & CONTENTS_WATER)
		result |= Q3_CONTENTS_WATER;
	if (contents & CONTENTS_FOG)
		result |= Q3_CONTENTS_FOG;

	if (contents & CONTENTS_ORIGIN)
		result |= Q3_CONTENTS_ORIGIN;

	if (contents & CONTENTS_AREAPORTAL)
		result |= Q3_CONTENTS_AREAPORTAL;

	if (contents & CONTENTS_PLAYERCLIP)
		result |= Q3_CONTENTS_PLAYERCLIP;
	
	if (contents & CONTENTS_MONSTERCLIP)
		result |= Q3_CONTENTS_MONSTERCLIP;

	if (contents & CONTENTS_NPCCLIP)
		result |= Q3_CONTENTS_BOTCLIP;

	if (contents & CONTENTS_TRANSLUCENT)
		result |= Q3_CONTENTS_TRANSLUCENT;
	if (contents & CONTENTS_DETAIL)
		result |= Q3_CONTENTS_DETAIL;

	return result;
}

static int StringEndsWith(const char *str, const char *suffix)
{
    size_t len = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  len)
        return 0;

    return strncmp(str + len - lensuffix, suffix, lensuffix) == 0;
}

#define FILE_HASH_SIZE 256
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

static void OutputGeneratedFogShaders( const char *basename, mfog_t *fogs, int numFogs, strbuf_t *sb )
{
	int		i;
	mfog_t	*fog;
	char	line[2048];

	if (VIEWFOGS)
	{
		return;
	}

	for (i=0 ; i< numFogs ; i++)
	{
		fog = &fogs[i];

		sprintf (line, "%s\n", fog->shader);
		AppendToBuffer( sb, line );

		AppendToBuffer( sb, "{\n" );

		float	color[3];
		float	distance;

		color[0] = ( float )( fog->texinfo->color[0] ) / 255.0;
		color[1] = ( float )( fog->texinfo->color[1] ) / 255.0;
		color[2] = ( float )( fog->texinfo->color[2] ) / 255.0;

		distance = fog->texinfo->value;
		if( distance == 0 )
		{
			distance = 128;
		}

		sprintf( line, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm fog\nsurfaceparm nolightmap\n" );
		AppendToBuffer( sb, line );

		sprintf(line, "fogparms ( %f %f %f ) %f\n",
			fog->shader, color[0], color[1], color[2], distance);
		AppendToBuffer( sb, line );

		AppendToBuffer( sb, "}\n" );
	}
}

static void OutputTexInfoShaders( const char *basename, msurface_t *faces, int numFaces, mtexinfo_t *texInfos, int numTexInfos, skybox_t *skybox, mfog_t *fogs, int numFogs, bspFile_t *bsp )
{
	int		i, j, len, size;
	static char	filedata[16384 * 1024];
	int		filedatasize;
	msurface_t	*face;
	mtexinfo_t	*tex, *htex, *frametex;
	char	line[2048];
	char	texname[MAX_QPATH];
	strbuf_t	stringbuf;

	StringBuffer( &stringbuf, filedata, sizeof(filedata) - 1 );

	OutputGeneratedFogShaders( basename, fogs, numFogs, &stringbuf );

	SkyBoxPrint( basename, skybox, &stringbuf );

	if (VIEWFOGS)
	{
		for ( i = 0, tex = texInfos; i < numTexInfos; i++, tex++ ) {
			if ( !(tex->flags & SURF_FOGPLANE) )
			{
				continue;
			}

			sprintf (line, "texinfo_%d\n", i);
			AppendToBuffer( &stringbuf, line );

			AppendToBuffer( &stringbuf, "{\n" );

			float	color[3];
			float	distance;

			color[0] = ( float )( tex->color[0] ) / 255.0;
			color[1] = ( float )( tex->color[1] ) / 255.0;
			color[2] = ( float )( tex->color[2] ) / 255.0;

			distance = tex->value;
			if( distance == 0 )
			{
				distance = 128;
			}

			sprintf( line, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm nolightmap\n" );
			AppendToBuffer( &stringbuf, line );

			sprintf(line, "viewfogvars ( %f %f %f ) %f\n",
				color[0], color[1], color[2], distance);
			AppendToBuffer( &stringbuf, line );

			AppendToBuffer( &stringbuf, "}\n" );
		}
	}

#if 0
	// deduplicate all texinfos. if two texinfos have same texture, flags -- they are the same shader
	//    - skip a texinfo that is not the first frame of animation
	static mtexinfo_t*		hashTable[FILE_HASH_SIZE];

	// TODO: need to take surfaces into account!
	// e.g. if two surfs use same texinfo, but have different surface flags,
	//   we should generate a shader for each one.
	memset( hashTable, 0, sizeof( hashTable ) );
	for ( i = 0, face = faces; i < numFaces; i++, face++ ) {
		if ( ( !face->flags && face->texinfo->image->num_alpha_pixels == 0 )
			|| ( face->flags & SURF_SKY ) )
			continue; // does not require a shader

		long hash = generateHashValue( tex->image->name );

		for ( htex = hashTable[hash]; htex; htex = htex->hash_next ) {
			if ( !strcmp( tex->image->name, htex->image->name ) ) {
				// TODO: go over dupes and figure out which of those flags to use?
				// - but then, need to group them somehow
				tex->duplicate = 1;
				break;
			}
		}
		if ( !htex ) {
			tex->hash_next = hashTable[hash];
			hashTable[hash] = tex;
		}
	}

	for ( i = 0, tex = texInfo; i < numTexInfo; i++, tex++ ) {
		if ( ( !tex->flags && tex->image->num_alpha_pixels == 0 )
			|| ( tex->flags & SURF_SKY )
			|| tex->duplicate )
			continue;

		// SURF_FLOWING + value
		// SURF_LIGHT + value
		// SURF_TRANS33, SURF_TRANS66: just set 0.33 and 0.66 alphaGen constant. disable culling. this is a transparent surf.
		// SURF_MIDTEXTURE.
		// SURF_FULLBRIGHT --> no lightmap!
		// SURF_WARP. this is warping surface. needs subdivision.
#if 0
		// print out flags
		Com_Printf("%s %d ", tex->image->name, tex->value );
#define PRINTFLAG(f) do { if (tex->flags & (f)) Com_Printf( "%s ", #f ); } while (0)
		PRINTFLAG(SURF_LIGHT);
		PRINTFLAG(SURF_FULLBRIGHT);
		PRINTFLAG(SURF_SKY);
		PRINTFLAG(SURF_WARP);
		PRINTFLAG(SURF_TRANS33);
		PRINTFLAG(SURF_TRANS66);
		PRINTFLAG(SURF_FLOWING);
		PRINTFLAG(SURF_NODRAW);
		PRINTFLAG(SURF_HINT);
		PRINTFLAG(SURF_SKIP);
		PRINTFLAG(SURF_WOOD);
		PRINTFLAG(SURF_METAL);
		PRINTFLAG(SURF_STONE);
		PRINTFLAG(SURF_GLASS);
		PRINTFLAG(SURF_ICE);
		PRINTFLAG(SURF_SNOW);
		PRINTFLAG(SURF_MIRROR);
		PRINTFLAG(SURF_TRANSTHING);
		PRINTFLAG(SURF_ALPHACHAN);
		PRINTFLAG(SURF_MIDTEXTURE);
		PRINTFLAG(SURF_PUDDLE);
		PRINTFLAG(SURF_SURGE);
		PRINTFLAG(SURF_BIGSURGE);
		PRINTFLAG(SURF_BULLETLIGHT);
		PRINTFLAG(SURF_FOGPLANE);
		PRINTFLAG(SURF_SAND);
		Com_Printf("\n");
#undef PRINTFLAG
#endif
		COM_StripExtension( tex->image->name, texname );

		sprintf( line, "%s\n", texname );
		AppendToBuffer( &stringbuf, line );
		AppendToBuffer( &stringbuf, "{\n" );

		if (tex->flags & SURF_LIGHT)
		{
			sprintf( line, "\tq3map_surfaceLight %d\n", tex->value );
			AppendToBuffer( &stringbuf, line );
		}
		if (tex->flags & SURF_FULLBRIGHT)
		{
			AppendToBuffer( &stringbuf, "\tsurfaceparm nolightmap\n" );
		}
		if (tex->flags & (SURF_TRANS33|SURF_TRANS66)) {
			AppendToBuffer( &stringbuf, "\tsurfaceparm trans\n" );
			//AppendToBuffer( &stringbuf, "\tcull none\n" );
		}
		// SURF_FLOWING
		//if (tex->flags & SURF_FLOWING) {
		//	printf("its thinking!\n");
		//}
		if (tex->flags & SURF_NODRAW) {
			AppendToBuffer( &stringbuf, "\tsurfaceparm nodraw\n" );
		}

		// isn't this surface flag???
		if ( tex->flags & SURF_WARP ) {
			AppendToBuffer( &stringbuf, "\ttessSize 256\n" ); // gl_subdivide_size default value

			if( tex->flags & ( SURF_SURGE | SURF_BIGSURGE ) )
			{
				float fWaveMultiplier, fWaveModifier;

				if( tex->flags & SURF_SURGE )
				{
					fWaveModifier = 0.250;		// small surge
					fWaveMultiplier = 1;
				}
				else
				{
					fWaveModifier = 0.500;		// big surge
					fWaveMultiplier = 4;
				}

				sprintf( line, "\tdeformVertexes wave %f sin 0 %f 0 1\n", fWaveMultiplier, fWaveModifier );
				AppendToBuffer( &stringbuf, line );
			}
		}
/*
float TURBSCALE = (256.0 / (2 * M_PI));
float ONE_DIV_SIXTYFOUR = 0.015625;
static void Foo(mtexinfo_t *texinfo, float fWarpTime) {
	float		fScroll;
	int			nWidth = texinfo->image->width;
	int			nHeight = texinfo->image->height;
	float		fOrigS, fOrigT, fWarpS, fWarpT, fS, fT, zWave;
	float		fWaveModifier = 0;
	int			fWaveMultiplier = 0;
	qboolean	bSurge = qfalse;

	if( texinfo->flags & SURF_FLOWING )
		fScroll = 64 * ( ( fWarpTime * 0.5 ) - ( int )( fWarpTime * 0.5) );
	else
		fScroll = 0;

	if( texinfo->flags & SURF_WARP ) // NOTE: actually surf flags!
	{
		if( texinfo->flags & ( SURF_SURGE | SURF_BIGSURGE ) )
		{
			bSurge = qtrue;

			if( texinfo->flags & SURF_SURGE )
			{
				fWaveModifier = 0.250;		// small surge
				fWaveMultiplier = 1;
			}
			else
			{
				fWaveModifier = 0.500;		// big surge
				fWaveMultiplier = 4;
			}
		}

		// 

		// fOrigS = texcoord.s
		// fOrigT = texcoord.t
		// NOTE: r_turbsin is 256 values of sine from the 360 degrees rotation, recorded up to 6 decimal digits
		//     for (int i = 0; i < 256; i++) {
        //       double f = sin( 360.0 * (double)(i/256.0) * M_PI/180.0 ) * 8.0;
		//		 fprintf(fp, "%g,", f);
		//     }
		fWarpS = r_turbsin[Q_ftol( ( ( fOrigT * 0.125 + fWarpTime ) * TURBSCALE ) ) & 255];
		fWarpT = r_turbsin[Q_ftol( ( ( fOrigS * 0.125 + fWarpTime ) * TURBSCALE ) ) & 255];

		fS = fOrigS + fWarpS;
		fS -= fScroll;
		fS /= nWidth;

		fT = fOrigT + fWarpT;
		fT /= nHeight;

		if( bSurge )
		{
			zWave = r_turbsin[Q_ftol( ( ( ( fS + fT ) * fWaveModifier + fWarpTime ) * TURBSCALE ) ) & 255];
			vVertex.z += zWave * fWaveMultiplier;
		}
	}
}
*/

		AppendToBuffer( &stringbuf, "\t{\n" );
		/*if ( tex->next ) {
			// TODO: animated, but only output the FIRST frame in the chain... doesn't make sense to do others
			float frequency = tex->numframes * 0.1;

			sprintf( line, "\t\tanimMap %f", frequency );
			AppendToBuffer( &stringbuf, line );
			for ( j = 0, frametex = tex; j < 8 && frametex; j++, frametex = frametex->next ) {
				sprintf( line, " %s", frametex->image->name );
				AppendToBuffer( &stringbuf, line );
			}
			AppendToBuffer( &stringbuf, "\n" );
		} else*/ {
			sprintf( line, "\t\tmap %s\n", tex->image->name );
			AppendToBuffer( &stringbuf, line );
		}
		if ( tex->flags & ( SURF_TRANS33|SURF_TRANS66) ) {
			float	transp = tex->flags & SURF_TRANS33 ? 0.33 : 0.66;

			sprintf( line, "\t\talphaGen const %f\n", transp );
			AppendToBuffer( &stringbuf, line );
		}
		if ( tex->flags & ( SURF_TRANS33|SURF_TRANS66) || tex->image->num_alpha_pixels ) {
			AppendToBuffer( &stringbuf, "\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n" );
			AppendToBuffer( &stringbuf, "\t\talphaFunc GT0\n" );
			AppendToBuffer( &stringbuf, "\t\tdepthWrite\n" );
		}
		if (tex->flags & SURF_FLOWING) {
			//float fScroll = 64 * FLOWING_SPEED;

			// TODO: need to take flowing into account
			// N shaders... seems like no way to do otherwise
			AppendToBuffer( &stringbuf, "\t\ttcmod scroll 0.5 0.5\n");
		}
		AppendToBuffer( &stringbuf, "\t\trgbGen identity\n" );
		AppendToBuffer( &stringbuf, "\t}\n" );

		if (!(tex->flags & (SURF_FULLBRIGHT|SURF_WARP|SURF_NODRAW|SURF_HINT|SURF_SKIP|SURF_SKY))) {
			// add lightmap pass
			AppendToBuffer( &stringbuf, "\t{\n" );
			AppendToBuffer( &stringbuf, "\t\tmap $lightmap\n" );
			AppendToBuffer( &stringbuf, "\t\trgbgen identityLighting\n" );
			AppendToBuffer( &stringbuf, "\t\tblendFunc filter\n" );
			AppendToBuffer( &stringbuf, "\t\tdepthFunc equal\n" );
			AppendToBuffer( &stringbuf, "\t\ttcgen lightmap\n" );
			AppendToBuffer( &stringbuf, "\t}\n" );
		}

		AppendToBuffer( &stringbuf, "}\n" );
	}
#endif
	filedatasize = stringbuf.cur - stringbuf.buf + 1;

	bsp->shaderStringLength = filedatasize + 1;
	bsp->shaderString = malloc( filedatasize + 1 );
	Q_strncpyz( bsp->shaderString, filedata, filedatasize );
}

#if 1
// NOTE: only for JKA
void MarkOutside( int outsideShaderNum, dbrush_t *brushes, dbrushside_t *brushSides, int *leafBrushes, dnode_t *nodes, dleaf_t *leafs, int node, dmodel_t *dm ) {
	int i;
	dleaf_t *pleaf;
	dnode_t	*pnode;

	while ( node >= 0 )
	{
		pnode = &nodes[node];
		// TODO: clip?
		// - also dmodel bounds are not good enough (need also height!)
		// - check if dmodel bounds are split by the node
		//   - only on the right side: go only the right side
		//   - only on the left side: go only the left side
		//   - split: carry on with the fragments (smaller bounds)
		// - when arrived at a leaf:
		//   - add the submodel brush to the leafbrushes of this leaf

		// look at CM_PointLeafnum_r for traversal code (but that's for points, you have a box)
		MarkOutside( outsideShaderNum, brushes, brushSides, leafBrushes, nodes, leafs, pnode->children[0], dm );
		node = pnode->children[1];
	}

	pleaf = &leafs[-1 - node];
	for ( i = 0; i < pleaf->numLeafBrushes; i++ )
	{
		int brushnum = leafBrushes[pleaf->firstLeafBrush + i];
		brushes[brushnum].shaderNum = outsideShaderNum;
		//Com_Printf( "outside: %s\n", texinfos[brushes[brushnum].shaderNum].image->name );
		// brushes[brushnum].shaderNum = // common/outside
		//brushSides[brushes[brushnum].firstSide].planeNum
	}
}

static void FindSubmodelBrushRange( int *leafBrushes, dnode_t *nodes, dleaf_t *leafs, int node, int *firstBrush, int *lastBrush ) {
	int i;
	dleaf_t *pleaf;
	dnode_t	*pnode;

	while ( node >= 0 )
	{
		pnode = &nodes[node];
		FindSubmodelBrushRange( leafBrushes, nodes, leafs, pnode->children[0], firstBrush, lastBrush );
		node = pnode->children[1];
	}

	pleaf = &leafs[-1 - node];
	for ( i = 0; i < pleaf->numLeafBrushes; i++ )
	{
		int brushnum = leafBrushes[pleaf->firstLeafBrush + i];
		if (*firstBrush > brushnum)
			*firstBrush = brushnum;
		if (*lastBrush < brushnum)
			*lastBrush = brushnum;
	}
}
#endif

static void ProcessTexInfo( bspFile_t *bsp, const void *data, dheader_t *header, mtexinfo_t *texInfo, int numTexInfo ) {
	int				i;
	realDtexinfo_t	*in;
	float			*fin;
	mtexinfo_t		*out;
	char			*err = NULL;
	char			filename[256];
	char			filename_out[256];
	int				next;

	in = GetLump( header, data, LUMP_TEXINFO );
	for ( i = 0, out = texInfo; i < numTexInfo; i++, in++, out++ )
	{
		out->s[0] = LittleFloat( in->vecs[0][0] );
		out->s[1] = LittleFloat( in->vecs[0][1] );
		out->s[2] = LittleFloat( in->vecs[0][2] );
		out->s_offset = LittleFloat( in->vecs[0][3] );

		out->t[0] = LittleFloat( in->vecs[1][0] );
		out->t[1] = LittleFloat( in->vecs[1][1] );
		out->t[2] = LittleFloat( in->vecs[1][2] );
		out->t_offset = LittleFloat( in->vecs[1][3] );

		out->value = LittleLong( in->value );
		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = texInfo + next;
		else
			out->next = NULL;
		
		sprintf(filename, "textures/%s.tga", in->texture);
		out->image = image_load_from_file(filename, &err);
		if (err != NULL)
		{
			printf("Error loading texture %s: %s", in->texture, err);
			out->image = image_notexture;
		}
		out->hash_next = NULL;

		// skip editor-only texinfos
		if (StringEndsWith(out->image->name, "clip.tga"))
		{
			out->skip = qtrue;
		}
		else if (StringEndsWith(out->image->name, "trigger.tga"))
		{
			out->skip = qtrue;
		}
		else if (StringEndsWith(out->image->name, "hint.tga"))
		{
			out->skip = qtrue;
		}
	}

	// count animation frames
	for ( i = 0; i < numTexInfo; i++ )
	{
		mtexinfo_t *step;

		out = &texInfo[i];
		out->numframes = 1;

		for (step = out->next ; step && step != out ; step=step->next)
		{
			out->numframes++;

			// amw - to catch corrupted .wal files..
			if (out->numframes > 1024)
			{
				break;
			}
		}
	}

	fin = GetLump( header, data, LUMP_EXTSURFINFO );

	for (i = 0, out = texInfo; i < numTexInfo; i++, out++)
	{
		out->color[0] = LittleFloat (*fin++);
		out->color[1] = LittleFloat (*fin++);
		out->color[2] = LittleFloat (*fin++);
	}
}

static void ProcessFaces(
		bspFile_t *bsp, const void *data, dheader_t *header,
		msurface_t *faces, int numFaces,
		mtexinfo_t *texInfo, int numTexInfo,
		float *verts, int numVerts,
		byte *lightmapData, int lightingLength,
		medge_t *edges, int numEdges,
		int *surfEdges, int numSurfEdges
	) {
	int i, j;
	static gllightmapstate_t gl_lms;

	GL_BeginBuildingLightmaps(&gl_lms);
	{
		realDface_t *in = GetLump( header, data, LUMP_FACES );
		msurface_t *out = faces;

		for ( i = 0; i < numFaces; i++, in++, out++ )
		{
			int planenum, side, ti;

			out->firstedge = LittleLong(in->firstedge);
			out->numedges = LittleShort(in->numedges);		
			out->flags = 0;
			out->fogNum = -1;

			planenum = LittleShort(in->planenum);
			side = LittleShort(in->side);
			if (side)
			{
				// this means normal should be flipped? or the winding reversed?
				//out->flags |= SURF_PLANEBACK;
				planenum += bsp->numPlanes / 2;
			}

			out->side = side;
			out->plane = bsp->planes + planenum;
			
			//out->nNumSurfSprites = 0;

			ti = LittleShort (in->texinfo);
			if (ti < 0 || ti >= numTexInfo)
				Com_Error (ERR_DROP, "MOD_LoadBmodel: bad texinfo number");
			out->texinfo = texInfo + ti;

			CalcSurfaceExtents (verts, edges, surfEdges, out);

		// lighting info

			for (j=0 ; j<DK_MAXLIGHTMAPS ; j++)
				out->styles[j] = in->styles[j];
			j = LittleLong(in->lightofs);
			if (j == -1)
				out->samples = NULL;
			else
				out->samples = lightmapData + j;
			
			// create lightmaps and polygons
			if ( lightingLength > 0 &&
				 !( out->texinfo->flags & ( SURF_SKY | SURF_FULLBRIGHT) ) && 
				( out->texinfo->flags != SURF_FOGPLANE ) )
			{
				GL_CreateSurfaceLightmap (&gl_lms, out);
			}
			else
			{
				out->lightmaptexturenum = -1;
			}

			// set the drawing flags
			//if (out->texinfo->flags & SURF_WARP)
			//{
			//	out->flags |= SURF_DRAWTURB;
			//	GL_SubdivideSurface (out);	// cut up polygon for warps
			//}
			//if (! (out->texinfo->flags & (SURF_WARP)) ) 
			GL_BuildPolygonFromSurface(verts, edges, surfEdges, out);
		}
	}

	LM_UploadBlock(&gl_lms);

	if (lightingLength > 0)
	{
		mlightmap_t *lm, *lmnext;
		byte *lmbuf;
		size_t len;

		bsp->numLightmaps = gl_lms.texnum;
		len = bsp->numLightmaps * LM_BLOCKLIGHTS_SIZE;
		bsp->lightmapData = malloc( len );

		lmbuf = bsp->lightmapData + len;
		lm = gl_lms.lightmaps;
		while (lm != NULL)
		{
			lmbuf -= LM_BLOCKLIGHTS_SIZE;
			lmnext = lm->next;
			memcpy(lmbuf, lm->buffer, LM_BLOCKLIGHTS_SIZE);
			free(lm);
			lm = lmnext;
		}
	}
}

static void ConvertSurfaces( bspFile_t *bsp, msurface_t *faces, int numFaces, mtexinfo_t *texInfo, int numTexInfo ) {
	int	i,j,k;

	int numIndexes = 0;
	int numDrawVerts = 0;
	for (i = 0; i < numFaces; i++)
	{
		msurface_t *face = &faces[i];

		if (face->texinfo->skip) {
			continue; // texinfo has no shader: skip this
		}

		numDrawVerts += face->polys->numverts;
		numIndexes += (face->polys->numverts - 2) * 3;
	}
	
	bsp->numDrawVerts = 0;
	bsp->drawVerts = malloc( numDrawVerts * sizeof( *bsp->drawVerts ) );
	bsp->numDrawIndexes = 0;
	bsp->drawIndexes = malloc( numIndexes * sizeof( *bsp->drawIndexes ) );
	int maxSurfaces = numFaces;
	int numSurfaces = 0;
	bsp->surfaces = malloc( maxSurfaces * sizeof( *bsp->surfaces ) );
	for (i = 0; i < numFaces; i++)
	{
		msurface_t *in = &faces[i];

		dsurface_t *out = &bsp->surfaces[numSurfaces++];
		int *index;

		//if ((in->flags & SURF_FOGPLANE) || (in->texinfo->flags & SURF_FOGPLANE)) {
		//	printf("no idea %s\n", in->texinfo->image->name);
		//}

#if 1
		out->shaderNum = in->texinfo->shaderNum;
#else
		out->shaderNum = in->shaderNum;
#endif
		if (VIEWFOGS)
		{
			out->fogNum = in->fogNum;
		}
		else
		{
			out->fogNum = in->fogNum; // 0 if no fogs
		}
		out->surfaceType = MST_PLANAR;

		// convert vertices
		out->firstVert = bsp->numDrawVerts;
		if (in->texinfo->skip)
		{
			out->numVerts = 0;
		}
		else
		{
			out->numVerts = in->polys->numverts;
			bsp->numDrawVerts += out->numVerts;
			memcpy(bsp->drawVerts + out->firstVert, in->polys->verts, out->numVerts * sizeof(*bsp->drawVerts));
			for (j = 0; j < out->numVerts; j++)
			{
				bsp->drawVerts[out->firstVert + j].color[0][0] = in->texinfo->color[0];
				bsp->drawVerts[out->firstVert + j].color[0][1] = in->texinfo->color[1];
				bsp->drawVerts[out->firstVert + j].color[0][2] = in->texinfo->color[2];
				bsp->drawVerts[out->firstVert + j].color[0][3] = 255;
			}
		}

		// convert indices
		out->firstIndex = bsp->numDrawIndexes;
		if (in->texinfo->skip)
		{
			out->numIndexes = 0;
		}
		else
		{
			out->numIndexes = (in->polys->numverts - 2) * 3;
			bsp->numDrawIndexes += out->numIndexes;
			index = &bsp->drawIndexes[out->firstIndex];
			for (k = 0, j = 2; k < out->numIndexes; k += 3, j++) {
				index[k + 0] = 0;
				index[k + 1] = j - 1;
				index[k + 2] = j;
			}
		}

		out->lightmapNum[0] = in->lightmaptexturenum;

		// TODO: is this correct? Q3A does not seem to care about this
		//out->lightmapX = in->texturemins[0];
		//out->lightmapY = in->texturemins[1];
		//out->lightmapWidth = in->extents[0];
		//out->lightmapHeight = in->extents[1];
		out->lightmapX[0] = out->lightmapY[0] = 0;
		out->lightmapWidth = out->lightmapHeight = LIGHTMAP_SIZE;

		out->lightmapOrigin[0] = out->lightmapOrigin[1] = out->lightmapOrigin[2] = 0.0f;
		// TODO: are these tangent/binormal vectors in lightmap space?
		out->lightmapVecs[0][0] = out->lightmapVecs[0][1] = out->lightmapVecs[0][2] = 0.0f;
		out->lightmapVecs[1][0] = out->lightmapVecs[1][1] = out->lightmapVecs[1][2] = 0.0f;
		// this is the normal vector basically
		out->lightmapVecs[2][0] = in->plane->normal[0];
		out->lightmapVecs[2][1] = in->plane->normal[1];
		out->lightmapVecs[2][2] = in->plane->normal[2];
	}
	bsp->numSurfaces = numSurfaces;
}

static qboolean outputJKA = qfalse; // TODO: make it truly switchable

bspFile_t *BSP_LoadDK( const bspFormat_t *format, const char *name, const void *data, int length ) {
	int				i, j, k;
	dheader_t		header;
	bspFile_t		*bsp;
	char			mapnameBuffer[MAX_QPATH];
	char			*mapname;

	COM_StripExtension( name, mapnameBuffer );
	mapname = COM_SkipPath( mapnameBuffer );

	BSP_SwapBlock( (int *) &header, (int *)data, sizeof ( dheader_t ) );

	if ( header.ident != format->ident || header.version != format->version ) {
        Com_Printf("Header mismatch!\n");
		return NULL;
	}

	image_init();

	if ( !mapMusicLoaded ) {
		numMapMusic = ParseMusicInfoFromFile( "music/music.csv" );
		mapMusicLoaded = 1;
	}

	if ( !decos_loaded) {
		num_decos[0] = ParseDecoInfoFromFile( "models/e1/e1decoinfo.csv", 0 );
		num_decos[1] = ParseDecoInfoFromFile( "models/e2/e2decoinfo.csv", 1 );
		num_decos[2] = ParseDecoInfoFromFile( "models/e3/e3decoinfo.csv", 2 );
		num_decos[3] = ParseDecoInfoFromFile( "models/e4/e4decoinfo.csv", 3 );
		decos_loaded = 1;
	}

	bsp = malloc( sizeof ( bspFile_t ) );
	Com_Memset( bsp, 0, sizeof ( bspFile_t ) );

    //
    // count and alloc
    //
	int numEdges = GetLumpElements( &header, LUMP_EDGES, sizeof ( realDedge_t ) );
	medge_t *edges = malloc( numEdges * sizeof ( *edges ) );

	int numSurfEdges = GetLumpElements( &header, LUMP_SURFEDGES, sizeof( int ) );
	int *surfEdges = malloc( numSurfEdges * sizeof( int ) );

	int lightingLength = GetLumpElements( &header, LUMP_LIGHTING, sizeof( byte ) );
	byte *lightmapData = NULL;
	if ( lightingLength > 0 )
	{
		lightmapData = malloc( lightingLength * sizeof( byte) );
	}

	int numTexInfo = GetLumpElements( &header, LUMP_TEXINFO, sizeof( realDtexinfo_t ) );
	mtexinfo_t *texInfo = malloc( numTexInfo * sizeof( *texInfo ) );

	int numExtendedSurfInfo = GetLumpElements( &header, LUMP_EXTSURFINFO, sizeof( float ) );

	int numFaces = GetLumpElements( &header, LUMP_FACES, sizeof( realDface_t ) );
	msurface_t *faces = malloc( numFaces * sizeof( *faces ) );

	int numMarkSurfaces = GetLumpElements( &header, LUMP_LEAFFACES, sizeof(unsigned short) );
	msurface_t **markSurfaces = malloc( numMarkSurfaces * sizeof(*markSurfaces) );

	int visibilityLength = GetLumpElements( &header, LUMP_VISIBILITY, 1 );
	realDvis_t *visibility = NULL;
	if ( visibilityLength > 0 )
	{
		visibility = malloc( visibilityLength );
	}

	int numLeafs = GetLumpElements( &header, LUMP_LEAFS, sizeof( realDleaf_t ) );
	mleaf_t *leafs = malloc( numLeafs * sizeof( *leafs ) );

	int numLeafBrushes = GetLumpElements( &header, LUMP_LEAFBRUSHES, sizeof( unsigned short ) );
	unsigned short *leafBrushes = malloc( numLeafBrushes * sizeof( *leafBrushes ) );

	int numBrushes = GetLumpElements( &header, LUMP_BRUSHES, sizeof( realDbrush_t ) );
	mbrush_t *brushes = malloc( numBrushes * sizeof( *brushes ) );

	int numBrushSides = GetLumpElements( &header, LUMP_BRUSHSIDES, sizeof(realDbrushside_t ) );


	int numNodes = GetLumpElements( &header, LUMP_NODES, sizeof ( realDnode_t ) );
	mnode_t *nodes = malloc( numNodes * sizeof ( *nodes ) );

	bsp->numSubmodels = GetLumpElements( &header, LUMP_MODELS, sizeof( realDmodel_t ) );
	bsp->submodels = malloc( bsp->numSubmodels * sizeof( *bsp->submodels ) );

	bsp->shaderStringLength = 0;
	bsp->shaderString = NULL;

	skybox_t skybox;
	ConvertEntityString( mapname, bsp, data, &header, &skybox );

	int		numFogs = 0;
	#define MAX_FOGS 1024
	mfog_t	fogs[MAX_FOGS];

	int numVerts = GetLumpElements( &header, LUMP_VERTEXES, sizeof ( realDvertex_t ) );
	float *verts = malloc( numVerts * sizeof( *verts ) * 3 );

	//
	// copy and swap and convert data
	//
	{
		realDvertex_t *in = GetLump( &header, data, LUMP_VERTEXES );
		float *out = verts;

		for ( i = 0; i < numVerts; i++, in++, out += 3 ) {
			for (j = 0; j < 3; j++ ) {
				out[j] = LittleFloat (in->point[j]);
			}
		}
	}

	// edges
	{
		realDedge_t *in = GetLump( &header, data, LUMP_EDGES );
		medge_t *out = edges;

		for ( i=0 ; i<numEdges ; i++, in++, out++)
		{
			out->v[0] = (unsigned short)LittleShort(in->v[0]);
			out->v[1] = (unsigned short)LittleShort(in->v[1]);
		}
	}
	// surfedges
	{
		int *in = GetLump( &header, data, LUMP_SURFEDGES );
		int *out = surfEdges;

		for ( i = 0; i < numSurfEdges; i++)
		{
			out[i] = LittleLong(in[i]);
		}
	}

	// lighting
	if ( lightingLength )
	{
		byte *in = GetLump( &header, data, LUMP_LIGHTING );
		memcpy (lightmapData, in, lightingLength);
	}

	// planes
	bsp->numPlanes = GetLumpElements( &header, LUMP_PLANES, sizeof ( realDplane_t ) ) * 2;
	bsp->planes = malloc( bsp->numPlanes * sizeof ( *bsp->planes ) );
	{
		realDplane_t *in = GetLump( &header, data, LUMP_PLANES );
		dplane_t *out = bsp->planes;

		k = bsp->numPlanes / 2;
		for ( i = 0; i < k; i++, in++) {
			for (j=0 ; j<3 ; j++) {
				out[i].normal[j] = LittleFloat (in->normal[j]);
				out[k + i].normal[j] = -out[i].normal[j];
			}

			out[i].dist = LittleFloat (in->dist);
			out[k + i].dist = out[i].dist;
		}
	}

	// textures
	ProcessTexInfo( bsp, data, &header, texInfo, numTexInfo );

	// NOTE: for JKA only, add an extra "outside" shader
	int outsideShaderNum = numTexInfo - 1;
	int maxShaders = numTexInfo + 1; // outside
	int numShaders = 0;
	bsp->shaders = malloc( maxShaders * sizeof( *bsp->shaders ) );
	for (i = 0; i < numTexInfo; i++)
	{
		mtexinfo_t *texinfo = &texInfo[i];

		dshader_t *out = &bsp->shaders[numShaders];
		texinfo->shaderNum = numShaders;
		numShaders++;

#if 0
		// print out flags
		Com_Printf("%s ", texinfo->image->name );
#define PRINTFLAG(f) do { if (texinfo->flags & (f)) Com_Printf( "%s ", #f ); } while (0)
		PRINTFLAG(SURF_LIGHT);
		PRINTFLAG(SURF_FULLBRIGHT);
		PRINTFLAG(SURF_SKY);
		PRINTFLAG(SURF_WARP);
		PRINTFLAG(SURF_TRANS33);
		PRINTFLAG(SURF_TRANS66);
		PRINTFLAG(SURF_FLOWING);
		PRINTFLAG(SURF_NODRAW);
		PRINTFLAG(SURF_HINT);
		PRINTFLAG(SURF_SKIP);
		PRINTFLAG(SURF_WOOD);
		PRINTFLAG(SURF_METAL);
		PRINTFLAG(SURF_STONE);
		PRINTFLAG(SURF_GLASS);
		PRINTFLAG(SURF_ICE);
		PRINTFLAG(SURF_SNOW);
		PRINTFLAG(SURF_MIRROR);
		PRINTFLAG(SURF_TRANSTHING);
		PRINTFLAG(SURF_ALPHACHAN);
		PRINTFLAG(SURF_MIDTEXTURE);
		PRINTFLAG(SURF_PUDDLE);
		PRINTFLAG(SURF_SURGE);
		PRINTFLAG(SURF_BIGSURGE);
		PRINTFLAG(SURF_BULLETLIGHT);
		PRINTFLAG(SURF_FOGPLANE);
		PRINTFLAG(SURF_SAND);
		Com_Printf("\n");
#undef PRINTFLAG
#endif
		// special names
		if (texinfo->flags & SURF_SKY)
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_SKY | Q3_SURF_NOMARKS | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_PLAYERCLIP;
			}
			else {
				out->surfaceFlags = JKA_SURF_SKY | JKA_SURF_NOMARKS | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_SOLID;
			}

			sprintf( out->shader, "textures/%s/sky", mapname );
		}
		else if (StringEndsWith(texinfo->image->name, "clip.tga"))
		{
			// on e1m1b we have fogplanes textured by clip... which COMPLETELY ignored by DK!
			if ( texinfo->flags & SURF_FOGPLANE ) {
				if ( !outputJKA ) {
					out->surfaceFlags = Q3_SURF_NODRAW | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
					out->contentFlags = Q3_CONTENTS_STRUCTURAL | Q3_CONTENTS_TRANSLUCENT;
				} else {
					out->surfaceFlags = JKA_SURF_NODRAW | JKA_SURF_NOIMPACT;
					out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
				}

				//Q_strncpyz(out->shader, "textures/common/nodraw", sizeof( out->shader ) );
				Q_strncpyz(out->shader, texinfo->image->name, sizeof( out->shader ) );

			} else {
				if ( !outputJKA ) {
					out->surfaceFlags = Q3_SURF_NOLIGHTMAP | Q3_SURF_NOMARKS | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
					out->contentFlags = Q3_CONTENTS_PLAYERCLIP;
				} else {
					out->surfaceFlags = JKA_SURF_NOMARKS | JKA_SURF_NOIMPACT;
					out->contentFlags = JKA_CONTENTS_PLAYERCLIP;
				}

				//Q_strncpyz(out->shader, "textures/common/clip", sizeof( out->shader ) );
				Q_strncpyz(out->shader, texinfo->image->name, sizeof( out->shader ) );
			}
		}
		else if (StringEndsWith(texinfo->image->name, "trigger.tga"))
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NODRAW;
			} else {
				out->surfaceFlags = JKA_SURF_NODRAW;
			}
			out->contentFlags = 0;
			Q_strncpyz(out->shader, "textures/common/trigger", sizeof( out->shader ) );
		}
		else if (StringEndsWith(texinfo->image->name, "hint.tga")) {
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NODRAW | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_STRUCTURAL | Q3_CONTENTS_TRANSLUCENT;
			} else {
				out->surfaceFlags = JKA_SURF_NODRAW | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
			}
			Q_strncpyz( out->shader, "textures/common/hint" , sizeof( out->shader ) );
		}
		else
		{
			// Com_Printf( "%s\n", texinfo->image->name );

			if ( !outputJKA ) {
				out->surfaceFlags = DK_SurfaceFlagsToQuake3( texinfo->flags );
			} else {
				out->surfaceFlags = DK_SurfaceFlagsToJKA( texinfo->flags );
			}

			if ( texinfo->flags & SURF_WARP ) {
				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_TRANSLUCENT | Q3_CONTENTS_WATER;
				} else {
					out->contentFlags = JKA_CONTENTS_TRANSLUCENT | JKA_CONTENTS_WATER;
				}
			}
			else if ( ( texinfo->flags & SURF_TRANS33 ) || ( texinfo->flags & SURF_TRANS66 ) ) {
				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_TRANSLUCENT;
				} else {
					out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
				}
			}
			else {
				// by default, these should be the same as in the wal file
				// or if overridden, the contents of the face? or brush?

				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_SOLID; // TODO: need some mapping
				} else {
					out->contentFlags = JKA_CONTENTS_SOLID;
				}
			}

			char customName[MAX_QPATH];
			/*if ( VIEWFOGS && (texinfo->flags & SURF_FOGPLANE) ) {
				sprintf(customName, "texinfo_%d", i);
				Q_strncpyz(out->shader, customName, sizeof( out->shader ) );
			}
			else*/
			{
				Q_strncpyz(out->shader, texinfo->image->name, sizeof( out->shader ) );
			}
		}
	}
	Q_strncpyz( bsp->shaders[numShaders].shader, "textures/system/outside", sizeof( bsp->shaders[numShaders].shader ) );
	bsp->shaders[numShaders].contentFlags |= JKA_CONTENTS_OUTSIDE;
	bsp->shaders[numShaders].surfaceFlags = 0;
	numShaders++;
	bsp->numShaders = numShaders;

	// brushsides
	bsp->numBrushSides = numBrushSides;
	bsp->brushSides = malloc( bsp->numBrushSides * sizeof( *bsp->brushSides ) );
	{
		dbrushside_t	*out;
		realDbrushside_t 	*in = GetLump( &header, data, LUMP_BRUSHSIDES );

		// need to save space for box planes
		if (numBrushSides > MAX_MAP_BRUSHSIDES)
			Com_Error (ERR_DROP, "Map has too many planes");
		out = bsp->brushSides;
		for ( i=0 ; i<numBrushSides ; i++, in++, out++)
		{
			//if (i >= 171 && i <= 174 || i == 262 || i == 561)
			//{
			//	Com_Printf("found the fog brush\n");
			//}
			out->planeNum = LittleShort (in->planenum);
			j = LittleShort (in->texinfo);
			if (j < 0) {
				j = 0; // HACK
				//Com_Error(ERR_DROP, "oops");
			}
			if (j >= numTexInfo)
				Com_Error (ERR_DROP, "Bad brushside texinfo");
			out->shaderNum = texInfo[j].shaderNum;
			out->surfaceNum = j;
		}
	}

	// brushes
	bsp->numBrushes = numBrushes;
	bsp->brushes = malloc( bsp->numBrushes * sizeof( *bsp->brushes ) );
	{
		realDbrush_t	*in = GetLump( &header, data, LUMP_BRUSHES );
		dbrush_t	*out;

		if (numBrushes > MAX_MAP_BRUSHES)
			Com_Error (ERR_DROP, "Map has too many brushes");
		out = bsp->brushes;
		for (i=0 ; i<numBrushes ; i++, out++, in++)
		{
			out->firstSide = LittleLong(in->firstside);
			out->numSides = LittleLong(in->numsides);
			
			/*if (i >= 171 && i <= 174 || i == 262 || i == 561)
			{
				Com_Printf("found the fog brush\n");

				for (j = 0; j < out->numSides; j++) {
					dbrushside_t *side = &bsp->brushSides[ out->firstSide + j ];
				}
			}*/
			// j = DK_ContentsToQuake3 ( LittleLong(in->contents) );
			
			out->shaderNum = bsp->brushSides[out->firstSide].shaderNum;
		}
	}

	// faces
	ProcessFaces( bsp, data, &header, faces, numFaces, texInfo, numTexInfo, verts, numVerts, lightmapData, lightingLength, edges, numEdges, surfEdges, numSurfEdges );

	// marksurfaces
	bsp->numLeafSurfaces = numMarkSurfaces;
	bsp->leafSurfaces = malloc( bsp->numLeafSurfaces * sizeof( *bsp->leafSurfaces ) );
	{
		unsigned short *in = GetLump( &header, data, LUMP_LEAFFACES );
		int *out = bsp->leafSurfaces;

		for ( i=0 ; i< numMarkSurfaces ; i++)
		{
			j = LittleShort(in[i]);
			if (j < 0 ||  j >= numFaces)
				Com_Error (ERR_DROP, "ParseMarksurfaces: bad surface number");
			out[i] = j;
		}
	}

	// visibility
	ConvertVisibility( bsp, data, &header, visibility, visibilityLength );

	// leafs
	{
		realDleaf_t *in = GetLump( &header, data, LUMP_LEAFS );
		mleaf_t *out = leafs;

		for ( i=0 ; i<numLeafs ; i++, in++, out++)
		{
			int	contents;

			out->mins[0] = LittleShort( in->mins[0] );
			out->mins[1] = LittleShort( in->mins[1] );
			out->mins[2] = LittleShort( in->mins[2] );

			out->maxs[0] = LittleShort( in->maxs[0] );
			out->maxs[1] = LittleShort( in->maxs[1] );
			out->maxs[2] = LittleShort( in->maxs[2] );

			if( ( out->mins[1] > out->maxs[1] ) || 
				( out->mins[0] > out->maxs[0] ) || 
				( out->mins[2] > out->maxs[2] ) ){
				printf ("Flipped bounds on node!");
			}

			contents = LittleLong(in->contents);
			// TODO: runtime switch for JKA
			out->contents = DK_ContentsToJKA( contents );
			//out->contents = DK_ContentsToQuake3( contents );

			out->cluster = LittleShort(in->cluster);
			out->area = LittleShort(in->area);

			out->firstmarksurface =  LittleShort(in->firstleafface);
			out->nummarksurfaces = LittleShort(in->numleaffaces);

			out->firstleafbrush = LittleShort(in->firstleafbrush);
			out->numleafbrushes = LittleShort(in->numleafbrushes);

			// SCG[4/17/99]: Added for volumetric fog
			if ( contents & CONTENTS_FOG )
			{
				int	brushNum = LittleLong( in->brushnum );
				int	setShader = 0;

				//Com_Printf("fog brush num %d\n", brushNum);
				for ( k = 0; k < numFogs; k++ )
				{
					if ( fogs[k].brushNum == brushNum )
						break;
				}
				if ( k == numFogs )
				{
					if ( numFogs == MAX_FOGS )
						Com_Error( ERR_DROP, "MAX_FOGS on the map" );
					fogs[numFogs].brushNum = brushNum;

					//Q_strncpyz( fogs[numFogs].shader, "textures/sfx/fog_timdm1", sizeof( fogs[numFogs].shader ) );
					sprintf( fogs[numFogs].shader, "fogs/%s_%d", mapname, k );

					fogs[numFogs].visibleSide = 5; // 5 looks better than -1... but tesselation artifacts exist.
					numFogs++;
					setShader = 1;
				}

				// volumeNum = R_FogVolumeForBrush( in->brushnum );
				if (VIEWFOGS)
				{
					/*for (j=0 ; j<out->nummarksurfaces ; j++)
					{
						msurface_t *surf = &faces[bsp->leafSurfaces[out->firstmarksurface + j]];

						surf->texinfo->flags |= SURF_FOGPLANE;
					}*/
				}
				else
				{
					for (j=0 ; j<out->nummarksurfaces ; j++)
					{
						msurface_t *surf = &faces[bsp->leafSurfaces[out->firstmarksurface + j]];

						// looks like we need to clip each face
						// against the brush: what's inside the brush is marked
						// with the fog num.
						// we can leave the non-fogged surface as-is. but the fogged
						// surface can be appended into the surfaces list.
						// we'd still need to fix things up...

						//surf->flags |= SURF_DRAWFOG;
						//surf->brushnum = volumeNum;
						//r_fogvolumes[volumeNum].hull.Add( ( void * ) surf );
						surf->fogNum = k;
						if ( setShader && surf->texinfo->flags & SURF_FOGPLANE )
						{
							fogs[k].texinfo = surf->texinfo;
							setShader = 0;
						}
					}
				}
			}
		}
	}

	// leafbrushes
	{
		unsigned short	*out;
		unsigned short 	*in = GetLump( &header, data, LUMP_LEAFBRUSHES );

		if (numLeafBrushes < 1)
			Com_Error (ERR_DROP, "Map with no planes");
		// need to save space for box planes
		if (numLeafBrushes > MAX_MAP_LEAFBRUSHES)
			Com_Error (ERR_DROP, "Map has too many leafbrushes");

		out = leafBrushes;
		for ( i=0 ; i<numLeafBrushes ; i++, in++, out++)
			*out = LittleShort (*in);
	}

	// nodes
	bsp->numNodes = numNodes;
	bsp->nodes = malloc( bsp->numNodes * sizeof( *bsp->nodes ) );
	{
		realDnode_t		*in = GetLump( &header, data, LUMP_NODES );
		dnode_t 	*out = bsp->nodes;

		for ( i=0 ; i<numNodes ; i++, in++, out++)
		{
			out->mins[0] = LittleShort( in->mins[0] );
			out->maxs[0] = LittleShort( in->maxs[0] );

			out->mins[1] = LittleShort( in->mins[1] );
			out->maxs[1] = LittleShort( in->maxs[1] );

			out->mins[2] = LittleShort( in->mins[2] );
			out->maxs[2] = LittleShort( in->maxs[2] );

			if( ( out->mins[1] > out->maxs[1] ) || 
				( out->mins[0] > out->maxs[0] ) || 
				( out->mins[2] > out->maxs[2] ) ){
				printf ("Flipped bounds on node!");
			}

			out->planeNum = LittleLong(in->planenum);

			out->children[0] = LittleLong(in->children[0]);
			out->children[1] = LittleLong(in->children[1]);
		}
	}

	bsp->numLeafs = numLeafs;
	bsp->leafs = malloc( bsp->numLeafs * sizeof( *bsp->leafs ) );
	for (i = 0; i < numLeafs; i++) {
		mleaf_t *in = &leafs[i];
		dleaf_t *out = &bsp->leafs[i];

		out->cluster = in->cluster;
		out->area = in->area;

		//Com_Printf("%d\n", in->contents);

		out->mins[0] = in->mins[0];
		out->mins[1] = in->mins[1];
		out->mins[2] = in->mins[2];
		out->maxs[0] = in->maxs[0];
		out->maxs[1] = in->maxs[1];
		out->maxs[2] = in->maxs[2];

		out->firstLeafSurface = in->firstmarksurface;
		out->numLeafSurfaces = in->nummarksurfaces;

		out->firstLeafBrush = in->firstleafbrush;
		out->numLeafBrushes = in->numleafbrushes;
	}

	bsp->numLeafBrushes = numLeafBrushes;
	bsp->leafBrushes = malloc( bsp->numLeafBrushes * sizeof( *bsp->leafBrushes ) );
	for (i = 0; i < numLeafBrushes; i++)
	{
		bsp->leafBrushes[i] = leafBrushes[i];
	}

	ConvertSurfaces( bsp, faces, numFaces, texInfo, numTexInfo );

	bsp->defaultLightGridSize[0] = 1.0f;
	bsp->defaultLightGridSize[1] = 1.0f;
	bsp->defaultLightGridSize[2] = 1.0f;
	bsp->lightGridData = NULL;
	bsp->numGridPoints = 0;

	{
		realDmodel_t	*in = GetLump( &header, data, LUMP_MODELS );
		dmodel_t	*out = bsp->submodels;

		for ( i=0 ; i<bsp->numSubmodels ; i++, in++, out++)
		{
			out->mins[0] = LittleFloat (in->mins[0]);
			out->mins[1] = LittleFloat (in->mins[1]);
			out->mins[2] = LittleFloat (in->mins[2]);

			out->maxs[0] = LittleFloat (in->maxs[0]);
			out->maxs[1] = LittleFloat (in->maxs[1]);
			out->maxs[2] = LittleFloat (in->maxs[2]);

			//out->origin[0] = LittleFloat (in->origin[0]);
			//out->origin[1] = LittleFloat (in->origin[1]);
			//out->origin[2] = LittleFloat (in->origin[2]);

			out->firstSurface = LittleLong (in->firstface);
			out->numSurfaces = LittleLong (in->numfaces);

			int headnode = LittleLong (in->headnode);

			// JKA: also we want to add CONTENTS_OUTSIDE for brushes if this brushmodel is used by effect_rain
			// FIXME: hardcoded for e1m1a
			// doesn't work because JKA looks for the worldmodel to have CONTENTS_OUTSIDE in some brushes..
			//if (i >= 5 && i <= 49) {
			//	MarkOutside( outsideShaderNum, bsp->brushes, bsp->brushSides, bsp->leafBrushes, bsp->nodes, bsp->leafs, 0, out );
			//}
			/*
			int	firstBrush, lastBrush;

			firstBrush = bsp->numBrushes, lastBrush = 0;
			// FIXME: does it work? doesn't seem to.
			FindSubmodelBrushRange( bsp->leafBrushes, bsp->nodes, bsp->leafs, headnode, &firstBrush, &lastBrush );

			if (firstBrush <= lastBrush)
			{
				out->firstBrush = firstBrush;
				out->numBrushes = lastBrush + 1 - firstBrush;
			}
			else*/
			{
				out->firstBrush = 0;
				out->numBrushes = 0;
			}
		}
	}

	if (VIEWFOGS)
	{
		bsp->numFogs = 0;
		bsp->fogs = NULL;
	}
	else
	{
		bsp->numFogs = numFogs;
		bsp->fogs = malloc( numFogs * sizeof( *bsp->fogs ) );
		{
			mfog_t	*in;
			dfog_t	*out;

			in = fogs;
			out = bsp->fogs;
			for ( i = 0; i < numFogs; i++, in++, out++ )
			{
				out->brushNum = in->brushNum;
				Q_strncpyz( out->shader, in->shader, sizeof( out->shader ) );
				out->visibleSide = in->visibleSide;
			}
		}
	}

	OutputTexInfoShaders( mapname, faces, numFaces, texInfo, numTexInfo, &skybox, fogs, numFogs, bsp );

	SkyBoxFree( &skybox );
	free(edges);
	free(surfEdges);
	free(texInfo);
	if (lightingLength > 0)
	{
		free(lightmapData);
	}
	free(markSurfaces);
	free(faces);
	if (visibilityLength > 0)
	{
		free(visibility);
	}
	free(leafs);
	free(leafBrushes);
	free(brushes);
	free(nodes);
	free(verts);
	//free(areas);
	//free(areaPortals);

	image_shutdown();

    return bsp;
}

/****************************************************
*/

bspFormat_t daikatanaBspFormat = {
	"Daikatana",
	IDBSPHEADER,
	BSPVERSION, // TODO: BSPVERSION_COLORLUMP, BSPVERSION_TEXINFOOLD, BSPVERSION_Q2?
	NULL,

	BSP_LoadDK,
};
