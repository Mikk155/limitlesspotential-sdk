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
#include "talkmonster.h"

/**
 *    @brief Used for announcements per level, for door lock/unlock spoken voice.
 */
class CAmbientSpeaker : public CAmbient
{
    DECLARE_CLASS( CAmbientSpeaker, CAmbient );
    DECLARE_DATAMAP();

public:
    void Think() override;
    SpawnAction Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    int ObjectCaps() override { return ( CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ); }

private:
    bool m_bStartOff;
    int m_iMin = 15;
    int m_iMax = 135;
    int m_iPitch = 100;
    float m_fAttenuation = 0.3;

    bool m_HasCustomMaxLegacy;
};

LINK_ENTITY_TO_CLASS( ambient_speaker, CAmbientSpeaker );

BEGIN_DATAMAP( CAmbientSpeaker )
    DEFINE_FIELD( m_bStartOff, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_iMin, FIELD_INTEGER ),
    DEFINE_FIELD( m_iMax, FIELD_INTEGER ),
    DEFINE_FIELD( m_iPitch, FIELD_INTEGER ),
    DEFINE_FIELD( m_fAttenuation, FIELD_FLOAT ),
    
END_DATAMAP();

SpawnAction CAmbientSpeaker::Spawn()
{
    if( FStringNull( pev->message ) || strlen( STRING( pev->message ) ) < 1 )
    {
        Logger->error( "SPEAKER with no Level/Sentence! at: {}", pev->origin.MakeString() );
        return SpawnAction::DelayRemove;
    }

    pev->nextthink = 0.0;

    // set first announcement time for random n second
    if( !m_bStartOff )
    {
        pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_iMin, m_HasCustomMaxLegacy ? m_iMax : 15.0 );
    }

    return BaseClass::Spawn();
}

void CAmbientSpeaker::Think()
{
    float flvolume = pev->health * 0.1;

    // Wait for the talkmonster to finish first.
    if( gpGlobals->time <= CTalkMonster::g_talkWaitTime )
    {
        pev->nextthink = CTalkMonster::g_talkWaitTime + RANDOM_FLOAT( 5, 10 );
        return;
    }

    const char* szSoundFile = STRING( pev->message );

    if( szSoundFile[0] == '!' )
    {
        // play single sentence, one shot
        EmitAmbientSound( pev->origin, szSoundFile, flvolume, m_fAttenuation, 0, m_iPitch );

        // shut off and reset
        pev->nextthink = 0.0;
    }
    else
    {
        // make random announcement from sentence group
        if( sentences::g_Sentences.PlayRndSz( this, szSoundFile, flvolume, m_fAttenuation, 0, m_iPitch ) < 0 )
        {
            Logger->debug( "Level Design Error!: SPEAKER has bad sentence group name: {}", szSoundFile );
        }

        pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_iMin, m_iMax );

        // time delay until it's ok to speak: used so that two NPCs don't talk at once
        CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 3, 7 );
    }

    // Check for removal
    BaseClass::Use( this, this, USE_OFF );
}

/**
 *    @brief if an announcement is pending, cancel it. If no announcement is pending, start one.
 */
void CAmbientSpeaker::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    bool fActive = ( pev->nextthink > 0.0 );

    // fActive is true only if an announcement is pending

    if( useType != USE_TOGGLE )
    {
        // ignore if we're just turning something on that's already on, or
        // turning something off that's already off.
        if( ( fActive && useType == USE_ON ) || ( !fActive && useType == USE_OFF ) )
            return;
    }

    if( useType == USE_ON )
    {
        // turn on announcements
        pev->nextthink = gpGlobals->time + 0.1;
        return;
    }

    if( useType == USE_OFF )
    {
        // turn off announcements
        pev->nextthink = 0.0;
        return;
    }

    // Toggle announcements

    if( fActive )
    {
        // turn off announcements
        pev->nextthink = 0.0;
    }
    else
    {
        // turn on announcements
        pev->nextthink = gpGlobals->time + 0.1;
    }
}

bool CAmbientSpeaker::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_bStartOff" ) )
    {
        m_bStartOff = ( atoi( pkvd->szValue ) == 1 );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iMax" ) )
    {
        m_HasCustomMaxLegacy = true;
        m_iMax = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iMin" ) )
    {
        m_iMin = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_fAttenuation" ) )
    {
        m_fAttenuation = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iPitch" ) )
    {
        m_iPitch = atoi( pkvd->szValue );
    }
    else
    {
        return CBaseEntity::KeyValue( pkvd );
    }

    return true;
}
