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

#include "GM_Deathmatch.h"

void GM_Multiplayer::PlayerPreThink( CBasePlayer* player, float time )
{
#ifdef CLIENT_DLL
#else
#if 0
    if( !player->IsObserver() )
    {
        if( player->pev->button == player->pev->oldbuttons )
        {
            player->m_iAFKTime = 0;
        }
        else if( gpGlobals->time > player->m_flAFKNextThink )
        {
            player->m_iAFKTime++;
            player->m_flAFKNextThink = gpGlobals->time + 1.0f;

            if( player->m_iAFKTime > g_cfg.GetValue<int>( "max_afk_time"sv, 300 ) )
            {
                player->m_bIsAFK = true;
                player->m_iAFKTime = 0;
            }
        }
    }
    else if( player->m_bIsAFK && gpGlobals->time > player->m_flAFKNextThink )
    {
        if( ( player->pev->button & IN_USE ) != 0 )
        {
            // -Move out of spectator
            player->m_bIsAFK = false;
        }
        else
        {
            ClientPrint( player, HUD_PRINTTALK, "Press +Use to exit AFK mode\n" );
        }

        player->m_flAFKNextThink = gpGlobals->time + 1.0f;
    }
#endif

    // If it's time. trace a hull to have a valid free space if the client uses "unstuck" command
    if( player->m_flLastFreePositionTrack < gpGlobals->time )
    {
        TraceResult tr;
        UTIL_TraceHull( player->pev->origin, player->pev->origin, dont_ignore_monsters, human_hull, nullptr, &tr );

        if( tr.fStartSolid != 0 || tr.fAllSolid != 0 )
        {
            player->m_VecLastFreePosition = tr.vecEndPos;
        }

        player->m_flLastFreePositionTrack = gpGlobals->time + 20.0f;
    }

    if( ( player->m_iItems & CTFItem::PortableHEV ) != 0 )
    {
        if( player->m_flNextHEVCharge <= gpGlobals->time )
        {
            if( player->pev->armorvalue < 150 )
            {
                player->pev->armorvalue += 1;

                if( !player->m_fPlayingAChargeSound )
                {
                    player->EmitSound( CHAN_STATIC, "ctf/pow_armor_charge.wav", VOL_NORM, ATTN_NORM );
                    player->m_fPlayingAChargeSound = true;
                }
            }
            else if( player->m_fPlayingAChargeSound )
            {
                player->StopSound( CHAN_STATIC, "ctf/pow_armor_charge.wav" );
                player->m_fPlayingAChargeSound = false;
            }

            player->m_flNextHEVCharge = gpGlobals->time + 0.5;
        }
    }
    else if( player->pev->armorvalue > 100 && player->m_flNextHEVCharge <= gpGlobals->time )
    {
        player->pev->armorvalue -= 1;
        player->m_flNextHEVCharge = gpGlobals->time + 0.5;
    }

    if( ( player->m_iItems & CTFItem::Regeneration ) != 0 )
    {
        if( player->m_flNextHealthCharge <= gpGlobals->time )
        {
            if( player->pev->health < 150.0 )
            {
                player->pev->health += 1;

                if( !player->m_fPlayingHChargeSound )
                {
                    player->EmitSound( CHAN_STATIC, "ctf/pow_health_charge.wav", VOL_NORM, ATTN_NORM );
                    player->m_fPlayingHChargeSound = true;
                }
            }
            else if( player->m_fPlayingHChargeSound )
            {
                player->StopSound( CHAN_STATIC, "ctf/pow_health_charge.wav" );
                player->m_fPlayingHChargeSound = false;
            }

            player->m_flNextHealthCharge = gpGlobals->time + 0.5;
        }
    }
    else if( player->pev->health > 100.0 && gpGlobals->time >= player->m_flNextHealthCharge )
    {
        player->pev->health -= 1;
        player->m_flNextHealthCharge = gpGlobals->time + 0.5;
    }

    if( player->m_pActiveWeapon && player->m_flNextAmmoCharge <= gpGlobals->time && ( player->m_iItems & CTFItem::Backpack ) != 0 )
    {
        player->m_pActiveWeapon->IncrementAmmo( player );
        player->m_flNextAmmoCharge = gpGlobals->time + 0.75;
    }

    if( gpGlobals->time - player->m_flLastDamageTime > 0.15 )
    {
        if( player->m_iMostDamage < player->m_iLastDamage )
            player->m_iMostDamage = player->m_iLastDamage;

        player->m_flLastDamageTime = 0;
        player->m_iLastDamage = 0;
    }

    if( g_fGameOver )
    {
        // check for button presses
        if( ( player->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) ) != 0 )
            m_iEndIntermissionButtonHit = true;

        // clear attack/use commands from player
        player->m_afButtonPressed = 0;
        player->pev->button = 0;
        player->m_afButtonReleased = 0;
    }
#endif

    BaseClass::PlayerPreThink(player, time);
}