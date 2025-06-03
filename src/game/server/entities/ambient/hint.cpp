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

#include "CAmbient.h"
#include "soundent.h"

class CAmbientHint : public CAmbient
{
    DECLARE_CLASS( CAmbientHint, CAmbient );
    DECLARE_DATAMAP();

    public:
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        float m_fDuration = 0.3;
        int m_iSoundBits;
        int m_iVolume = 500;
};

BEGIN_DATAMAP( CAmbientHint )
    DEFINE_FIELD( m_fDuration, FIELD_FLOAT ),
    DEFINE_FIELD( m_iSoundBits, FIELD_INTEGER ),
    DEFINE_FIELD( m_iVolume, FIELD_INTEGER ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( ambient_hint, CAmbientHint );

bool CAmbientHint::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_fDuration" ) )
    {
        m_fDuration = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iSoundBits" ) )
    {
        m_iSoundBits = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iVolume" ) )
    {
        m_iVolume = atoi( pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CAmbientHint::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( m_iSoundBits != 0 )
    {
        CSoundEnt::InsertSound( m_iSoundBits, PlayFromEntity( pActivator, pCaller ), m_iVolume, m_fDuration );
    }

    BaseClass::Use( pActivator, pCaller, useType, value );
}
