/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *    This product contains software technology licensed from Id
 *    Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *    All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "cbase.h"
#include "player_start.h"

LINK_ENTITY_TO_CLASS( info_player_start, CPlayerSpawnPoint );
LINK_ENTITY_TO_CLASS( info_player_start_mp, CPlayerSpawnPoint );

BEGIN_DATAMAP( CPlayerSpawnPoint )
    DEFINE_FIELD( m_flOffSet, FIELD_FLOAT ),
    DEFINE_FIELD( m_PlayerFilterType, FIELD_INTEGER ),
    DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
    DEFINE_ARRAY( m_iKey, FIELD_STRING, 16 ),
    DEFINE_ARRAY( m_iValue, FIELD_STRING, 16 ),
END_DATAMAP();

bool CPlayerSpawnPoint::KeyValue( KeyValueData* pkvd )
{
    if( strncmp( pkvd->szKeyName, "-", 1 ) == 0 )
    {
        if( m_cTargets > MAX_SPAWNPOINT_KEYVALUES )
        {
            CBaseEntity::Logger->error( "Reached {} keyvalue limit on {}::{} can not set key {}",
                MAX_SPAWNPOINT_KEYVALUES, STRING(pev->classname), STRING(pev->targetname), pkvd->szKeyName );
            return false;
        }

        char temp[256];

        UTIL_StripToken(pkvd->szKeyName + 1, temp);
        m_iKey[m_cTargets] = ALLOC_STRING(temp);

        UTIL_StripToken(pkvd->szValue + 1, temp);
        m_iValue[m_cTargets] = ALLOC_STRING(temp);

        ++m_cTargets;
    }
    else if( FStrEq( pkvd->szKeyName, "m_flOffSet" ) )
    {
        m_flOffSet = std::max( 0, atoi( pkvd->szValue ) );
    }
    else if( FStrEq( pkvd->szKeyName, "m_PlayerFilterName" ) )
    {
        m_PlayerFilterName = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_PlayerFilterType" ) )
    {
        m_PlayerFilterType = std::min( 1, std::max( 0, atoi( pkvd->szValue ) ) );
    }
    else
    {
        return BaseClass::KeyValue(pkvd);
    }
    return true;
}

void CPlayerSpawnPoint::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    switch( useType )
    {
        case USE_OFF:
            SetBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
        break;
        case USE_ON:
            ClearBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
        break;
        case USE_SET:
            if( InitialState )
                SetBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
            else ClearBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
        break;
        default:
            if( !FBitSet( pev->spawnflags, SF_SPAWNPOINT_STARTOFF ) )
                SetBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
            else ClearBits( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
        break;
    }
}

bool CPlayerSpawnPoint::Spawn()
{
    if( g_pGameRules->IsMultiplayer() && FStrEq( STRING( pev->classname ), "info_player_start" ) )
        pev->classname = MAKE_STRING( "info_player_start_mp" );

    InitialState = FBitSet( pev->spawnflags, SF_SPAWNPOINT_STARTOFF );
    return true;
}

void CPlayerSpawnPoint::SetPosition( Vector& origin )
{
    int offset = ( m_flOffSet > 0 ? m_flOffSet : g_cfg.GetValue<int>( "plr_spawn_offset"sv, 512 ) );

    if( offset <= 32 )
    {
        origin = pev->origin + Vector( 0, 0, 1 );
    }
    else
    {
        TraceResult tr;
        UTIL_MakeVectors( Vector( 0, RANDOM_FLOAT( 0.0f, 360.0f ), 0 ) );
        UTIL_TraceLine( origin, origin + gpGlobals->v_forward * offset, ignore_monsters, edict(), &tr );
        origin = tr.vecEndPos + Vector( 0, 0, 1 );
    }
}

bool CPlayerSpawnPoint::CanPlayerSpawn( CBasePlayer* player )
{
    if( player != nullptr && !IsLockedByMaster( player ) && !( pev->spawnflags & 1 ) == 0 )
    {
        if( !FStringNull( m_PlayerFilterName ) )
        {
            bool is_named_eq = FStrEq( STRING( m_PlayerFilterName ), STRING( player->pev->targetname ) );

            if( ( m_PlayerFilterType == 0 && is_named_eq ) || ( m_PlayerFilterType == 1 && !is_named_eq ) )
                return false;
        }
        return true;
    }

    return false;
}

void CPlayerSpawnPoint::SpawnPlayer( CBasePlayer* player )
{
    if( player != nullptr )
    {
        SetPosition( player->pev->origin );

        player->pev->v_angle = g_vecZero;
        player->pev->velocity = g_vecZero;
        player->pev->angles = pev->angles;
        player->pev->punchangle = g_vecZero;
        player->pev->fixangle = FIXANGLE_ABSOLUTE;

        UTIL_InitializeKeyValues( player, m_iKey, m_iValue, m_cTargets );

        if( !FStringNull( pev->target ) )
        {
            FireTargets( STRING( pev->target ), player, this, USE_TOGGLE, UseValue( player->pev->origin ) );
        }
    }
}

CPlayerSpawnPoint* FindBestPlayerSpawn( CBasePlayer* player, const char* spawn_name )
{
    CBaseEntity* entity = nullptr;

#if 0
    int spawn_count = 0;
    CPlayerSpawnPoint* choosen_spawn = nullptr;

    while( ( entity = UTIL_FindEntityByClassname( entity, spawn_name ) ) != nullptr )
    {
        CPlayerSpawnPoint* spawnspot = static_cast<CPlayerSpawnPoint*>( entity );

        if( spawnspot->CanPlayerSpawn( player ) )
        {
            spawn_count++;
            if( RANDOM_LONG( 0, spawn_count ) == 1 )
                choosen_spawn = spawnspot;
            else if( choosen_spawn == nullptr )
                choosen_spawn = spawnspot;
        }
    }

    if( choosen_spawn != nullptr )
        return choosen_spawn;
#endif

    std::vector<CPlayerSpawnPoint*> list_spawnspots;

    while( ( entity = UTIL_FindEntityByClassname( entity, spawn_name ) ) != nullptr )
    {
        CPlayerSpawnPoint* spawnspot = static_cast<CPlayerSpawnPoint*>( entity );

        if( spawnspot->CanPlayerSpawn( player ) )
        {
            list_spawnspots.push_back(spawnspot);
        }
    }

    if( list_spawnspots.size() > 0 )
    {
        return list_spawnspots.at( RANDOM_LONG( 0, list_spawnspots.size() - 1 ) );
    }

    CBaseEntity::Logger->error( "No active {} entity on level", spawn_name );

    return nullptr;
}
