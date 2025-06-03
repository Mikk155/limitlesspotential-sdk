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

/*
 * @brief Simple entity to play sounds
 */
class CAmbientSound : public CAmbient
{
    DECLARE_CLASS( CAmbientSound, CAmbient );
    DECLARE_DATAMAP();

    public:
        bool Spawn() override;
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        float m_fVolume;
        float m_flAttenuation;
        int m_iPitch;
};

BEGIN_DATAMAP( CAmbientSound )
    DEFINE_FIELD( m_fVolume, FIELD_FLOAT ),
    DEFINE_FIELD( m_flAttenuation, FIELD_FLOAT ),
    DEFINE_FIELD( m_iPitch, FIELD_INTEGER ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( ambient_sound, CAmbientSound );

bool CAmbientSound::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_fVolume" ) )
    {
        m_fVolume = std::clamp( 0.0, 10.0, atof( pkvd->szValue ) );
    }
    else if( FStrEq( pkvd->szKeyName, "m_flAttenuation" ) )
    {
        m_flAttenuation = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iPitch" ) )
    {
        m_iPitch = atoi( pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CAmbientSound::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( ( pev->spawnflags & AMBIENT_ACTIVATOR_ONLY ) != 0 )
    {
        if( pActivator != nullptr && pActivator->IsPlayer() )
        {
            pActivator->EmitAmbientSound( PlayFromEntity(pActivator, pCaller), STRING(m_sPlaySound), m_fVolume, m_flAttenuation, SND_ONLYHOST, m_iPitch );
        }
        else
        {
            CBaseEntity::Logger->warn( "Null activator on \"Activator only\" ambient_sound entity at {}", pev->origin.MakeString(0) );
        }
    }
    else
    {
        EmitAmbientSound( PlayFromEntity(pActivator, pCaller), STRING(m_sPlaySound), m_fVolume, m_flAttenuation, 0, m_iPitch );
    }

    BaseClass::Use( pActivator, pCaller, useType, value );
}

bool CAmbientSound::Spawn()
{
    Precache();
    return BaseClass::Spawn();
}
