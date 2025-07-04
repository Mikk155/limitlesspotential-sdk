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

#include <memory>
#include <string>
#include <vector>

#include "GameLibrary.h"

#include "Achievements.h"

#include "trigger/eventhandler.h"

class CBasePlayer;
struct cvar_t;
class MapState;
struct ServerConfigContext;

template <typename DataContext>
class GameConfigDefinition;

/**
 *    @brief Handles core server actions
 */
class ServerLibrary final : public GameLibrary
{
public:
    ServerLibrary();
    ~ServerLibrary();

    ServerLibrary( const ServerLibrary& ) = delete;
    ServerLibrary& operator=( const ServerLibrary& ) = delete;
    ServerLibrary( ServerLibrary&& ) = delete;
    ServerLibrary& operator=( ServerLibrary&& ) = delete;

    MapState* GetMapState() { return m_MapState.get(); }

    bool IsCurrentMapLoadedFromSaveGame() const { return m_IsCurrentMapLoadedFromSaveGame; }

    bool HasFinishedLoading() const { return m_HasFinishedLoading; }

    int GetSpawnCount() const { return m_SpawnCount; }

    bool Initialize() override;

    void Shutdown() override;

    void RunFrame() override;

    /**
     *    @brief Should only ever be called by worldspawn's destructor
     */
    void MapIsEnding()
    {
        m_IsStartingNewMap = true;
    }

    bool CheckForNewMapStart( bool loadGame )
    {
        if( m_IsStartingNewMap )
        {
            m_IsStartingNewMap = false;
            NewMapStarted( loadGame );
            return true;
        }

        return false;
    }

    /**
     *    @brief Called right before entities are activated
     */
    void PreMapActivate();

    /**
     *    @brief Called right after entities are activated
     */
    void PostMapActivate();

    void OnUpdateClientData();

    /**
     *    @brief Called when the player activates (first UpdateClientData call after ClientPutInServer or Restore).
     */
    void PlayerActivating( CBasePlayer* player );

    bool CanPlayerConnect( edict_t* ent, const char* name, const char* address, char reason[128] );

protected:
    void AddGameSystems() override;

    void SetEntLogLevels( spdlog::level::level_enum level ) override;

private:
    template <typename... Args>
    void ShutdownServer( spdlog::format_string_t<Args...> fmt, Args&&... args );

    /**
     *    @brief Called when a new map has started
     *    @param loadGame Whether this is a save game being loaded or a new map being started
     */
    void NewMapStarted( bool loadGame );

    void CreateConfigDefinitions();

    void DefineConfigVariables();

public:
    void LoadServerConfigFiles( const WorldConfig& MapConfiguration );
private:

    void SendFogMessage( CBasePlayer* player );

    void LoadAllMaps( const CommandArgs& args );

    void LoadNextMap();

private:
    cvar_t* m_AllowDownload{};
    cvar_t* m_SendResources{};
    cvar_t* m_AllowDLFile{};

    std::shared_ptr<const GameConfigDefinition<ServerConfigContext>> m_ServerConfigDefinition;
    std::shared_ptr<const GameConfigDefinition<ServerConfigContext>> m_MapConfigDefinition;

    bool m_IsStartingNewMap = true;
    bool m_IsCurrentMapLoadedFromSaveGame = false;
    bool m_HasFinishedLoading = true;

    int m_InNewMapStartedCount = 0;

    int m_SpawnCount = 0;

    std::unique_ptr<MapState> m_MapState;

    std::vector<std::string> m_MapsToLoad;

public:
    std::vector<trigger_event_t> m_events;
};

inline ServerLibrary g_Server;
