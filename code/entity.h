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

#ifndef __ENTITY_H__
#define __ENTITY_H__

typedef struct epair_s {
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct {
	epair_t		*epairs;
} entity_t;

char *copystring(const char *s);

//#define MAX_MAP_ENTITIES 8192
//extern	int			num_entities;
//extern	entity_t	entities[MAX_MAP_ENTITIES];

int     NumEntities();
entity_t *GetEmptyEntity();
entity_t *GetEntityNum(int entityNum);

void	ParseEntities( char *entityData, int entityDataSize );
void	UnparseEntities( char *entdata, int *entdatasize, int maxMapEntString );

void 	SetKeyValue( entity_t *ent, const char *key, const char *value );
void	SetKeyInteger( entity_t *ent, const char *key, int value );
void	SetKeyFloat( entity_t *ent, const char *key, float value );

void RenameKeys( entity_t *ent, int numPairs, epair_t *pairs );

const char 	*ValueForKey( const entity_t *ent, const char *key );
// will return "" if not present

void	AppendKeyValueMatching( entity_t *ent, const char *keyPrefix, const char *valuePrefix );

int		IntegerForKey( const entity_t *ent, const char *key );
float	FloatForKey( const entity_t *ent, const char *key );
void 	GetVectorForKey( const entity_t *ent, const char *key, float vec[3] );
void	SetVectorForKey( entity_t *ent, const char *key, float vec[3] );

epair_t *ParseEpair( void );

void	PrintEntity( const entity_t *ent );

void	FreeEntity( entity_t *ent );

#endif /* !__ENTITY_H__ */