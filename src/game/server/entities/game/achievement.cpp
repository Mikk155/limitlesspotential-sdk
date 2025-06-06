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
#include "Achievements.h"

class CGameAchievement : public CPointEntity
{
    DECLARE_CLASS( CGameAchievement, CPointEntity );
    DECLARE_DATAMAP();

    public:
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        string_t m_label;
        string_t m_name;
        string_t m_description;
};

BEGIN_DATAMAP( CGameAchievement )
    DEFINE_FIELD( m_label, FIELD_STRING ),
    DEFINE_FIELD( m_name, FIELD_STRING ),
    DEFINE_FIELD( m_description, FIELD_STRING ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( game_achievement, CGameAchievement );

bool CGameAchievement::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_label" ) )
    {
        m_label = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_name" ) )
    {
        m_name = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_description" ) )
    {
        m_description = ALLOC_STRING( pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CGameAchievement::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( ( pev->spawnflags & 1 ) != 0 )
    {
        if( CBasePlayer* player = ToBasePlayer(pActivator); player != nullptr )
        {
            g_Achievement.Achieve( player, STRING(m_label), STRING(m_name), STRING(m_description) );
        }
    }
    else
    {
        g_Achievement.Achieve( STRING(m_label), STRING(m_name), STRING(m_description) );
    }

    UTIL_Remove(this);
}
