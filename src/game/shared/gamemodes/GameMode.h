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

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>

#include <spdlog/logger.h>

#include "utils/shared_utils.h"

#ifdef CLIENT_DLL

#define GM_LIB "Client DLL"

#include "ui/hud/hud.h"
#include "networking/ClientUserMessages.h"

#else

#define GM_LIB "Server DLL"

#include "UserMessages.h"
#include "ConCommandSystem.h"
#include "player.h"

#endif

class CBaseItem;
class CBaseMonster;
class CBasePlayer;
class cl_entity_t;
class BufferReader;
class CBasePlayerWeapon;
class ScopedClientCommand;

enum ClientGameModeNetwork
{
    fnGameModeUpdate = 0,
    fnOnClientInit,
    fnOnClientConnect,
    fnOnClientDisconnect
};

/**
 * @brief Manages the rules for the current game mode.
 * @details For creating new gamemodes go to the bottom of this file.
 */
class GM_Base
{
public:

    GM_Base() = default;

    /**
     * @brief Return whatever the current gamemode is @c GameModeName
    */
    bool IsGamemode( std::string_view GameModeName ) {
        return ( GameModeName == std::string_view( GetName() ) );
    }

    /**
     * @brief Return whatever the current gamemode's baseclass is @c GameModeName
    */
    bool IsBaseGamemode( std::string_view GameModeName ) {
        return ( GameModeName == std::string_view( GetBaseName() ) );
    }

    void _UpdateClientGameMode_( CBasePlayer* player );

    virtual const char* GetName() const { return "default"; }
    virtual const char* GetBaseName() const { return "default"; }
    virtual const char* GetGameDescription() const { return "Half-Life: Limitless Potential"; }

    /**
     * @brief Return whatever players can form teams in this gamemode
    */
    virtual bool IsTeamPlay() { return false; }

    /**
     * @brief Return whatever this gamemode inherits from GM_Multiplayer
    */
    virtual bool IsMultiplayer() { return false; };

    /**
     * @brief Called every frame
     * SERVER: In StartFrame()
     * CLIENT: After everything in CHud::Think()
    */
    virtual void OnThink() {};

    /**
     * @brief Called every time a player thinks
     * SERVER: called from CBasePlayer::PreThink before anything else. @c time is zero
     * CLIENT: called from CHud::Redraw before anything else. @c player is nullptr
    */
    virtual void OnPlayerPreThink( CBasePlayer* player, float time );

    /**
     * @brief Called every time a player thinks
     * SERVER: called from CBasePlayer::PostThink at the end. @c time is zero
     * CLIENT: called from CHud::Redraw at the end. @c player is nullptr
    */
    virtual void OnPlayerPostThink( CBasePlayer* player, float time ) {};

    /**
     * @brief Called when a map starts, this is called at the bottom of ServerLibrary::LoadServerConfigFiles
     * CLIENT: No effect.
     */
    virtual void OnMapInit() {};

    /**
     * @brief Called when the gamemode has been selected while the map is starting
     */
    virtual void OnRegister();

    /**
     * @brief Called before the gamemode is being removed and a new one is selected while the map is starting.
     */
    virtual void OnUnRegister();

    /**
     * @brief Called when a client is fully initialized in the server
     * CLIENT: Called when the client receives the new gamemode
     */
    virtual void OnClientInit( CBasePlayer* player );

    /**
     * @brief This player just hit the ground after a fall. How much damage?
     * 
     * CLIENT: @c player is nullptr
     */
    virtual float OnPlayerFallDamage( CBasePlayer* player, float fall_velocity );

    /**
     * @brief A client is connecting to the server
     * 
     * CLIENT: Called on all connected players
     */
    virtual void OnClientConnect( int index );

    /**
     * @brief A client disconnected from the server
     * 
     * CLIENT: Called on all connected players
     */
    virtual void OnClientDisconnect( int index );
};

using GameModeFactoryEntry = std::pair<const char*, std::function<GM_Base*()>>;

class CGameModes final : public IGameSystem
{
private:

    int GameModeFactorySize;

    bool GameModeAutoUpdate = false;

public:

    std::string gamemode_name;

    GM_Base* gamemode;

    const char* GetName() const override {
        return "GameMode";
    }

    bool Initialize() override;
    void PostInitialize() override {}
    void Shutdown() override;

    static inline std::shared_ptr<spdlog::logger> Logger;

    std::vector<GameModeFactoryEntry> GameModeFactory;

    template <typename TGameMode>
    void NewGameModeEntry();

#ifdef ANGELSCRIPT
    void RegisterCustomGameModes();
#endif

    void SetGameMode( std::string name ) {
        gamemode_name = name;
    }

    bool UpdateGameMode( const std::string& name );

#ifdef CLIENT_DLL
    void MsgFunc_UpdateGameMode( BufferReader& reader );
#endif

    CGameModes() = default;
    ~CGameModes() = default;
    GM_Base* operator->();
};

inline CGameModes g_GameMode;

/*
This is a simple example for what you require on your gamemode:

#pragma once

#include "GameMode.h"
#include "GM_Multiplayer.h"

class GM_Sandbox : public GM_Multiplayer
{
    DECLARE_CLASS( GM_Sandbox, GM_Multiplayer );
    GM_Sandbox() = default;

public:

    static constexpr char GameModeName[] = "gamename";
    const char* GetName() const override { return GameModeName; }
    const char* GetBaseName() const override { return BaseClass::GetName(); }
};

You can inherit from any gamemode but if it's multiplayer then go for it

Go to "CGameModes::RegisterCustomGameModes()" in GameMode.cpp and register your gamemode

void CGameModes::RegisterCustomGameModes()
{
    NewGameModeEntry<GM_Sandbox>();
}
*/