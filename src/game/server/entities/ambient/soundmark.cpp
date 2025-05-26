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
#include "soundent.h"

class CSoundMark : public CPointEntity
{
    DECLARE_CLASS( CSoundMark, CPointEntity );
    DECLARE_DATAMAP();

    public:
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        float m_fSoundVolume = 1.0;
        float m_fSoundDuration = 0.3;
        string_t m_sOriginEntity;
};

BEGIN_DATAMAP( CSoundMark )
    DEFINE_FIELD( m_fSoundVolume, FIELD_FLOAT ),
    DEFINE_FIELD( m_fSoundDuration, FIELD_FLOAT ),
    DEFINE_FIELD( m_sOriginEntity, FIELD_STRING ),
END_DATAMAP();


LINK_ENTITY_TO_CLASS( ambient_soundmark, CSoundMark );

bool CSoundMark :: KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "sound_origin" ) )
    {
        m_sOriginEntity = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "sound_volume" ) )
    {
        m_fSoundVolume = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "sound_duration" ) )
    {
        m_fSoundDuration = atof( pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CSoundMark::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    auto pTarget = AllocNewActivator( pActivator, pCaller, m_sOriginEntity );

    CSoundEnt::InsertSound( pev->spawnflags, ( pTarget != nullptr ? pTarget->pev->origin : pev->origin ), m_fSoundVolume, m_fSoundDuration );
}
