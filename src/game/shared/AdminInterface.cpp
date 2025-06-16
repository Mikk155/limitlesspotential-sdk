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

#include "AdminInterface.h"
#include "GameLibrary.h"
#include "JSONSystem.h"

#include <algorithm>

#ifdef CLIENT_DLL
#else

constexpr std::string_view AdminInteraceSchemaName{"admins"sv};

static std::string GetAdminInterfaceSchema()
{
    std::string_view SimplePattern{"^[a-z_]+$"};
    std::string_view SteamIDPattern{"^([a-zA-Z_:][a-zA-Z0-9_:]*)([123]?)$"};

    // Get all available commands
    json ValidCommands = json::array();

    const ClientCommandsMap& CommandsMap = g_ClientCommands.GetCommandsMap();

    for( const auto& [ name, Command ] : CommandsMap )
    {
        if( const ClientCommand* CommandPtr = Command.get(); CommandPtr != nullptr )
        {
            if( ( CommandPtr->Flags & ClientCommandFlag::AdminInterface ) != 0 )
            {
                ValidCommands.push_back( name );
            }
        }
    }

    return fmt::format( R"(
{{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "additionalProperties": false,
    "$defs": {{
        "ArrayValues": {{
            "items": {{
                "description": "These items must be role names listened in the 'roles' label",
                "pattern": "{}",
                "type": "string"
            }},
            "minItems": 1,
            "type": "array",
            "uniqueItems": true
        }}
    }},
    "properties": {{
        "default": {{
            "$ref": "#/$defs/ArrayValues",
            "title": "Default permission",
            "description": "Default permissions for anyone"
        }},
        "administrators": {{
            "title": "Administrators",
            "type": "object",
            "patternProperties": {{
                "{}": {{
                    "type": "object",
                    "properties": {{
                        "roles": {{
                            "$ref": "#/$defs/ArrayValues",
                            "title": "Permission roles",
                            "description": "These items must be role names listened in roles.json"
                        }},
                        "name": {{
                            "type": "string",
                            "title": "Identifier name",
                            "description": "This is only used to identify the user. is not required."
                        }}
                    }},
                    "required": [ "roles" ],
                    "title": "Player's Steam ID",
                    "description": "The key must be the Steam ID of a player"
                }}
            }}
        }},
        "roles": {{
            "title": "Permission roles",
            "type": "object",
            "patternProperties": {{
                "{}": {{
                    "description": "The key must be the role name usable in the 'administrators' label",
                    "items": {{
                        "description": "These items must be command names for this role to have permissions",
                        "pattern": "{}",
                        "enum": {},
                        "type": "string"
                    }},
                    "minItems": 1,
                    "type": "array",
                    "uniqueItems": true
                }}
            }}
        }},
        "$schema": {{
            "type": "string"
        }}
    }},
    "required": [ "default" ],
    "title": "Admin Interface Access Permissions",
    "type": "object"
}}
)",
    SimplePattern, SteamIDPattern, SimplePattern, SimplePattern, ValidCommands.dump() );
}
#endif

bool CAdminInterface::Initialize()
{
    m_Logger = g_Logging.CreateLogger( "admin" );

    g_NetworkData.RegisterHandler( "AdminInterface", this );

    return true;
}

void CAdminInterface::Shutdown()
{
    m_Logger.reset();
}

CAdminInterface::StringPtr CAdminInterface::ToStringPool( const std::string& str )
{
    auto it = std::find_if( m_StringPool.begin(), m_StringPool.end(),
    [&]( const auto& strptr ) { return str == *strptr; } );
    
    if( it == m_StringPool.end() )
    {
        StringPtr StringPointer = std::make_shared<std::string>( str );

        m_StringPool.push_back( std::move( StringPointer ) );

        it = m_StringPool.end() - 1;
    }

    return *it;
}

json CAdminInterface::ParsePermissions( json& input )
{
    AdminRoleMap ParsedRoles;

    const json& roles_object = input[ "roles" ];

    for( const auto& [ _RoleName, _Commands ] : roles_object.items() )
    {
        StringPtr RoleName;
        StringPoolList RoleCommands;

        for( const json& _Command : _Commands )
        {
            if( !RoleName ) {
                StringPtr RoleName = ToStringPool( _RoleName );
            }

            std::string _CommandValue = _Command.get<std::string>();

#ifndef CLIENT_DLL
            if( const ClientCommand* clientCommand = g_ClientCommands.Find( _CommandValue ); clientCommand != nullptr )
            {
                if( ( clientCommand->Flags & ClientCommandFlag::AdminInterface ) == 0 )
                {
                    m_Logger->warn( "Failed to add command \"{}\" is not marked with struct flags ClientCommandFlag::AdminInterface in the SDK.", _CommandValue );
                    continue;
                }
            }
            else
            {
                m_Logger->warn( "Can not add command \"{}\" doesn't exists in the SDK", _CommandValue );
                continue;
            }
#endif

            StringPtr Command = ToStringPool( _CommandValue );
            RoleCommands.push_back( Command );
            m_Logger->info( "Added command \"{}\" to role \"{}\"", _CommandValue, _RoleName );
        }

        if( RoleCommands.size() > 0 )
        {
            m_Logger->info( "Registered role \"{}\"", _RoleName );
            ParsedRoles.emplace( _RoleName, std::move( RoleCommands ) );
        }
    }

    if( ParsedRoles.size() <= 0 )
    {
        m_Logger->warn( "No valid roles parsed!" );
        return json::object();
    }

    auto AddCommandToRole = [&, this]( const json& RolesArray, StringPtr LevelName ) -> StringPoolList
    {
        StringPoolList CommandPoolList;

        for( const auto& _RoleName : RolesArray )
        {
            std::string _RoleNameValue = _RoleName.get<std::string>();
            StringPtr RoleName = ToStringPool( _RoleNameValue );

            if( auto ParsedRolesIteration = ParsedRoles.find( *RoleName ); ParsedRolesIteration != ParsedRoles.end() )
            {
                for( const StringPtr& CommandPtr : ParsedRolesIteration->second )
                {
                    if( auto it = std::find_if( CommandPoolList.begin(), CommandPoolList.end(), [&]( const StringPtr& var )
                    { return var == CommandPtr; } ); it == CommandPoolList.end() )
                    {
                        CommandPoolList.push_back( CommandPtr );
                        m_Logger->info( "Added role \"{}\" to admin \"{}\"", _RoleNameValue, *AdminName( LevelName ) );
                    }
                }
            }
            else
            {
                m_Logger->warn( "Unknown role \"{}\"", _RoleNameValue );
            }
        }

        return CommandPoolList;
    };

    const json& admin_object = input[ "administrators" ];

    for( const auto& [ _SteamID, _AdminSection ] : admin_object.items() )
    {
        StringPtr SteamID = ToStringPool( _SteamID );

        if( auto _name = _AdminSection.find( "name" ); _name != _AdminSection.end() )
        {
            std::string name = _name.value().get<std::string>();
            StringPtr AdminName = ToStringPool( name );
            m_AdminNames.emplace( std::string_view( _SteamID ), AdminName );
        }

        if( StringPoolList AdminCommands = AddCommandToRole( _AdminSection[ "roles" ], SteamID ); AdminCommands.size() > 0 )
        {
            if( StringPtr AdminNick = AdminName( SteamID ); *SteamID != *AdminNick )
            {
                m_Logger->info( "Registered administrator \"{}\" ({})", *AdminNick, _SteamID );
            }
            else
            {
                m_Logger->info( "Registered administrator \"{}\"", _SteamID );
            }
            m_ParsedAdmins.emplace( std::string_view( _SteamID ), std::move( AdminCommands ) );
        }
    }

    StringPtr DefaultName = ToStringPool( "default" );

    if( StringPoolList DefaultCommands = AddCommandToRole( input[ "default" ], DefaultName ); DefaultCommands.size() > 0 )
    {
        m_Logger->info( "Registered default permissions" );
        m_ParsedAdmins.emplace( "default"sv, std::move( DefaultCommands ) );
    }

    return ( m_ParsedAdmins.size() > 0 ? input : json::object() );
}

CAdminInterface::StringPtr CAdminInterface::AdminName( StringPtr SteamID )
{
    if( auto it = m_AdminNames.find( * SteamID ); it != m_AdminNames.end() )
    {
        return it->second;
    }
    return SteamID;
}

void CAdminInterface::HandleNetworkDataBlock( NetworkDataBlock& block )
{
    // In singleplayer the system give full access to the host
    if( !g_GameMode->IsMultiplayer() )
    {
        return;
    }

    m_ParsedAdmins.clear();
    m_AdminNames.clear();

    // Clear string pool
    m_StringPool.clear();

    m_Active = false;

    if( block.Mode == NetworkDataMode::Serialize )
    {
#ifndef CLIENT_DLL
        RegisterCommands();

        block.Data = m_Active;

        // The server must reformat first with the available commands only.
        const JSONLoadParameters SchemaParams{ .SchemaName = AdminInteraceSchemaName, .PathID = "GAMECONFIG" };

        if( auto result = g_JSON.ParseJSONFile( "cfg/server/admins.json", SchemaParams,
        [this]( json& input ) { return ParsePermissions( input ); } ); result.has_value() && result.value().size() > 0 )
        {
            m_Active = true;
            block.Data = std::move( result.value() );
        }
        else
        {
            m_Logger->error( "Failed to open and parse \"cfg/server/admins.json\"" );
        }
#endif
    }
    else
    {
        if( block.Data.is_object() )
        {
            m_Active = ( ParsePermissions( block.Data ).size() > 0 );
        }
    }
}

bool CAdminInterface::HasAccess( CBasePlayer* player, std::string_view command )
{
#ifndef CLIENT_DLL
    if( !player ) {
        return false;
    }
#endif

    // Have full access on singleplayer
    if( !g_GameMode->IsMultiplayer() ) {
        return true;
    }

#ifndef CLIENT_DLL
    // Host player has full permisions in a listen server
    if( !IS_DEDICATED_SERVER() && UTIL_GetLocalPlayer() == player ) {
        return true;
    } // -TODO Know if the host is this client
#endif

    if( !m_Active ) // Don't bother if we don't have a list.
        return false;

    auto HasAccessByRole = [&]( const std::string& JsonLabel ) -> bool
    {
        if( auto it = m_ParsedAdmins.find( JsonLabel ); it != m_ParsedAdmins.end() )
        {
            if( StringPoolList RolesPool = it->second; RolesPool.size() > 0 )
            {
                for( const StringPtr& cmd : RolesPool )
                {
                    if( *cmd == command )
                        return true;
                }
            }
        }
        return false;
    };

    if( HasAccessByRole( "default" ) )
        return true;

#ifdef CLIENT_DLL
    SteamID* steamid = GetClientSteamID();
#else
    SteamID* steamid = player->GetSteamID();
#endif

    if( steamid != nullptr )
        return HasAccessByRole( steamid->steamID );

    return false;
}

#ifndef CLIENT_DLL
std::optional<json> CAdminInterface::ParseJson( CBasePlayer* player, std::string text )
{
    if( text.empty() )
    {
        if( player != nullptr )
            UTIL_ConsolePrint( player, "Failed to parse kevyalues. empty string provided\n" );
        return std::nullopt;
    }

    // The engine manages quotes so the players must create a object like:
    // "{ 'key': 'value', 'key2': 'value2' }"
    std::replace( text.begin(), text.end(), '\'', '"' );

    json JsonObject;

    try
    {
        JsonObject = json::parse( text );

        if( JsonObject.size() <= 0 )
        {
            if( player != nullptr )
                UTIL_ConsolePrint( player, "Failed to parse kevyalues. json has no valid data\n" );
            return std::nullopt;
        }

        if( !JsonObject.is_object() )
        {
            if( player != nullptr )
                UTIL_ConsolePrint( player, "Failed to parse kevyalues. json is not an object\n" );
            return std::nullopt;
        }

        return JsonObject;
    }
    catch( const json::parse_error& e )
    {
        UTIL_ConsolePrint( player, "Failed to parse json: {}\n", e.what() );
    }

    return std::nullopt;
}

bool CAdminInterface::ParseKeyvalues( CBasePlayer* player, CBaseEntity* entity, std::optional<json> KeyValuesOpt )
{
    if( FNullEnt( entity ) )
    {
        UTIL_ConsolePrint( player, "Failed to parse kevyalues. nullptr entity\n" );
        return false;
    }

    if( !KeyValuesOpt.has_value() )
    {
        return false;
    }

    json keyvalues = KeyValuesOpt.value();

    auto edict = entity->edict();

    const char* classname = entity->GetClassname();

    bool AnyInitialized = false;

    for( const auto& [ key, jvalue ] : keyvalues.items() )
    {
        if( !jvalue.is_string() )
        {
            UTIL_ConsolePrint( player, "Ignoring value of key \"{}\" is not a string!\n", key );
            continue;
        }

        if( std::string value = jvalue.get<std::string>(); !value.empty() )
        {
            // Skip the classname the same way the engine does.
            if( key == "classname" )
                continue;

            KeyValueData kvd{.szClassName = classname};

            kvd.szKeyName = key.c_str();
            kvd.szValue = value.c_str();
            kvd.fHandled = 0;

            AnyInitialized = true;

            DispatchKeyValue( edict, &kvd );
        }
        else
        {
            UTIL_ConsolePrint( player, "Ignoring value of key \"{}\" is empty!\n", key );
        }
    }

    return AnyInitialized;
}

// Utility for finding player by command arguments
class TargetPlayerIterator
{
    private:

        CBasePlayer* m_pPlayer;
        int m_iNextIndex;
        CBasePlayer* m_Admin = nullptr;
        const char* m_Target = nullptr;

    public:

        static constexpr int FirstPlayerIndex = 1;

        TargetPlayerIterator( const TargetPlayerIterator& ) = default;

        TargetPlayerIterator( CBasePlayer* admin, const char* arg, CBasePlayer* start = nullptr ) :
        m_Admin( admin ), m_Target( arg ), m_pPlayer( start ), m_iNextIndex( start != nullptr ? start->entindex() + 1 : FirstPlayerIndex ) { }

        TargetPlayerIterator() : m_pPlayer( nullptr ), m_iNextIndex( gpGlobals->maxClients + 1 ) { }

        TargetPlayerIterator& operator=( const TargetPlayerIterator& ) = default;

        const CBasePlayer* operator*() const { return m_pPlayer; }

        CBasePlayer* operator*() { return m_pPlayer; }

        CBasePlayer* operator->() { return m_pPlayer; }

        void operator++()
        {
            m_pPlayer = FindNextPlayer( m_Admin, m_Target, m_iNextIndex, &m_iNextIndex );
        }

        void operator++(int)
        {
            ++*this;
        }

        bool operator==( const TargetPlayerIterator& other ) const
        {
            return m_pPlayer == other.m_pPlayer;
        }

        bool operator!=( const TargetPlayerIterator& other ) const
        {
            return !( *this == other );
        }

        static CBasePlayer* FindNextPlayer( CBasePlayer* admin, const char* arg, int index, int* pOutNextIndex = nullptr )
        {
            auto IsValid = [&]( CBasePlayer* player ) -> bool
            {
                if( !player || player == nullptr )
                    return false;

                if( FStrEq( arg, "@all" ) )
                    return true;
                if( FStrEq( arg, "@me" ) )
                    return ( player == admin );
                if( FStrEq( arg, "@dead" ) )
                    return ( !player->IsAlive() );
                if( FStrEq( arg, "@alive" ) )
                    return ( player->IsAlive() );
                if( strstr( arg, "STEAM_0:" ) != nullptr )
                    return ( player->GetSteamID()->Equals( arg ) );
                // Nickname assumed
                return ( FStrEq( STRING( player->pev->netname ), arg ) ); //-TODO Wildcarding as AFBase?
            };

            while( index <= gpGlobals->maxClients )
            {
                if( !arg || arg == nullptr || arg[0] == '\0' )
                {
                    break;
                }

                if( CBasePlayer* player = UTIL_PlayerByIndex( index ); IsValid( player ) )
                {
                    if( pOutNextIndex )
                    {
                        *pOutNextIndex = index + 1;
                    }

                    return player;
                }

                ++index;
            }

            if( index > gpGlobals->maxClients )
            {
                UTIL_ConsolePrint( admin, "Failed to find a target player!\n" );
            }

            if( pOutNextIndex )
            {
                *pOutNextIndex = gpGlobals->maxClients;
            }

            return nullptr;
        }
};

class TargetPlayerEnumerator
{
    private:

        CBasePlayer* m_Start = nullptr;
        CBasePlayer* m_Admin = nullptr;
        const char* m_Target = nullptr;

        void ConstructArguments( const CommandArgs& args, int index )
        {
            if( args.Count() > index )
            {
                m_Target = args.Argument( index );
            }
            else
            {
                if( m_Admin != nullptr )
                {
                    //-TODO Move to titles?
                    UTIL_ConsolePrint( m_Admin, "No target argument\nUsage:\n@all : All players\n@me : The executor of the command\n@dead : Dead players\n" );
                    UTIL_ConsolePrint( m_Admin, "@alive Alive players\n\"STEAMID_*\" : SteamID Owner\n\"Player nickname\" : Nickname match\n" );
                }
            }
        }

    public:

        TargetPlayerEnumerator( const TargetPlayerEnumerator& ) = default;

        TargetPlayerEnumerator( CBasePlayer* admin, const CommandArgs& args, int index, CBasePlayer* start = nullptr ) :
        m_Admin( admin ), m_Start( start )
        {
            ConstructArguments( args, index );
        }

        TargetPlayerIterator begin()
        {
            return { m_Admin, m_Target, TargetPlayerIterator::FindNextPlayer( m_Admin, m_Target, TargetPlayerIterator::FirstPlayerIndex ) };
        }

        TargetPlayerIterator end()
        {
            return {};
        }
};

void CAdminInterface::RegisterCommands()
{
    if( m_binitialized )
        return;

    m_binitialized = true;

    CClientCommandCreateArguments fCheats{ .Flags = ( ClientCommandFlag::Cheat | ClientCommandFlag::AdminInterface ) };
    CClientCommandCreateArguments fDefault{ .Flags = ( ClientCommandFlag::AdminInterface ) };

    g_ClientCommands.Create( "give"sv, []( CBasePlayer* player, const CommandArgs& args )
    {
        bool ShowInfo = true;

        if( args.Count() > 1 )
        {
            // Make a copy of the classname
            string_t iszItem = ALLOC_STRING( args.Argument( 1 ) );

            std::optional<json> HasAnyKeyvalues = std::nullopt;

            if( args.Count() == 4 )
            {
                HasAnyKeyvalues = g_AdminInterface.ParseJson( player, args.Argument( 3 ) );
            }

            for( auto target : TargetPlayerEnumerator( player, args, 2 ) )
            {
                if( target != nullptr )
                {
                    auto entity = g_ItemDictionary->Create( STRING( iszItem ) );

                    if( FNullEnt( entity ) )
                    {
                        UTIL_ConsolePrint( player, "Failed to create. nullptr entity\n" );
                        break; // Invalid classname? Don't flood the console.
                    }

                    entity->pev->origin = target->pev->origin;
                    entity->m_RespawnDelay = ITEM_NEVER_RESPAWN_DELAY;

                    if( args.Count() == 4 && !g_AdminInterface.ParseKeyvalues( player, entity, HasAnyKeyvalues ) )
                    {
                        g_engfuncs.pfnRemoveEntity( entity->edict() );
                        break; // Invalid object? Don't flood the console.
                    }

                    DispatchSpawn( entity->edict() );

                    if( entity->AddToPlayer( target ) != ItemAddResult::Added )
                    {
                        g_engfuncs.pfnRemoveEntity( entity->edict() );
                    }
                    else
                    {
                        UTIL_ConsolePrint( player, "Gave {} a {}\n", UTIL_GetBestEntityName( target ), UTIL_GetBestEntityName( entity ) );
                    }
                }
            }
        }
        else
        {
            UTIL_ConsolePrint( player, "Usage: give <classname> <target> ?\"{ '<key>': '<value>', '<key2>': '<value2>' }\"\n" );
        }
    }, fCheats );

    g_ClientCommands.Create( "ent_trigger"sv, []( CBasePlayer* player, const CommandArgs& args )
    {
        USE_TYPE useType = ( args.Count() > 1 ? ToUseType( args.Argument( 1 ), USE_TOGGLE ) :  USE_TOGGLE );

        if( args.Count() > 1 )
        {
            useType = static_cast<USE_TYPE>( std::clamp(  atoi( args.Argument( 1 ) ), USE_UNSET + 1, USE_UNKNOWN - 1 ) );
        }

        UseValue value;

        if( args.Count() > 2 )
        {
            value.Float = atof( args.Argument( 2 ) );
        }

        int FiredEntities = 0;

        if( args.Count() > 3 )
        {
            CBaseEntity* entity = nullptr;

            do
            {
                entity = UTIL_FindEntityByIdentifier( entity, args.Argument( 3 ) );

                if( entity != nullptr )
                {
                    entity->Use( player, player, useType, value );
                    FiredEntities++;
                }
            }
            while( entity != nullptr );
        }
        // Trace forward
        else
        {
            Vector forward;
            AngleVectors( player->pev->v_angle, &forward, nullptr, nullptr );

            Vector origin = player->pev->origin + forward * MAX_EXTENT;

            TraceResult tr;
            UTIL_TraceLine( player->pev->origin + player->pev->view_ofs, origin, dont_ignore_monsters, player->edict(), &tr );

            if( !FNullEnt( tr.pHit ) )
            {
                if( auto entity =  CBaseEntity::Instance( tr.pHit ); entity != nullptr )
                {
                    entity->Use( player, player, useType, value );
                    FiredEntities++;
                }
            }
        }

        if( FiredEntities == 0 )
        {
            UTIL_ConsolePrint( player, "Couldn't find any matching entity\n" );
            UTIL_ConsolePrint( player, "Usage: ent_trigger ?<use type> ?<use value> ?<targetname or classname>\n" );
            UTIL_ConsolePrint( player, "if no targetname or classname provided it will attempt to trace forwards\n" );
        }
        else
        {
            UTIL_ConsolePrint( player, "Fired {} {}\n", FiredEntities, FiredEntities > 1 ? "entities" : "entity" );
        }
    }, fDefault );

    // Last call since ClientCommands needs to be initialize first
    g_JSON.RegisterSchema( AdminInteraceSchemaName, &GetAdminInterfaceSchema );
}
#endif
