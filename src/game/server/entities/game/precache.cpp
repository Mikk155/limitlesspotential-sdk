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

#include "cbase.h"

#define GAME_PRECACHE_MAX 32

enum PrecacheType
{
    Model = 0,
    Sound,
    Generic,
    Other
};

class CGamePrecache : public CPointEntity
{
    DECLARE_CLASS( CGamePrecache, CPointEntity );
    DECLARE_DATAMAP();

public:

    bool Spawn() override
    {
        Precache();
        return BaseClass::Spawn();
    };

    void Precache() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void PrecacheToArray( PrecacheType, const char* item );

private:
    int m_PrecacheType[GAME_PRECACHE_MAX];
    string_t m_Assets[GAME_PRECACHE_MAX];
    int m_ArraySize;
};

BEGIN_DATAMAP( CGamePrecache )
    DEFINE_FIELD( m_ArraySize, FIELD_INTEGER ),
    DEFINE_ARRAY( m_PrecacheType, FIELD_INTEGER, GAME_PRECACHE_MAX ),
    DEFINE_ARRAY( m_Assets, FIELD_STRING, GAME_PRECACHE_MAX ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( game_precache, CGamePrecache );

void CGamePrecache::PrecacheToArray( PrecacheType mode, const char* item )
{
    if( m_ArraySize < GAME_PRECACHE_MAX )
    {
        m_PrecacheType[ m_ArraySize ] = mode;
        m_Assets[ m_ArraySize ] = ALLOC_STRING( item );
        ++m_ArraySize;
    }
    else
    {
        CBaseEntity::Logger->warn( "game_precache failed to add {} to precache list, max precache limit reached, use another entity.", item );
    }
}

bool CGamePrecache::KeyValue( KeyValueData* pkvd )
{
    if( strstr( pkvd->szKeyName, "model" ) != nullptr )
    {
        PrecacheToArray( PrecacheType::Model, pkvd->szValue );
    }
    else if( strstr( pkvd->szKeyName, "sound" ) != nullptr )
    {
        PrecacheToArray( PrecacheType::Sound, pkvd->szValue );
    }
    else if( strstr( pkvd->szKeyName, "generic" ) != nullptr )
    {
        PrecacheToArray( PrecacheType::Generic, pkvd->szValue );
    }
    else if( strstr( pkvd->szKeyName, "other" ) != nullptr )
    {
        PrecacheToArray( PrecacheType::Other, pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CGamePrecache::Precache()
{
    for( int i = 0; i < m_ArraySize; ++i )
    {
        switch( m_PrecacheType[i] )
        {
            case PrecacheType::Model:
                PrecacheModel( STRING( m_Assets[i] ) );
            break;
            case PrecacheType::Sound:
                PrecacheSound( STRING( m_Assets[i] ) );
            break;
            case PrecacheType::Generic:
                UTIL_PrecacheGenericDirect( STRING( m_Assets[i] ) );
            break;
            case PrecacheType::Other:
                UTIL_PrecacheOther( STRING( m_Assets[i] ) );
            break;
        }
    }
}
