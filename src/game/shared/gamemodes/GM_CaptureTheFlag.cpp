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

#include "GM_CaptureTheFlag.h"

void GM_CaptureTheFlag::OnPlayerPreThink( CBasePlayer* player, float time )
{
#ifdef CLIENT_DLL
#else
    const auto vecSrc = player->GetGunPosition();

    UTIL_MakeVectors( player->pev->v_angle );

    TraceResult tr;

    if( 0 != player->m_iFOV )
    {
        UTIL_TraceLine( vecSrc, vecSrc + 4096 * gpGlobals->v_forward, dont_ignore_monsters, player->edict(), &tr );
    }
    else
    {
        UTIL_TraceLine( vecSrc, vecSrc + 1280.0 * gpGlobals->v_forward, dont_ignore_monsters, player->edict(), &tr );
    }

    if( 0 != gmsgPlayerBrowse && tr.flFraction < 1.0 && player->m_iLastPlayerTrace != ENTINDEX( tr.pHit ) )
    {
        auto pOther = ToBasePlayer( tr.pHit );

        if( !pOther )
        {
            MESSAGE_BEGIN( MSG_ONE, gmsgPlayerBrowse, nullptr, player );
            g_engfuncs.pfnWriteByte( 0 );
            g_engfuncs.pfnWriteByte( 0 );
            g_engfuncs.pfnWriteString( "" );
            g_engfuncs.pfnMessageEnd();
        }
        else
        {
            MESSAGE_BEGIN( MSG_ONE, gmsgPlayerBrowse, nullptr, player );
            g_engfuncs.pfnWriteByte( static_cast<int>( pOther->m_iTeamNum == player->m_iTeamNum ) );

            const auto v11 = 0 == player->pev->iuser1 ? pOther->m_iTeamNum : CTFTeam::None;

            g_engfuncs.pfnWriteByte( (int)v11 );
            g_engfuncs.pfnWriteString( STRING( pOther->pev->netname ) );
            g_engfuncs.pfnWriteByte( ( byte )pOther->m_iItems );
            // Round health up to 0 to prevent wraparound
            g_engfuncs.pfnWriteByte( ( byte )std::max( 0.f, pOther->pev->health ) );
            g_engfuncs.pfnWriteByte( ( byte )pOther->pev->armorvalue );
            g_engfuncs.pfnMessageEnd();
        }

        player->m_iLastPlayerTrace = ENTINDEX( tr.pHit );
    }
#endif

    BaseClass::OnPlayerPreThink(player, time);
}