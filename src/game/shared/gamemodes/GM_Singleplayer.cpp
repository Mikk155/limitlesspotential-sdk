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

#include "GM_Singleplayer.h"

#ifdef CLIENT_DLL

#include "vgui_TeamFortressViewport.h"
#include "vgui_ScorePanel.h"

#else

#endif

void GM_Singleplayer::OnRegister()
{
#ifdef CLIENT_DLL
#else
    // Define this as a dummy command to silence console errors.
    m_VModEnableCommand = g_ClientCommands.CreateScoped( "vmodenable", []( auto, const auto& ) {} );
    m_MapSelectionTrigger = g_ClientCommands.CreateScoped( "client_fire", []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() != 1 )
        {
            FireTargets( args.Argument(1), player, player, ( args.Count() != 2 ? static_cast<USE_TYPE>( atoi( args.Argument(2) ) ) : USE_TOGGLE ) );
        }
    } );
#endif

    BaseClass::OnRegister();
}

void GM_Singleplayer::OnUnRegister()
{
#ifdef CLIENT_DLL
#else
    m_VModEnableCommand.Remove();
    m_MapSelectionTrigger.Remove();
#endif

    BaseClass::OnUnRegister();
}

void GM_Singleplayer::OnClientInit( CBasePlayer* player )
{
#ifdef CLIENT_DLL
    if( gViewPort )
    {
        gViewPort->ShowCampaignSelectMenu();
    }
#else
    if( gpGlobals->maxClients > 1 || (int)CVAR_GET_FLOAT( "deathmatch" ) == 1 )
    {
        m_IsMultiplayer = true;
    }
#endif

    BaseClass::OnClientInit(player);
}

void GM_Singleplayer::OnThink()
{
#ifdef CLIENT_DLL
#else
    if( m_IsMultiplayer )
    {
        if( (int)m_MultiplayerRestart == 0 )
        {
            g_GameMode.Logger->warn( "Running map {} with GM_Singleplayer gamemode on a multiplayer server!", STRING( gpGlobals->mapname ) );

            if( IS_DEDICATED_SERVER() )
            {
                UTIL_ClientPrintAll( HUD_PRINTTALK, "Warning! This is a SinglePlayer map and may not work as expected.\n" );
                m_IsMultiplayer = false;
                return;
            }

            UTIL_ClientPrintAll( HUD_PRINTTALK, "Warning! This is a SinglePlayer map. This level will be restarted in 10 seconds..\n" );
            m_MultiplayerRestart = gpGlobals->time + 10.0;
        }
        else if( m_MultiplayerRestart < gpGlobals->time )
        {
            CLIENT_COMMAND( UTIL_GetLocalPlayer()->edict(),
                fmt::format( "disconnect;deathmatch 0;maxplayers 1; map {}\n", STRING( gpGlobals->mapname ) ).c_str() );
            m_IsMultiplayer = false;
        }
    }
#endif

    BaseClass::OnThink();
}