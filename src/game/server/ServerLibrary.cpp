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

#include <chrono>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "cbase.h"
#include "CClientFog.h"
#include "client.h"
#include "EntityTemplateSystem.h"
#include "MapState.h"
#include "nodes.h"
#include "ProjectInfoSystem.h"
#include "scripted.h"
#include "ServerConfigContext.h"
#include "ServerLibrary.h"
#include "ConfigurationSystem.h"
#include "UserMessages.h"
#include "voice_gamemgr.h"

#include "bot/BotSystem.h"

#include "config/CommandWhitelist.h"
#include "config/ConditionEvaluator.h"
#include "config/GameConfig.h"
#include "config/sections/CommandsSection.h"
#include "config/sections/CrosshairColorSection.h"
#include "config/sections/EchoSection.h"
#include "config/sections/EntityClassificationsSection.h"
#include "config/sections/EntityTemplatesSection.h"
#include "config/sections/GameDataFilesSections.h"
#include "config/sections/GlobalReplacementFilesSections.h"
#include "config/sections/HudColorSection.h"
#include "config/sections/HudReplacementSection.h"
#include "config/sections/KeyvalueManagerSection.h"
#include "config/sections/SpawnInventorySection.h"
#include "config/sections/SuitLightTypeSection.h"

#include "entities/EntityClassificationSystem.h"

#include "gamerules/MapCycleSystem.h"
#include "gamerules/PersistentInventorySystem.h"
#include "gamerules/SpawnInventorySystem.h"

#include "models/BspLoader.h"

#include "networking/NetworkDataSystem.h"

#include "sound/MaterialSystem.h"
#include "sound/SentencesSystem.h"
#include "sound/ServerSoundSystem.h"

#include "ui/hud/HudReplacementSystem.h"

constexpr char DefaultMapConfigFileName[] = "cfg/server/default_map_config.json";

cvar_t servercfgfile = {"sv_servercfgfile", "cfg/server/server.json", FCVAR_NOEXTRAWHITEPACE | FCVAR_ISPATH};
cvar_t mp_gamemode = {"mp_gamemode", "", FCVAR_SERVER};
cvar_t mp_createserver_gamemode = {"mp_createserver_gamemode", "", FCVAR_SERVER};

cvar_t sv_infinite_ammo = {"sv_infinite_ammo", "0", FCVAR_SERVER};
cvar_t sv_bottomless_magazines = {"sv_bottomless_magazines", "0", FCVAR_SERVER};

ServerLibrary::ServerLibrary() = default;
ServerLibrary::~ServerLibrary() = default;

bool ServerLibrary::Initialize()
{
    m_MapState = std::make_unique<MapState>();

    // Make sure both use the same info on the server.
    g_ProjectInfo.SetServerInfo( *g_ProjectInfo.GetLocalInfo() );

    if( !GameLibrary::Initialize() )
    {
        return false;
    }

    m_AllowDownload = g_ConCommands.GetCVar( "sv_allowdownload" );
    m_SendResources = g_ConCommands.GetCVar( "sv_send_resources" );
    m_AllowDLFile = g_ConCommands.GetCVar( "sv_allow_dlfile" );

    g_PrecacheLogger = g_Logging.CreateLogger( "precache" );
    CBaseEntity::IOLogger = g_Logging.CreateLogger( "ent.io" );
    CBaseMonster::AILogger = g_Logging.CreateLogger( "ent.ai" );
    CCineMonster::AIScriptLogger = g_Logging.CreateLogger( "ent.ai.script" );
    CGraph::Logger = g_Logging.CreateLogger( "nodegraph" );
    CSaveRestoreBuffer::Logger = g_Logging.CreateLogger( "saverestore" );
    CGameRules::Logger = g_Logging.CreateLogger( "gamerules" );
    CVoiceGameMgr::Logger = g_Logging.CreateLogger( "voice" );

    UTIL_CreatePrecacheLists();

    SV_CreateClientCommands();

    g_engfuncs.pfnCVarRegister( &servercfgfile );
    g_engfuncs.pfnCVarRegister( &mp_gamemode );
    g_engfuncs.pfnCVarRegister( &mp_createserver_gamemode );

    g_engfuncs.pfnCVarRegister( &sv_infinite_ammo );
    g_engfuncs.pfnCVarRegister( &sv_bottomless_magazines );

    g_ConCommands.CreateCommand( "load_all_maps", [this]( const auto& args )
        { LoadAllMaps( args ); } );

    // Escape hatch in case the command is executed in error.
    g_ConCommands.CreateCommand( "stop_loading_all_maps", [this]( const auto& )
        { m_MapsToLoad.clear(); } );

    g_ConCommands.RegisterChangeCallback( &sv_allowbunnyhopping, []( const auto& state )
        {
            const bool allowBunnyHopping = state.Cvar->value != 0;

            const auto setting = UTIL_ToString( allowBunnyHopping ? 1 : 0 );

            for( int i = 1; i <= gpGlobals->maxClients; ++i )
            {
                auto player = UTIL_PlayerByIndex( i );

                if( !player )
                {
                    continue;
                }

                g_engfuncs.pfnSetPhysicsKeyValue( player->edict(), "bj", setting.c_str() );
            } } );

    g_ConCommands.RegisterChangeCallback( &sv_infinite_ammo, []( const auto& state )
        { g_cfg.SetValue( "infinite_ammo", state.Cvar->value ); } );

    g_ConCommands.RegisterChangeCallback( &sv_bottomless_magazines, []( const auto& state )
        { g_cfg.SetValue( "bottomless_magazines", state.Cvar->value ); } );

    RegisterCommandWhitelistSchema();

    LoadCommandWhitelist();

    CreateConfigDefinitions();
    DefineConfigVariables();

    return true;
}

void ServerLibrary::Shutdown()
{
    m_MapConfigDefinition.reset();
    m_ServerConfigDefinition.reset();

    CVoiceGameMgr::Logger.reset();
    CGameRules::Logger.reset();
    CSaveRestoreBuffer::Logger.reset();
    CGraph::Logger.reset();
    CCineMonster::AIScriptLogger.reset();
    CBaseMonster::AILogger.reset();
    CBaseEntity::IOLogger.reset();
    g_PrecacheLogger.reset();

    GameLibrary::Shutdown();
}

static void ForceCvarToValue( cvar_t* cvar, float value )
{
    if( cvar->value != value )
    {
        // So server operators know what's going on since these cvars aren't normally logged.
        g_GameLogger->warn( "Forcing server cvar \"{}\" to \"{}\" to ensure network data file is transferred",
            cvar->name, value );
        g_engfuncs.pfnCvar_DirectSet( cvar, std::to_string( value ).c_str() );
    }
}

void ServerLibrary::RunFrame()
{
    GameLibrary::RunFrame();

    // Force the download cvars to enabled so we can download network data.
    ForceCvarToValue( m_AllowDownload, 1 );
    ForceCvarToValue( m_SendResources, 1 );
    ForceCvarToValue( m_AllowDLFile, 1 );

    g_Bots.RunFrame();

    // If we're loading all maps then change maps after 3 seconds (time starts at 1)
    // to give the game time to generate files.
    if( !m_MapsToLoad.empty() && gpGlobals->time > 4 )
    {
        LoadNextMap();
    }
}

// Note: don't return after calling this to ensure that server state is still mostly correct.
// Otherwise the game may crash.
template <typename... Args>
void ServerLibrary::ShutdownServer( spdlog::format_string_t<Args...> fmt, Args&&... args )
{
    g_GameLogger->critical( fmt, std::forward<Args>( args )... );
    SERVER_COMMAND( "shutdownserver\n" );
    // Don't do this; if done at certain points during the map spawn phase
    // this will cause a fatal error because the engine tries to write to
    // an uninitialized network message buffer.
    // SERVER_EXECUTE();
}

void ServerLibrary::NewMapStarted( bool loadGame )
{
    m_IsCurrentMapLoadedFromSaveGame = loadGame;

    ++m_SpawnCount;

    m_HasFinishedLoading = false;

    // Need to check if we're getting multiple map start commands in the same frame.
    m_IsStartingNewMap = true;
    ++m_InNewMapStartedCount;

    // Execute any commands still queued up so cvars have the correct value.
    SERVER_EXECUTE();
    // These extra executions are needed to overcome the engine's inserting of wait commands that
    // delay server settings configured in the Create Server dialog from being set.
    // We need those settings to configure our gamerules so we're brute forcing the additional executions.
    SERVER_EXECUTE();
    SERVER_EXECUTE();

    m_IsStartingNewMap = false;
    --m_InNewMapStartedCount;

    // If multiple map change commands are executed in the same frame then this will break the server's internal state.

    // Note that this will not happen the first time you load multiple maps after launching the client.
    // Console commands are processed differently when the server dll is loaded so
    // it will load the maps in reverse order.

    // This is because when the server dll is about to load it first executes remaining console commands.
    // If there are more map commands those will also try to load the server dll,
    // executing remaining commands in the process,
    // followed by the remaining map commands executing in reverse order.
    // Thus the second command is executed first, then control returns to the first map load command
    // which then continues loading.
    if( m_InNewMapStartedCount > 0 )
    {
        // Reset so we clear to a good state.
        m_IsStartingNewMap = true;
        m_InNewMapStartedCount = 0;

        // This engine function triggers a Host_Error when the first character has
        // a value <= to the whitespace character ' '.
        // It prints the entire string, so we can use this as a poor man's Host_Error.
        g_engfuncs.pfnForceUnmodified( force_exactfile, nullptr, nullptr,
            " \nERROR: Cannot execute multiple map load commands at the same time\n" );
        return;
    }

    g_GameLogger->trace( "Starting new map" );

    // Log some useful game info.
    g_GameLogger->info( "Maximum number of edicts: {}", gpGlobals->maxEntities );

    g_LastPlayerJoinTime = 0;

    for( auto list : {g_ModelPrecache.get(), g_SoundPrecache.get(), g_GenericPrecache.get()} )
    {
        list->Clear();
    }

    ClearStringPool();

    // Initialize map state to its default state
    *m_MapState = MapState{};

    g_ReplacementMaps.Clear();

    // Add BSP models to precache list.
    const auto completeMapName = fmt::format( "maps/{}.bsp", STRING( gpGlobals->mapname ) );

    if( auto bspData = BspLoader::Load( completeMapName.c_str() ); bspData )
    {
        g_ModelPrecache->AddUnchecked( STRING( ALLOC_STRING( completeMapName.c_str() ) ) );

        // Submodel 0 is the world so skip it.
        for( std::size_t subModel = 1; subModel < bspData->SubModelCount; ++subModel )
        {
            g_ModelPrecache->AddUnchecked( STRING( ALLOC_STRING( fmt::format( "*{}", subModel ).c_str() ) ) );
        }
    }
    else
    {
        ShutdownServer( "Shutting down server due to error loading BSP data" );
    }

    // Reset sky name to its default value. If the map specifies its own sky
    // it will be set in CWorld::KeyValue or restored by the engine on save game load.
    CVAR_SET_STRING( "sv_skyname", DefaultSkyName );

    if( !m_IsCurrentMapLoadedFromSaveGame )
    {
        // Clear trigger_event's events on new maps
        m_events.clear();
    }
}

void ServerLibrary::PreMapActivate()
{
    g_Achievement.PreMapActivate();
}

void ServerLibrary::PostMapActivate()
{
    if( !g_NetworkData.GenerateNetworkDataFile() )
    {
        ShutdownServer( "Shutting down server due to fatal error writing network data file" );
    }

    if( g_PrecacheLogger->should_log( spdlog::level::debug ) )
    {
        for( auto list : {g_ModelPrecache.get(), g_SoundPrecache.get(), g_GenericPrecache.get()} )
        {
            // Don't count the invalid string.
            g_PrecacheLogger->debug( "[{}] {} precached total", list->GetType(), list->GetCount() - 1 );
        }
    }
}

void ServerLibrary::OnUpdateClientData()
{
    // The first update occurs after the engine has unpaused itself.
    if( !m_HasFinishedLoading )
    {
        m_HasFinishedLoading = true;
    }
}

void ServerLibrary::PlayerActivating( CBasePlayer* player )
{
    // In singleplayer ClientPutInServer is only called when starting a new map with the map command,
    // so we need to initialize this here so entities can send their own updates at the right time.
    if( g_LastPlayerJoinTime == 0 )
    {
        g_LastPlayerJoinTime = gpGlobals->time;
    }

    // Override the hud color.
    if( m_MapState->m_HudColor )
    {
        g_GameMode->SetClientHUDColor( HUDElements::Uncategorized, player->entindex(), *m_MapState->m_HudColor );
    }

    // Override the crosshair color.
    if( m_MapState->m_CrosshairColor )
    {
        g_GameMode->SetClientHUDColor( HUDElements::Crosshair, player->entindex(), *m_MapState->m_CrosshairColor );
    }

    // Override the light type.
    if( m_MapState->m_LightType )
    {
        player->SetSuitLightType( *m_MapState->m_LightType );
    }

    SendFogMessage( player );
}

void ServerLibrary::AddGameSystems()
{
    GameLibrary::AddGameSystems();
    // Depends on Angelscript
    g_GameSystems.Add( &g_ConditionEvaluator );
    g_GameSystems.Add( &g_GameConfigSystem );
    g_GameSystems.Add( &sound::g_ServerSound );
    g_GameSystems.Add( &sentences::g_Sentences );
    g_GameSystems.Add( &g_MapCycleSystem );
    g_GameSystems.Add( &g_EntityTemplates );
    g_GameSystems.Add( &g_Bots );
}

void ServerLibrary::SetEntLogLevels( spdlog::level::level_enum level )
{
    GameLibrary::SetEntLogLevels( level );

    const auto& levelName = spdlog::level::to_string_view( level );

    for( auto& logger : {
             CBaseEntity::IOLogger,
             CBaseMonster::AILogger,
             CCineMonster::AIScriptLogger,
             CGraph::Logger,
             CSaveRestoreBuffer::Logger,
             CGameRules::Logger,
             CVoiceGameMgr::Logger} )
    {
        logger->set_level( level );
        Con_Printf( "Set \"%s\" log level to %s\n", logger->name().c_str(), levelName.data() );
    }
}

void ServerLibrary::CreateConfigDefinitions()
{
    m_ServerConfigDefinition = g_GameConfigSystem.CreateDefinition( "ServerGameConfig", false, [&]()
        {
            std::vector<std::unique_ptr<const GameConfigSection<ServerConfigContext>>> sections;

            // Server configs only get common and command sections. All other configuration is handled elsewhere.
            sections.push_back( std::make_unique<EchoSection<ServerConfigContext>>() );
            sections.push_back( std::make_unique<CommandsSection<ServerConfigContext>>() );

            return sections; }() );

    m_MapConfigDefinition = g_GameConfigSystem.CreateDefinition( "MapGameConfig", true, [&, this]()
        {
            std::vector<std::unique_ptr<const GameConfigSection<ServerConfigContext>>> sections;

            sections.push_back( std::make_unique<EchoSection<ServerConfigContext>>() );
            sections.push_back( std::make_unique<CommandsSection<ServerConfigContext>>( &g_CommandWhitelist ) );
            sections.push_back( std::make_unique<CrosshairColorSection>() );
            sections.push_back( std::make_unique<SentencesSection>() );
            sections.push_back( std::make_unique<MaterialsSection>() );
            sections.push_back( std::make_unique<SkillSection>() );
            sections.push_back( std::make_unique<GlobalModelReplacementSection>() );
            sections.push_back( std::make_unique<GlobalSentenceReplacementSection>() );
            sections.push_back( std::make_unique<GlobalSoundReplacementSection>() );
            sections.push_back( std::make_unique<HudColorSection>() );
            sections.push_back( std::make_unique<SuitLightTypeSection>() );
            sections.push_back( std::make_unique<SpawnInventorySection>() );
            sections.push_back( std::make_unique<EntityTemplatesSection>() );
            sections.push_back( std::make_unique<EntityClassificationsSection>() );
            sections.push_back( std::make_unique<HudReplacementSection>() );
            sections.push_back( std::make_unique<KeyvalueManagerSection>() );

            return sections; }() );
}

void ServerLibrary::DefineConfigVariables()
{
    // Gamemode variables
    g_cfg.DefineVariable( "coop_persistent_inventory_grace_period", 60, {.Minimum = -1} );
    g_cfg.DefineVariable( "allow_monsters", 1, {.Minimum = 0, .Maximum = 1, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "fall_damage", 0.22522522522, { .Networked = true, . Type = ConfigVarType::Float } );

    // Item variables
    g_cfg.DefineVariable( "healthcharger_recharge_time", -1,
        {.Minimum = ChargerRechargeDelayNever, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "hevcharger_recharge_time", -1,
        {.Minimum = ChargerRechargeDelayNever, .Type = ConfigVarType::Integer} );

    g_cfg.DefineVariable( "weapon_respawn_time", ITEM_NEVER_RESPAWN_DELAY,
        {.Minimum = -1, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "ammo_respawn_time", ITEM_NEVER_RESPAWN_DELAY,
        {.Minimum = -1, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "pickupitem_respawn_time", ITEM_NEVER_RESPAWN_DELAY,
        {.Minimum = -1, .Type = ConfigVarType::Integer} );

    g_cfg.DefineVariable( "weapon_instant_respawn", 0, {.Minimum = 0, .Maximum = 1, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "allow_npc_item_dropping", 1, {.Minimum = 0, .Maximum = 1, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "allow_player_weapon_dropping", 0, {.Minimum = 0, .Maximum = 1, .Type = ConfigVarType::Integer} );

    // Weapon variables
    g_cfg.DefineVariable( "infinite_ammo", 0, {.Minimum = 0, .Maximum = 1, .Networked = true, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "bottomless_magazines", 0, {.Minimum = 0, .Maximum = 1, .Networked = true, .Type = ConfigVarType::Integer} );

    g_cfg.DefineVariable( "chainsaw_melee", 0, {.Minimum = 0, .Maximum = 1, .Networked = true, .Type = ConfigVarType::Integer} );
    g_cfg.DefineVariable( "revolver_laser_sight", 0, {.Networked = true} );
    g_cfg.DefineVariable( "smg_wide_spread", 0, {.Networked = true} );
    g_cfg.DefineVariable( "shotgun_single_tight_spread", 0, {.Networked = true} );
    g_cfg.DefineVariable( "shotgun_double_wide_spread", 0, {.Networked = true} );
    g_cfg.DefineVariable( "crossbow_sniper_bolt", 0, {.Networked = true} );
    g_cfg.DefineVariable( "gauss_charge_time", 4, {.Minimum = 0.1f, .Maximum = 10.f, .Networked = true} );
    g_cfg.DefineVariable( "gauss_fast_ammo_use", 0, {.Networked = true} );
    g_cfg.DefineVariable( "gauss_damage_radius", 2.5f, {.Minimum = 0} );
    g_cfg.DefineVariable( "egon_narrow_ammo_per_second", 6, {.Minimum = 0} );
    g_cfg.DefineVariable( "egon_wide_ammo_per_second", 10, {.Minimum = 0} );
    g_cfg.DefineVariable( "grapple_fast", 0, {.Networked = true} );
    g_cfg.DefineVariable( "m249_wide_spread", 0, {.Networked = true} );
    g_cfg.DefineVariable( "shockrifle_fast", 0, {.Networked = true} );
}

void ServerLibrary::LoadServerConfigFiles( const WorldConfig& MapConfiguration )
{
    const auto start = std::chrono::high_resolution_clock::now();

    std::string mapConfigFileName;

    Vector Version( 1, 0, 0 );

    for( int i = 0; i < 3; i++ )
    {
        if( MapConfiguration.version[i] < Version[i] )
        {
            g_GameLogger->error( "Map \"{}\" is older than this game version.\n" \
                "Please run the MapUpgrader tool to update from {} to {} and avoid problems.",
                    STRING( gpGlobals->mapname ), MapConfiguration.version.MakeString(0), Version.MakeString(0) );
            break;
        }
    }

    auto GetConfigFile = []( const std::string& name ) -> std::pair<std::string, bool>
    {
        std::string filename = fmt::format( "cfg/maps/{}.json", name );

        if( g_pFileSystem->FileExists( filename.c_str() ) )
        {
            return { filename, true };
        }
        else
        {
            g_GameLogger->debug( "Failed to open map cfg file \"{}\"", filename );
        }

        return { name, false };
    };

    if( !FStringNull( MapConfiguration.cfg ) )
    {
        if( std::pair<std::string, bool> cfg = GetConfigFile( STRING( MapConfiguration.cfg ) ); cfg.second )
        {
            mapConfigFileName = std::move( cfg.first );
        }
    }

    if( mapConfigFileName.empty() )
    {
        if( std::pair<std::string, bool> cfg = GetConfigFile( STRING( gpGlobals->mapname ) ); cfg.second )
        {
            mapConfigFileName = std::move( cfg.first );
        }
    }

    if( mapConfigFileName.empty() )
    {
        g_GameLogger->debug( "Using default map config file \"{}\"", DefaultMapConfigFileName );
        mapConfigFileName = DefaultMapConfigFileName;
    }

    g_GameLogger->trace( "Loading map config file" );
    const std::optional<GameConfig<ServerConfigContext>> mapConfig = m_MapConfigDefinition->TryLoad( mapConfigFileName.c_str() );

    // The Create Server dialog only accepts lists with numeric values so we need to remap it to the game mode name.
    if( mp_createserver_gamemode.string[0] != '\0' )
    {
        g_engfuncs.pfnCvar_DirectSet( &mp_gamemode, GameModeIndexToString( int( mp_createserver_gamemode.value ) ) );
        g_engfuncs.pfnCvar_DirectSet( &mp_createserver_gamemode, "" );
    }

    std::string GameModeName;

    if( !FStringNull( MapConfiguration.gamemode ) )
    {
        GameModeName = STRING( MapConfiguration.gamemode );
        CGameRules::Logger->trace( "Setting map configured game mode {}", GameModeName );
    }

    // Gamemode is not locked, the server can update it
    if( !MapConfiguration.gamemode_lock && mp_gamemode.string[0] != '\0' )
    {
        GameModeName = mp_gamemode.string;
        CGameRules::Logger->trace( "Setting server configured game mode {}", GameModeName );
    }

    delete g_pGameRules;
    g_pGameRules = InstallGameRules( GameModeName );

    assert( g_pGameRules );

#ifdef ANGELSCRIPT
    g_GameMode->RegisterCustomGameModes();
#endif

    g_GameMode.UpdateGameMode( GameModeName );

    ServerConfigContext context{.State = *m_MapState};

    // Initialize file lists to their defaults.
    context.SentencesFiles.push_back( "sound/sentences.json" );
    context.MaterialsFiles.push_back( "sound/materials.json" );

    context.EntityClassificationsFileName = "cfg/default_entity_classes.json";

    if( const auto cfgFile = servercfgfile.string; cfgFile && '\0' != cfgFile[0] )
    {
        g_GameLogger->trace( "Loading server config file" );

        if( auto config = m_ServerConfigDefinition->TryLoad( cfgFile, "GAMECONFIG" ); config )
        {
            config->Parse( context );
        }
    }

    if( mapConfig )
    {
        mapConfig->Parse( context );
    }

    sentences::g_Sentences.LoadSentences( context.SentencesFiles );
    g_MaterialSystem.LoadMaterials( context.MaterialsFiles );
    g_cfg.LoadConfigurationFiles();

    // Override skill vars with cvars if they are enabled only.
    if( sv_infinite_ammo.value != 0 )
    {
        g_cfg.SetValue( "infinite_ammo", sv_infinite_ammo.value );
    }

    if( sv_bottomless_magazines.value != 0 )
    {
        g_cfg.SetValue( "bottomless_magazines", sv_bottomless_magazines.value );
    }

    m_MapState->m_GlobalModelReplacement = g_ReplacementMaps.LoadMultiple( 
        context.GlobalModelReplacementFiles, {.CaseSensitive = false} );
    m_MapState->m_GlobalSentenceReplacement = g_ReplacementMaps.LoadMultiple( 
        context.GlobalSentenceReplacementFiles, {.CaseSensitive = true} );
    m_MapState->m_GlobalSoundReplacement = g_ReplacementMaps.LoadMultiple( 
        context.GlobalSoundReplacementFiles, {.CaseSensitive = false} );

    g_SpawnInventory.SetInventory( std::move( context.SpawnInventory ) );

    g_EntityClassifications.Load( context.EntityClassificationsFileName );

    g_EntityTemplates.LoadTemplates( context.EntityTemplates );

    // Register the weapons so we can then set the replacement filenames.
    Weapon_RegisterWeaponData();

    g_HudReplacements.HudReplacementFileName = std::move( context.HudReplacementFile );
    g_HudReplacements.SetWeaponHudReplacementFiles( std::move( context.WeaponHudReplacementFiles ) );

    g_PersistentInventory.MapInit();

    sentences::g_Sentences.MapInit();

    g_GameMode->OnMapInit();

    const auto timeElapsed = std::chrono::high_resolution_clock::now() - start;

    g_GameLogger->trace( "Server configurations loaded in {}ms",
        std::chrono::duration_cast<std::chrono::milliseconds>( timeElapsed ).count() );
}

void ServerLibrary::SendFogMessage( CBasePlayer* player )
{
    MESSAGE_BEGIN( MSG_ONE, gmsgFog, nullptr, player );

    auto fog = static_cast<CClientFog*>( UTIL_FindEntityByClassname( nullptr, "env_fog" ) );

    if( fog )
    {
        CBaseEntity::Logger->debug( "Map has fog!" );

        // TODO: Can probably send color as bytes instead.
        WRITE_SHORT( fog->pev->rendercolor.x );
        WRITE_SHORT( fog->pev->rendercolor.y );
        WRITE_SHORT( fog->pev->rendercolor.z );
        WRITE_COORD( fog->m_Density );
        WRITE_COORD( fog->m_StartDistance );
        WRITE_COORD( fog->m_StopDistance );
        WRITE_SHORT( fog->pev->spawnflags );
    }
    else
    {
        WRITE_SHORT( 0 );
        WRITE_SHORT( 0 );
        WRITE_SHORT( 0 );
        WRITE_COORD( 0 );
        WRITE_COORD( 0 );
        WRITE_COORD( 0 );
        WRITE_SHORT( 0 );
    }

    MESSAGE_END();
}

void ServerLibrary::LoadAllMaps( const CommandArgs& args )
{
    if( !m_MapsToLoad.empty() )
    {
        Con_Printf( "Already loading all maps (%u remaining)\nUse sv_stop_loading_all_maps to stop\n",
            m_MapsToLoad.size() );
        return;
    }

    FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;

    const char* fileName = g_pFileSystem->FindFirst("maps/*.bsp", &handle);

    if (fileName != nullptr)
    {
        do
        {
            std::string mapName = fileName;
            mapName.resize(mapName.size() - 4);

            if (std::find_if(m_MapsToLoad.begin(), m_MapsToLoad.end(), [=](const auto& candidate)
                    { return 0 == stricmp(candidate.c_str(), mapName.c_str()); }) == m_MapsToLoad.end())
            {
                m_MapsToLoad.push_back(std::move(mapName));
            }
        } while ((fileName = g_pFileSystem->FindNext(handle)) != nullptr);

        g_pFileSystem->FindClose(handle);

        // Sort in reverse order so the first map in alphabetic order is loaded first.
        std::sort(m_MapsToLoad.begin(), m_MapsToLoad.end(), [](const auto& lhs, const auto& rhs)
            { return rhs < lhs; });
    }

    if (!m_MapsToLoad.empty())
    {
        if (args.Count() == 2)
        {
            const char* firstMapToLoad = args.Argument(1);

            // Clear out all maps that would have been loaded before this one.
            if (auto it = std::find(m_MapsToLoad.begin(), m_MapsToLoad.end(), firstMapToLoad);
                it != m_MapsToLoad.end())
            {
                const std::size_t numberOfMapsToSkip = m_MapsToLoad.size() - (it - m_MapsToLoad.begin());

                m_MapsToLoad.erase(it + 1, m_MapsToLoad.end());

                Con_Printf("Skipping %u maps to start with \"%s\"\n", numberOfMapsToSkip, m_MapsToLoad.back().c_str());
            }
            else
            {
                Con_Printf("Unknown map \"%s\", starting from beginning\n", firstMapToLoad);
            }
        }

        Con_Printf("Loading %u maps one at a time to generate files\n", m_MapsToLoad.size());

        // Load the first map right now.
        LoadNextMap();
    }
    else
    {
        Con_Printf("No maps to load\n");
    }
}

void ServerLibrary::LoadNextMap()
{
    const std::string mapName = std::move(m_MapsToLoad.back());
    m_MapsToLoad.pop_back();

    Con_Printf("Loading map \"%s\" automatically (%u left)\n", mapName.c_str(), m_MapsToLoad.size() + 1);

    if (m_MapsToLoad.empty())
    {
        Con_Printf("Loading last map\n");
        m_MapsToLoad.shrink_to_fit();
    }

    SERVER_COMMAND(fmt::format("map \"{}\"\n", mapName).c_str());
}

bool ServerLibrary::CanPlayerConnect( edict_t* ent, const char* name, const char* address, char reason[128] )
{
    g_pGameRules->ClientConnected(ent, name, address, reason);

    if( ent )
    {
        int index = g_engfuncs.pfnIndexOfEdict( ent );
        g_GameMode->OnClientConnect(index);
    }

    return true;
}