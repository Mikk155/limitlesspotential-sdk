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

#include "Achievements.h"

#include "GameLibrary.h"

#include <JSONSystem.h>

#ifdef CLIENT_DLL

bool CAchievements::IsActive()
{
    return false;
}

bool CAchievements::Initialize()
{
    return true;
}

#else

cvar_t sv_achievements = { "sv_achievements", "1", FCVAR_SERVER };

bool CAchievements::IsActive()
{
    return ( static_cast<int>( sv_achievements.value ) == 1 );
}

bool CAchievements::Initialize()
{
    std::optional<json> json_opt = g_JSON.LoadJSONFile( "cfg/server/achievements.json" );

    if( !json_opt.has_value() || !json_opt.value().is_object() )
    {
        FileSystem_WriteTextToFile( "cfg/server/achievements.json", "{}", "GAMECONFIG" );
        m_achievements = json::object();
    }
    else
    {
        m_achievements = json_opt.value();
    }

    m_achievement_restore = g_ClientCommands.CreateScoped( "sv_achievement_restore", [this]( auto, const auto& )
    {
        FileSystem_WriteTextToFile( "cfg/server/achievements.json", "{}", "GAMECONFIG" );
        m_achievements = json::object();
        g_GameLogger->warn( "All achievements has been removed and restarted." );
    } );

    g_engfuncs.pfnCVarRegister( &sv_achievements );

    return true;
}

void CAchievements::PreMapActivate()
{
    m_flNextThink = 0;
}

void CAchievements::Save()
{
    if( m_bShouldSave && m_flNextThink < gpGlobals->time )
    {
        FileSystem_WriteTextToFile( "cfg/server/achievements.json",  m_achievements.dump().c_str(), "GAMECONFIG" );
        m_flNextThink = gpGlobals->time + 30.0;
        m_bShouldSave = false;
    }
}

void CAchievements::Achieve( CBasePlayer* player, const std::string& label, const std::string& name, const std::string& description )
{
    if( IsActive() && player != nullptr )
    {
        json& achievement = m_achievements[ label ];

        if( !achievement.is_object() )
        {
            achievement = json::object();
        }

        json& players = achievement[ "players" ];

        if( !players.is_array() )
        {
            players = json::array();
        }

        const char* ID = player->SteamID();

        for( const auto& entry : players )
        {
            if( entry == ID )
                return;
        }

        m_bShouldSave = true;

        players.push_back( ID );

        achievement[ "players" ] = players;

        m_achievements[ label ] = achievement;

        UTIL_ClientPrintAll( HUD_PRINTTALK, "- %s got the achievement \"%s\"\n", STRING( player->pev->netname ), name.c_str() );
        UTIL_ClientPrintAll( HUD_PRINTTALK, "(%s)\n", description.c_str() );
    }
}

void CAchievements::Achieve( const std::string& label, const std::string& name, const std::string& description )
{
    if( IsActive() )
    {
        for( auto player : UTIL_FindPlayers() )
        {
            Achieve( player, label, name, description );
        }
    }
}
#endif
