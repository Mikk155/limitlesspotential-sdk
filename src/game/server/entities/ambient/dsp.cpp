/***
*
*  Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*   This product contains software technology licensed from Id
*   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*   All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "dsp.h"
#include "PlayerList.h"

LINK_ENTITY_TO_CLASS( ambient_dsp, CAmbientDSP );

BEGIN_DATAMAP( CAmbientDSP )
    DEFINE_FIELD( m_Roomtype, FIELD_INTEGER ),
    DEFINE_FIELD( m_bState, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_bIsTrigger, FIELD_BOOLEAN ),
END_DATAMAP();

bool CAmbientDSP::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_Roomtype" ) )
    {
        m_Roomtype = atoi( pkvd->szValue );
        return true;
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CAmbientDSP::UpdateRoomType( CBasePlayer* player, USE_TYPE useType )
{
    if( player != nullptr )
    {
        switch( useType )
        {
            case USE_ON:
            {
                player->m_SndRoomtype = m_Roomtype;
                break;
            }
            case USE_OFF:
            {
                player->m_SndRoomtype = 0;
                break;
            }
            case USE_TOGGLE:
            {
                player->m_SndRoomtype = ( player->m_SndRoomtype == 0 ? m_Roomtype : 0 );
                break;
            }
        }
    }
}

void CAmbientDSP::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( m_bIsTrigger )
    {
        switch( useType )
        {
            case USE_OFF:
                m_bState = false;
            break;
            case USE_ON:
                m_bState = true;
            break;
            case USE_TOGGLE:
                m_bState = !m_bState;
            break;
        }
    }
    else
    {
        if( ( pev->spawnflags & AMBIENT_ACTIVATOR_ONLY ) != 0 )
        {
            if( pActivator != nullptr && pActivator->IsPlayer() )
            {
                UpdateRoomType( static_cast<CBasePlayer*>( pActivator ), useType );
            }
        }
        else
        {
            for( CBasePlayer* player : UTIL_FindPlayers() )
            {
                UpdateRoomType( player, useType );
            }
        }
        BaseClass::Use( pActivator, pCaller, useType, value );
    }
}

void CAmbientDSP::DSPTouch( CBaseEntity* pOther )
{
    if( !m_bState )
        return;

    auto executor = [this]( CBasePlayer* player ) -> void
    {
        if( player != nullptr && player->Intersects( this ) )
        {
            UpdateRoomType( player, USE_ON );

            // Check for removal
            BaseClass::Use( player, player, USE_OFF );
        }
    };

    if( g_GameMode->IsMultiplayer() )
    {
        for( CBasePlayer* player : UTIL_FindPlayers() )
        {
            executor( player );
        }
    }
    else
    {
        executor( UTIL_GetLocalPlayer() );
    }
}

bool CAmbientDSP::Spawn()
{
    if( STRING( pev->model )[0] == '*' )
    {
        SetModel( STRING( pev->model ) );

        pev->solid = SOLID_TRIGGER;
        m_bIsTrigger = true;

        SetTouch( &CAmbientDSP::DSPTouch );

        return true;
    }

    return BaseClass::Spawn();
}
