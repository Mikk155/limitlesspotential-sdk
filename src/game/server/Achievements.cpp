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

#include <JSONSystem.h>

bool CAchievements::Initialize()
{
    std::optional<json> json_opt = g_JSON.LoadJSONFile( "cfg/server/achievements.json" );

    if( !json_opt.has_value() || !json_opt.value().is_object() )
    {
        FileSystem_WriteTextToFile( "cfg/server/achievements.json", "{}", "GAMECONFIG" );
        m_achievements = {};
    }
    else
    {
        m_achievements = json_opt.value();
    }

    return true;
}

void CAchievements::PreMapActivate()
{
    m_flNextThink = 0;
}

void CAchievements::Save()
{
    if( m_flNextThink < gpGlobals->time )
    {
        const std::string output = m_achievements.dump();

        FileSystem_WriteTextToFile( "cfg/server/achievements.json", output.c_str(), "GAMECONFIG" );

        m_flNextThink = gpGlobals->time + 30.0;
    }
}

void CAchievements::Achieve( CBasePlayer* player, const std::string& name, const std::string& description )
{
    if( player != nullptr )
    {
        json& achievement = m_achievements[ name ];

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

        players.push_back( ID );

        achievement[ "players" ] = players;

        m_achievements[ name ] = achievement;

        UTIL_ClientPrintAll( HUD_PRINTTALK, "- %s got the achievement \"%s\"\n", STRING( player->pev->netname ), description.c_str() );
    }
}

void CAchievements::Achieve( const std::string& name, const std::string& description )
{
    for( auto player : UTIL_FindPlayers() )
    {
        Achieve( player, name, description );
    }
}
