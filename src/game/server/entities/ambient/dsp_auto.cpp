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

// CONSIDER: if player in water state, autoset roomtype to 14,15 or 16.
class CAmbientDSPAuto : public CAmbientDSP
{
    DECLARE_CLASS( CAmbientDSPAuto, CAmbientDSP );
    DECLARE_DATAMAP();

public:
    void Think() override;
    bool Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    float m_flRadius;
};

LINK_ENTITY_TO_CLASS( ambient_dsp_auto, CAmbientDSPAuto );

BEGIN_DATAMAP( CAmbientDSPAuto )
    DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
END_DATAMAP();

bool CAmbientDSPAuto::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_flRadius" ) )
    {
        m_flRadius = atof( pkvd->szValue );
        return true;
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

/**
 *    @brief returns true if the given sound entity (pSound) is in range and can see the given player entity (target)
 */
bool FEnvSoundInRange( CAmbientDSPAuto* pSound, CBaseEntity* target, float& flRange )
{
    const Vector vecSpot1 = pSound->pev->origin + pSound->pev->view_ofs;
    const Vector vecSpot2 = target->pev->origin + target->pev->view_ofs;
    TraceResult tr;

    UTIL_TraceLine( vecSpot1, vecSpot2, ignore_monsters, pSound->edict(), &tr );

    // check if line of sight crosses water boundary, or is blocked

    if( ( 0 != tr.fInOpen && 0 != tr.fInWater ) || tr.flFraction != 1 )
        return false;

    // calc range from sound entity to player

    const Vector vecRange = tr.vecEndPos - vecSpot1;
    flRange = vecRange.Length();

    return pSound->m_flRadius >= flRange;
}

void CAmbientDSPAuto::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
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

void CAmbientDSPAuto::Think()
{
    const bool shouldThinkFast = [this]()
    {
        if( !m_bState )
            return false;

        // get pointer to client if visible; UTIL_FindClientInPVS will
        // cycle through visible clients on consecutive calls.
        CBasePlayer* player = UTIL_FindClientInPVS( this );

        if( !player )
            return false; // no player in pvs of sound entity, slow it down

        // check to see if this is the sound entity that is currently affecting this player
        float flRange;

        if( player->m_SndLast && player->m_SndLast == this )
        {
            // this is the entity currently affecting player, check for validity
            if( player->m_SndRoomtype != 0 && player->m_flSndRange != 0 )
            {
                // we're looking at a valid sound entity affecting
                // player, make sure it's still valid, update range
                if( FEnvSoundInRange( this, player, flRange ) )
                {
                    player->m_flSndRange = flRange;
                    return true;
                }
                else
                {
                    // current sound entity affecting player is no longer valid,
                    // flag this state by clearing sound handle and range.
                    // NOTE: we do not actually change the player's room_type
                    // NOTE: until we have a new valid room_type to change it to.

                    player->m_SndLast = nullptr;
                    player->m_flSndRange = 0;
                }
            }

            // entity is affecting player but is out of range,
            // wait passively for another entity to usurp it...
            return false;
        }

        // if we got this far, we're looking at an entity that is contending
        // for current player sound. the closest entity to player wins.
        if( FEnvSoundInRange( this, player, flRange ) )
        {
            if( flRange < player->m_flSndRange || player->m_flSndRange == 0 )
            {
                // new entity is closer to player, so it wins.
                player->m_SndLast = this;
                player->m_SndRoomtype = m_Roomtype;
                player->m_flSndRange = flRange;

                // New room type is sent to player in CBasePlayer::UpdateClientData.

                // crank up nextthink rate for new active sound entity
            }
            // player is not closer to the contending sound entity.
            // this effectively cranks up the think rate of env_sound entities near the player.
        }

        // player is in pvs of sound entity, but either not visible or not in range. do nothing.

        return true;
    }();

    pev->nextthink = gpGlobals->time + ( shouldThinkFast ? 0.25 : 0.75 );
}

bool CAmbientDSPAuto::Spawn()
{
    // spread think times
    pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.0, 0.5 );

    return BaseClass::Spawn();
}
