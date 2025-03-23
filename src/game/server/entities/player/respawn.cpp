/***
 *
 * Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 * This product contains software technology licensed from Id
 * Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 * All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "cbase.h"

#define SF_PLAYER_RESPAWN_OUTSIDE ( 0 << 1 )
#define SF_PLAYER_RESPAWN_INSIDE  ( 1 << 1 )
#define SF_PLAYER_RESPAWN_ALIVE  ( 1 << 1 )

class CPlayerRespawn : public CPointEntity
{
    DECLARE_CLASS( CPlayerRespawn, CPointEntity );

public:
    bool Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
};

LINK_ENTITY_TO_CLASS( player_respawn, CPlayerRespawn );

bool CPlayerRespawn::Spawn()
{
    pev->solid = SOLID_NOT;

    SetModel( STRING( pev->model ) );

    return BaseClass::Spawn();
}

void CPlayerRespawn::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( IsLockedByMaster(pActivator) )
        return;

    FireTargets( STRING( pev->message ), nullptr, nullptr, USE_OFF );
    FireTargets( STRING( pev->netname ), nullptr, nullptr, USE_ON );

    if( !FBitSet( pev->spawnflags, ( SF_PLAYER_RESPAWN_INSIDE | SF_PLAYER_RESPAWN_OUTSIDE ) ) )
    {
        if( auto player = ToBasePlayer( pActivator ); player != nullptr )
        {
            if( player->IsAlive() && !FBitSet( pev->spawnflags, SF_PLAYER_RESPAWN_ALIVE ) )
                return;

            player->Spawn();
        }
    }
    else
    {
        for( auto player : UTIL_FindPlayers() )
        {
            if( player != nullptr && player->IsPlayer() )
            {
                if( FBitSet( pev->spawnflags, SF_PLAYER_RESPAWN_OUTSIDE ) && !Intersects( player )
                    || FBitSet( pev->spawnflags, SF_PLAYER_RESPAWN_INSIDE ) && Intersects( player )
                        || FBitSet( pev->spawnflags, ( SF_PLAYER_RESPAWN_INSIDE | SF_PLAYER_RESPAWN_OUTSIDE ) ) ) {
                            if( !player->IsAlive() || FBitSet( pev->spawnflags, SF_PLAYER_RESPAWN_ALIVE ) )
                                player->Spawn();
                } else {
                    FireTargets( STRING( pev->target ), player, this, USE_TOGGLE );
                }
            }
        }
    }
}
