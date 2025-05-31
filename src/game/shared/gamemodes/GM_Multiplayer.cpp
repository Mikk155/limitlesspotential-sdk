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

#include "GM_Multiplayer.h"

#ifdef CLIENT_DLL

#include "vgui_TeamFortressViewport.h"
#include "vgui_ScorePanel.h"

extern int giTeamplay;

#else

#include "voice_gamemgr.h"

CVoiceGameMgr g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
    bool CanPlayerHearPlayer( CBasePlayer* pListener, CBasePlayer* pTalker ) override
    {
        if( g_GameMode->IsTeamPlay() )
        {
            if( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
            {
                return false;
            }
        }
        return true;
    }
};

static CMultiplayGameMgrHelper g_GameMgrHelper;

extern char* pszPlayerIPs[MAX_PLAYERS * 2];

#endif

void GM_Multiplayer::OnRegister()
{
#ifdef CLIENT_DLL
#else
    g_VoiceGameMgr.Init( &g_GameMgrHelper, gpGlobals->maxClients );
#endif

    BaseClass::OnRegister();
}

void GM_Multiplayer::OnUnRegister()
{
#ifdef CLIENT_DLL
#else
    g_VoiceGameMgr.Shutdown();
#endif

    BaseClass::OnUnRegister();
}

void GM_Multiplayer::OnThink()
{
#ifdef CLIENT_DLL
#else
    g_VoiceGameMgr.Update( gpGlobals->frametime );
#endif

    BaseClass::OnThink();
}

void GM_Multiplayer::OnClientInit( CBasePlayer* player )
{
#ifdef CLIENT_DLL
    gHUD.m_Teamplay = giTeamplay = m_gamemode_teams;

    if( gViewPort && !gViewPort->m_pScoreBoard )
    {
        gViewPort->CreateScoreBoard();
        gViewPort->m_pScoreBoard->Initialize();

        if( !gHUD.m_iIntermission )
        {
            gViewPort->HideScoreBoard();
        }
    }

    // Re-read default titles
    gHUD.m_TextMessage.LoadGameTitles(nullptr, true);

// -TODO Transfer the titles this map uses
//    gHUD.m_TextMessage.LoadGameTitles( custom title (or iteration for multiple append-replace) );
#else
#endif

    BaseClass::OnClientInit(player);
}

void GM_Multiplayer::OnClientConnect( int index )
{
#ifdef CLIENT_DLL
/*
    if( cl_entity_t* ent = gEngfuncs.GetEntityByIndex( index ); ent )
    {
    }
*/
#else

    if( edict_t* ent = g_engfuncs.pfnPEntityOfEntIndex( index ); !FNullEnt(ent) )
    {
        g_VoiceGameMgr.ClientConnected(ent);
    }

#endif

    BaseClass::OnClientConnect(index);
}

void GM_Multiplayer::OnClientDisconnect( int index )
{
#ifdef CLIENT_DLL
/*
    if( cl_entity_t* ent = gEngfuncs.GetEntityByIndex( index ); ent )
    {
    }
*/
#else

    if( edict_t* ent = g_engfuncs.pfnPEntityOfEntIndex( index ); !FNullEnt(ent) )
    {
        MESSAGE_BEGIN( MSG_ALL, gmsgSayText );
        {
            WRITE_BYTE( index );

            if( !FStringNull( ent->v.netname ) )
            {
                char text[256] = "";
                snprintf( text, sizeof( text ), "- %s has left the game\n", STRING( ent->v.netname ) );
                text[sizeof( text ) - 1] = 0;
                WRITE_STRING( text );
            }
            else
            {
                WRITE_STRING( "- A player left the game\n" );
            }
        }
        MESSAGE_END();

        if( CBasePlayer* player = ToBasePlayer( ent ); player != nullptr )
        {
            g_GameMode.Logger->trace( "{} disconnected", PlayerLogInfo{*player} );

            player->RemoveAllItems( true ); // destroy all of the players weapons and items

            const int playerIndex = player->entindex();

            free( pszPlayerIPs[playerIndex] );
            pszPlayerIPs[playerIndex] = nullptr;

            MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
                WRITE_BYTE( player->entindex() );
                WRITE_BYTE( 0 );
            MESSAGE_END();

            for( auto entity : UTIL_FindPlayers() )
            {
                if( entity->pev && entity != player && entity->m_hObserverTarget == player )
                {
                    const int savedIUser1 = entity->pev->iuser1;
                    entity->pev->iuser1 = 0;
                    entity->m_hObserverTarget = nullptr;
                    entity->Observer_SetMode( savedIUser1 );
                }
            }
        }
    }
#endif

    BaseClass::OnClientDisconnect(index);
}

void GM_Multiplayer::OnPlayerPreThink( CBasePlayer* player, float time )
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

    BaseClass::OnPlayerPreThink(player, time);
}