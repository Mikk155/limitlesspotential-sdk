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

#include "GameMode.h"

#include "GM_Multiplayer.h"
#include "GM_Singleplayer.h"
#include "GM_Deathmatch.h"
#include "GM_TeamDeathmatch.h"
#include "GM_Cooperative.h"
#include "GM_CaptureTheFlag.h"

#ifndef CLIENT_DLL
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

#endif

void GM_Base::OnRegister()
{
    g_GameMode.Logger->trace( "Registered gamemode \"{}\" in {}", GetName(), GM_LIB );
}

void GM_Base::OnUnRegister()
{
    g_GameMode.Logger->trace( "Un Registered gamemode \"{}\" in {}", GetName(), GM_LIB );
}

void GM_Base::OnClientInit( CBasePlayer* player )
{
#ifndef CLIENT_DLL
    MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, nullptr, player );
        WRITE_BYTE( static_cast<int>( ClientGameModeNetwork::fnOnClientInit ) );
    MESSAGE_END();
#endif

    g_GameMode.Logger->trace( "Called OnClientInit in {}", GM_LIB );
}

float GM_Base::OnPlayerFallDamage( CBasePlayer* player, float fall_velocity )
{
    float flDamage = g_cfg.GetValue<float>( "fall_damage"sv, 0.22522522522f, player );

    if( flDamage == 0 )
        goto done;

    if( flDamage < 0 )
    {
        flDamage *= -1.0f; // Invert to get a fixed value
        goto done;
    }

    // subtract off the speed at which a player is allowed to fall without being hurt,
    // so damage will be based on speed beyond that, not the entire fall
    flDamage = ( fall_velocity - PLAYER_MAX_SAFE_FALL_SPEED ) * flDamage;
    // -TODO PLAYER_MAX_SAFE_FALL_SPEED cfg?

done:

    return flDamage;
}

void GM_Base::OnClientConnect( int index )
{
#ifdef CLIENT_DLL
#else
    MESSAGE_BEGIN( MSG_ALL, gmsgGameMode );
        WRITE_BYTE( static_cast<int>( ClientGameModeNetwork::fnOnClientConnect ) );
        WRITE_BYTE( index );
    MESSAGE_END();
#endif
}

void GM_Base::OnClientDisconnect( int index )
{
#ifdef CLIENT_DLL
#else
    MESSAGE_BEGIN( MSG_ALL, gmsgGameMode );
        WRITE_BYTE( static_cast<int>( ClientGameModeNetwork::fnOnClientDisconnect ) );
        WRITE_BYTE( index );
    MESSAGE_END();
#endif
}


void GM_Base::OnPlayerPreThink( CBasePlayer* player, float time )
{
#ifdef CLIENT_DLL
#else
#endif
}

void GM_Base::_UpdateClientGameMode_( CBasePlayer* player )
{
#ifndef CLIENT_DLL
    MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, nullptr, player );
        WRITE_BYTE( static_cast<int>( ClientGameModeNetwork::fnGameModeUpdate ) );
        WRITE_STRING( g_GameMode->GetName() );
        WRITE_STRING( g_GameMode->GetBaseName() );
    MESSAGE_END();
#else
#endif
}

/*
 * ================================================================================
 *          Game Mode manager
 * ================================================================================
 */

void CGameModes::Shutdown()
{
    Logger.reset();
    
    if( gamemode != nullptr )
    {
        delete gamemode;
    }

#ifndef CLIENT_DLL
    g_VoiceGameMgr.Shutdown();
#endif
}

template <typename TGameMode>
void CGameModes::NewGameModeEntry()
{
    int index = 0;

    for( const GameModeFactoryEntry& gameMode : GameModeFactory )
    {
        if( strcmp( gameMode.first, TGameMode::GameModeName ) == 0 )
        {
            Logger->error( "Couldn't create gamemode \"{}\" already exists at index {}", gameMode.first, index );
            return;
        }
        index++;
    }

    GameModeFactory.push_back( std::make_pair( TGameMode::GameModeName, [] { return new TGameMode(); } ) );
}

#ifdef ANGELSCRIPT
void CGameModes::RegisterCustomGameModes()
{
    GameModeFactory.resize(GameModeFactorySize);
//    NewGameModeEntry<GM_YourClassName>();
}
#endif

bool CGameModes::Initialize()
{
    Logger = g_Logging.CreateLogger( "gamemode" );

    NewGameModeEntry<GM_Singleplayer>();
    NewGameModeEntry<GM_Multiplayer>();
    NewGameModeEntry<GM_Deathmatch>();
    NewGameModeEntry<GM_TeamDeathmatch>();
    NewGameModeEntry<GM_CaptureTheFlag>();
    NewGameModeEntry<GM_Cooperative>();

    // So the game clears up custom gamemodes after a map ends
    GameModeFactorySize = GameModeFactory.size();

#ifndef CLIENT_DLL
    g_ConCommands.CreateCommand(  "mp_list_gamemodes", [this]( const auto& )
    {
        Con_Printf( "Singleplayer always uses singleplayer gamemode\n" );
        Con_Printf( "Set mp_gamemode to \"autodetect\" to autodetect the game mode\n" );

        int index = 0;

        for( const GameModeFactoryEntry& gameMode : GameModeFactory )
        {
            Con_Printf( "[%i] \"%s\"\n", index, gameMode.first );
            index++;
        }
    },
    CommandLibraryPrefix::No );

    g_ConCommands.CreateCommand(  "mp_gamemode_autoupdate", [this]( const auto& args )
    {
        if( args.Count() != 2 )
        {
            Con_Printf( "Usage: %s 0/1\nIf enabled, The gamemode will be updated instantly without the need for the map to change\n", args.Argument( 0 ) );
            return;
        }

        GameModeAutoUpdate = ( atoi( args.Argument( 1 ) ) == 1 );
        Con_Printf( "Set gamemode to update %s.\n", ( GameModeAutoUpdate ? "automatically" : "when the level changes" ) );
    },
    CommandLibraryPrefix::No );

    g_VoiceGameMgr.Init( &g_GameMgrHelper, gpGlobals->maxClients );
#else
    g_ClientUserMessages.RegisterHandler( "GameMode", &CGameModes::MsgFunc_UpdateGameMode, this );
#endif

    return true;
}

GM_Base* CGameModes::operator->()
{
    if( gamemode != nullptr )
    {
#ifndef CLIENT_DLL
        // -TODO Move out gamemode cvars once gamerules are striped out
        if( GameModeAutoUpdate )
        {
            if( const char* gamemode_changed = CVAR_GET_STRING( "mp_gamemode" ); gamemode_changed )
            {
                if( strcmp( gamemode_name.c_str(), gamemode_changed ) != 0 )
                {
                    UpdateGameMode( gamemode_changed );
                }
            }
        }
#endif
        return gamemode;
    }

    UpdateGameMode(gamemode_name);

    return gamemode;
}

bool CGameModes::UpdateGameMode( const std::string& name )
{
    if( gamemode != nullptr )
    {
        // Same gamemode, no need to update.
        if( name.empty() || strcmp( name.c_str(), gamemode->GetName() ) == 0 )
        {
            return true;
        }
        gamemode->OnUnRegister();
        delete gamemode;
    }

    bool GM_Found = false;

#ifndef CLIENT_DLL
    // If we're in the server library and there's only one max player set singleplayer.
    if( gpGlobals->maxClients == 1 ) // -TODO GMU Check it's not MP with 1 max player
    {
        if( !name.empty() && strcmp( name.c_str(), GM_Singleplayer::GameModeName ) != 0 )
        {
            Logger->info( "Ignoring gamemode setting {} in singleplayer", name );
        }

        gamemode_name = GM_Singleplayer::GameModeName;
        gamemode = new GM_Singleplayer();
        GM_Found = true;
    }
#endif

    if( strcmp( name.c_str(), "default" ) == 0 )
    {
        gamemode_name = "default";
        gamemode = new GM_Base();
        GM_Found = true;
    }

    if( !GM_Found )
    {
        for( const GameModeFactoryEntry& gameMode : GameModeFactory )
        {
            if( strcmp( gameMode.first, name.c_str() ) == 0 )
            {
                Logger->trace( "Using gamemode \"{}\" on {}", gameMode.first, GM_LIB );
                gamemode_name = gameMode.first;
                gamemode = gameMode.second();
                GM_Found = true;
                break;
            }
        }
    }

    if( !GM_Found )
    {
        if( name.empty() )
        {
            Logger->error( "Couldn't auto detect game mode, creating \"{}\" game mode", GM_Multiplayer::GameModeName );
        }
        else
        {
            Logger->error( "Couldn't create \"{}\" game mode, falling back to \"{}\" game mode", name, GM_Multiplayer::GameModeName );
        }

        gamemode_name = GM_Multiplayer::GameModeName;
        gamemode = new GM_Multiplayer();
    }

    gamemode->OnRegister();

#ifndef CLIENT_DLL
    MESSAGE_BEGIN( MSG_ALL, gmsgGameMode );
        WRITE_BYTE( static_cast<int>( ClientGameModeNetwork::fnGameModeUpdate ) );
        WRITE_STRING( gamemode->GetName() );
        WRITE_STRING( gamemode->GetBaseName() );
    MESSAGE_END();
#endif

    return GM_Found;
}

#ifdef CLIENT_DLL
void CGameModes::MsgFunc_UpdateGameMode( BufferReader& reader )
{
    ClientGameModeNetwork Action = static_cast<ClientGameModeNetwork>( reader.ReadByte() );

    switch( Action )
    {
        case ClientGameModeNetwork::fnGameModeUpdate:
        {
            std::string new_gamemode = std::string( reader.ReadString() );

            // If the client fails to get a custom gamemode then load the base class to at least have basic stuff
            if( bool found = UpdateGameMode( new_gamemode ); !found )
            {
                new_gamemode = std::string( reader.ReadString() );
                UpdateGameMode( new_gamemode );
            }
            break;
        }
        case ClientGameModeNetwork::fnOnClientInit:
        {
            g_GameMode->OnClientInit(nullptr);
            break;
        }
        case ClientGameModeNetwork::fnOnClientConnect:
        {
            g_GameMode->OnClientConnect( reader.ReadByte() );
            break;
        }
        case ClientGameModeNetwork::fnOnClientDisconnect:
        {
            g_GameMode->OnClientDisconnect( reader.ReadByte() );
            break;
        }
    }
}
#endif
