/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "entity.h"
#include "sekai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MAP_ENTITIES 8192
int			num_entities;
entity_t	entities[MAX_MAP_ENTITIES];

#define	MAX_KEY		32
#define	MAX_VALUE	1024

/* ****** ****** */

char *copystring(const char *s)
{
	char	*b;
	b = malloc(strlen(s)+1);
	strcpy (b, s);
	return b;
}

// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src ) {
	int		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_DROP, "Q_strcat: already overflowed" );
	}
	dest += l1;
	size -= l1;
	Q_strncpyz( dest, src, size );
}

/*
================
Q_filelength
================
*/
int Q_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

void SafeRead (FILE *f, void *buffer, int count)
{
	if ( (int)fread (buffer, 1, count, f) != count)
		Com_Error (ERR_DROP, "File read failure");
}


/*
==============
LoadFile
==============
*/
int LoadFile (const char *filename, void **bufferptr)
{
	FILE	*f;
	int    length;
	void    *buffer;

  *bufferptr = NULL;
  
  if (filename == NULL || strlen(filename) == 0)
  {
    return -1;
  }

	f = fopen (filename, "rb");
	if (!f)
	{
		return -1;
	}
	length = Q_filelength (f);
	buffer = malloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}

/* ****** ****** */

#define	MAXTOKEN	1024

typedef struct
{
	char	filename[1024];
	char    *buffer,*script_p,*end_p;
	int     line;
} script_t;

#define	MAX_INCLUDES	8
script_t	scriptstack[MAX_INCLUDES];
script_t	*script;
int			scriptline;

char    token[MAXTOKEN];
qboolean endofscript;
qboolean tokenready;                     // only qtrue if UnGetToken was just called

/*
==============
AddScriptToStack
==============
*/
void AddScriptToStack( const char *filename ) {
	int            size;

	script++;
	if (script == &scriptstack[MAX_INCLUDES])
		Com_Error (ERR_DROP, "script file exceeded MAX_INCLUDES");
	strcpy (script->filename, /*ExpandPath*/ (filename) );

	size = LoadFile (script->filename, (void **)&script->buffer);

	printf ("entering %s\n", script->filename);

	script->line = 1;

	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
}


/*
==============
LoadScriptFile
==============
*/
void LoadScriptFile( const char *filename ) {
	script = scriptstack;
	AddScriptToStack (filename);

	endofscript = qfalse;
	tokenready = qfalse;
}

/*
==============
ParseFromMemory
==============
*/
void ParseFromMemory (char *buffer, int size)
{
	script = scriptstack;
	script++;
	if (script == &scriptstack[MAX_INCLUDES])
		Com_Error (ERR_DROP, "script file exceeded MAX_INCLUDES");
	strcpy (script->filename, "memory buffer" );

	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	endofscript = qfalse;
	tokenready = qfalse;
}

/*
==============
UnGetToken

Signals that the current token was not used, and should be reported
for the next GetToken.  Note that

GetToken (qtrue);
UnGetToken ();
GetToken (qfalse);

could cross a line boundary.
==============
*/
void UnGetToken (void)
{
	tokenready = qtrue;
}

qboolean GetToken (qboolean crossline);

qboolean EndOfScript (qboolean crossline)
{
	if (!crossline)
		Com_Error (ERR_DROP, "Line %i is incomplete\n",scriptline);

	if (!strcmp (script->filename, "memory buffer"))
	{
		endofscript = qtrue;
		return qfalse;
	}

	free (script->buffer);
	if (script == scriptstack+1)
	{
		endofscript = qtrue;
		return qfalse;
	}
	script--;
	scriptline = script->line;
	printf ("returning to %s\n", script->filename);
	return GetToken (crossline);
}

/*
==============
GetToken
==============
*/
qboolean GetToken (qboolean crossline)
{
	char    *token_p;

	if (tokenready)                         // is a token allready waiting?
	{
		tokenready = qfalse;
		return qtrue;
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (crossline);

//
// skip space
//
skipspace:
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return EndOfScript (crossline);
		if (*script->script_p++ == '\n')
		{
			if (!crossline)
				Com_Error (ERR_DROP, "Line %i is incomplete\n",scriptline);
			scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (crossline);

	// ; # // comments
	if (*script->script_p == ';' || *script->script_p == '#'
		|| ( script->script_p[0] == '/' && script->script_p[1] == '/') )
	{
		if (!crossline)
			Com_Error (ERR_DROP, "Line %i is incomplete\n",scriptline);
		while (*script->script_p++ != '\n')
			if (script->script_p >= script->end_p)
				return EndOfScript (crossline);
		scriptline = script->line++;
		goto skipspace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*')
	{
		if (!crossline)
			Com_Error (ERR_DROP, "Line %i is incomplete\n",scriptline);
		script->script_p+=2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/')
		{
			if ( *script->script_p == '\n' ) {
				scriptline = script->line++;
			}
			script->script_p++;
			if (script->script_p >= script->end_p)
				return EndOfScript (crossline);
		}
		script->script_p += 2;
		goto skipspace;
	}

//
// copy token
//
	token_p = token;

	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
			if (token_p == &token[MAXTOKEN])
				Com_Error (ERR_DROP, "Token too large on line %i\n",scriptline);
		}
		script->script_p++;
	}
	else	// regular token
	while ( *script->script_p > 32 && *script->script_p != ';')
	{
		*token_p++ = *script->script_p++;
		if (script->script_p == script->end_p)
			break;
		if (token_p == &token[MAXTOKEN])
			Com_Error (ERR_DROP, "Token too large on line %i\n",scriptline);
	}

	*token_p = 0;

	if (!strcmp (token, "$include"))
	{
		GetToken (qfalse);
		AddScriptToStack (token);
		return GetToken (crossline);
	}

	return qtrue;
}


/*
==============
TokenAvailable

Returns qtrue if there is another token on the line
==============
*/
qboolean TokenAvailable (void) {
	int		oldLine;
	qboolean	r;

	oldLine = script->line;
	r = GetToken( qtrue );
	if ( !r ) {
		return qfalse;
	}
	UnGetToken();
	if ( oldLine == script->line ) {
		return qtrue;
	}
	return qfalse;
}

/* ****** ****** */

int     NumEntities()
{
    return num_entities;
}

entity_t *GetEmptyEntity() {
	for ( int i = 0; i < num_entities; i++ ) {
		if ( !entities[i].epairs )
			return &entities[i];
	}
	if ( num_entities == MAX_MAP_ENTITIES ) {
		Com_Error( ERR_DROP, "MAX_MAP_ENTITIES" );
	}
	entity_t *ent = &entities[num_entities++];
	ent->epairs = NULL;
	return ent;
}

entity_t *GetEntityNum( int entityNum ) {
    if ( entityNum < 0 || entityNum > num_entities ) {
        Com_Error( ERR_DROP, "Entity out of bounds %d\n", entityNum );
    }
    return &entities[entityNum];
}

void StripTrailing( char *e ) {
	char	*s;

	s = e + strlen(e)-1;
	while (s >= e && *s <= 32)
	{
		*s = 0;
		s--;
	}
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair( void ) {
	epair_t	*e;

	e = malloc( sizeof(epair_t) );
	memset( e, 0, sizeof(epair_t) );
	
	if ( strlen(token) >= MAX_KEY-1 ) {
		Com_Error (ERR_DROP, "ParseEpair: token too long");
	}
	e->key = copystring( token );
	GetToken( qfalse );
	if ( strlen(token) >= MAX_VALUE-1 ) {
		Com_Error (ERR_DROP, "ParseEpair: token too long");
	}
	e->value = copystring( token );

	// strip trailing spaces that sometimes get accidentally
	// added in the editor
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}


/*
================
ParseEntity
================
*/
qboolean	ParseEntity( void ) {
	epair_t		*e;
	entity_t	*mapent;

	if ( !GetToken (qtrue) ) {
		return qfalse;
	}

	if ( strcmp (token, "{") ) {
		Com_Error (ERR_DROP, "ParseEntity: { not found");
	}
	if ( num_entities == MAX_MAP_ENTITIES ) {
		Com_Error (ERR_DROP, "num_entities == MAX_MAP_ENTITIES");
	}
	mapent = &entities[num_entities];
	num_entities++;

	do {
		if ( !GetToken (qtrue) ) {
			Com_Error (ERR_DROP, "ParseEntity: EOF without closing brace");
		}
		if ( !strcmp (token, "}") ) {
			break;
		}
		e = ParseEpair ();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (1);
	
	return qtrue;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities( char *entityData, int entityDataSize ) {
	num_entities = 0;
	ParseFromMemory( entityData, entityDataSize );

	while ( ParseEntity () ) {
	}	
}


/*
================
UnparseEntities

Generates the dentdata string from all the entities
This allows the utilities to add or remove key/value pairs
to the data created by the map editor.
================
*/
void UnparseEntities( char *entdata, int *entdatasize, int maxMapEntString ) {
	char	*buf, *end;
	epair_t	*ep;
	char	line[2048];
	int		i;
	char	key[1024], value[1024];

	buf = entdata;
	end = buf;
	*end = 0;
	
	for (i=0 ; i<num_entities ; i++) {
		ep = entities[i].epairs;
		if ( !ep ) {
			continue;	// ent got removed
		}
		
		strcat (end,"{\n");
		end += 2;
				
		for ( ep = entities[i].epairs ; ep ; ep=ep->next ) {
			strcpy (key, ep->key);
			StripTrailing (key);
			strcpy (value, ep->value);
			StripTrailing (value);
				
			sprintf (line, "\"%s\" \"%s\"\n", key, value);
			strcat (end, line);
			end += strlen(line);
		}
		strcat (end,"}\n");
		end += 2;

		if (end > buf + maxMapEntString) {
			Com_Error (ERR_DROP, "Entity text too long");
		}
	}
	*entdatasize = end - buf + 1;
}

void PrintEntity( const entity_t *ent ) {
	epair_t	*ep;

	ep = ent->epairs;
	if ( !ep ) {
		return;
	}
	
	printf ("------- entity %p -------\n", ent);
	for (ep=ent->epairs ; ep ; ep=ep->next) {
		printf( "%s = %s\n", ep->key, ep->value );
	}

}

void FreeEntity( entity_t *ent ) {
	epair_t	*ep;
	epair_t *next;
	
	for (ep=ent->epairs ; ep ; ep=next) {
		next = ep->next;
		free(ep->key);
		free(ep->value);
		free(ep);
	}
	ent->epairs = NULL;
}

void 	SetKeyValue( entity_t *ent, const char *key, const char *value ) {
	epair_t	*ep;
	
	for ( ep=ent->epairs ; ep ; ep=ep->next ) {
		if ( !strcmp (ep->key, key) ) {
			free (ep->value);
			ep->value = copystring(value);
			return;
		}
	}
	ep = malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring(key);
	ep->value = copystring(value);
}

void SetKeyInteger( entity_t *ent, const char *key, int integer ) {
	char	value[MAX_VALUE];

	sprintf( value, "%d", integer );
	SetKeyValue( ent, key, value );
}

void SetKeyFloat( entity_t *ent, const char *key, float floating ) {
	char	value[MAX_VALUE];

	sprintf( value, "%f", floating );
	SetKeyValue( ent, key, value );
}

void RenameKeys( entity_t *ent, int numPairs, epair_t *pairs )
{
	epair_t	*ep, *kv;
	int		i;
	
	for ( ep=ent->epairs ; ep ; ep=ep->next ) {
		for ( i = 0, kv = pairs ; i < numPairs; i++, kv++ ) {
			if ( !strcmp (ep->key, kv->key) ) {
				free (ep->key);
				ep->key = copystring(kv->value);
				return;
			}
		}
	}
}

const char 	*ValueForKey( const entity_t *ent, const char *key ) {
	epair_t	*ep;
	
	for (ep=ent->epairs ; ep ; ep=ep->next) {
		if (!strcmp (ep->key, key) ) {
			return ep->value;
		}
	}
	return "";
}

void AppendKeyValueMatching( entity_t *ent, const char *keyPrefix, const char *valuePrefix ) {
	epair_t	*ep;
	int n = strlen( keyPrefix );
	char newstring[MAX_QPATH];
	
	for (ep=ent->epairs ; ep ; ep=ep->next) {
		if (strncmp (ep->key, keyPrefix, n ) ) {
			continue;
		}

		newstring[0] = '\0';
		Q_strcat( newstring, sizeof( newstring ), valuePrefix );
		Q_strcat( newstring, sizeof( newstring ), ep->value );

		free (ep->value);
		ep->value = copystring(newstring);
	}
}

int		IntegerForKey( const entity_t *ent, const char *key ) {
	const char	*k;

	k = ValueForKey( ent, key );
	return atoi(k);
}

float	FloatForKey( const entity_t *ent, const char *key ) {
	const char	*k;
	
	k = ValueForKey( ent, key );
	return atof(k);
}

void 	GetVectorForKey( const entity_t *ent, const char *key, float vec[3] ) {
	const char	*k;
	double	v1, v2, v3;

	k = ValueForKey (ent, key);

	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;
	sscanf (k, "%lf %lf %lf", &v1, &v2, &v3);
	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;
}

void SetVectorForKey( entity_t *ent, const char *key, float vec[3] ) {
	char	value[MAX_VALUE];

	sprintf( value, "%lf %lf %lf", vec[0], vec[1], vec[2] );
	SetKeyValue( ent, key, value );
}
