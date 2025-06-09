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

#include <JSONSystem.h>
#include <algorithm>

#ifdef CLIENT_DLL
#else

constexpr std::string_view AdminsSchemaName{"admin_admins"sv};
constexpr std::string_view RolesSchemaName{"admin_roles"sv};

enum SchemaType
{
    Admins = 0,
    Roles
};

static std::string GetAdminRolesSchema( SchemaType type )
{
    std::string_view RegexItems = "^[a-z_]+$";
    std::string_view RegexType;
    std::string_view ArrayDescription;
    std::string_view ItemsDescription;
    std::string_view ArrayTitle;

    switch( type )
    {
        case SchemaType::Admins:
        {
            ArrayTitle = "Player's Steam ID";
            ArrayDescription = "The key must be the Steam ID of a player";
            ItemsDescription = "These items must be role names listened in roles.json";
            RegexType = "^([a-zA-Z_:][a-zA-Z0-9_:]*)([123]?)$";
            break;
        }
        case SchemaType::Roles:
        default:
        {
            ArrayDescription = "The key must be the role name usable in admins.json";
            ItemsDescription = "These items must be command names for this role to have permissions";
            RegexType = RegexItems;
            break;
        }
    }

    return fmt::format( R"(
{{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "Admin Interface Access Permissions",
    "type": "object",
    "properties": {{
        "$schema": {{ "type": "string" }}
    }},
    "patternProperties": {{
        "{}": {{
            "type": "array",
            "title": "{}",
            "description": "{}",
            "items": {{
                "type": "string",
                "description": "{}",
                "pattern": "{}"
            }},
            "uniqueItems": true,
            "minItems": 1
        }}
    }},
    "additionalProperties": false
}}
)",
    RegexType, ArrayTitle, ArrayDescription, ItemsDescription, RegexItems );
}
static std::string GetAdminSchema() { return GetAdminRolesSchema( SchemaType::Admins ); }
static std::string GetRolesSchema() { return GetAdminRolesSchema( SchemaType::Roles ); }
#endif

bool CAdminInterface::Initialize()
{
    m_Logger = g_Logging.CreateLogger( "admin" );

#ifdef CLIENT_DLL
#else
    g_JSON.RegisterSchema( AdminsSchemaName, &GetAdminSchema );
    g_JSON.RegisterSchema( RolesSchemaName, &GetRolesSchema );

    m_ScopedAdminMenu = g_ClientCommands.CreateScoped( "admin_menu", [this]( auto, const auto& )
    {
        // -Open a menu with available commands
    } );
#endif

    return true;
}

void CAdminInterface::Shutdown()
{
    m_Logger.reset();
}

#ifdef CLIENT_DLL
#else
void CAdminInterface::OnMapInit()
{
    // In singleplayer the system give full access to the host so avoid these readings
    if( !g_GameMode->IsMultiplayer() )
        return;

    m_CommandsPool.clear();
    m_ParsedAdmins.clear();

    m_Logger->info( "Reading \"cfg/server/admin/roles.json\"" );

    AdminRoleMap ParsedRoles;

    if( std::optional<json> json_opt = g_JSON.LoadJSONFile( "cfg/server/admin/roles.json",
        { .SchemaName = RolesSchemaName, .PathID = "GAMECONFIG" } );
            json_opt.has_value() && json_opt.value().is_object() )
    {
        const json& roles_json = json_opt.value();

        for( const auto& [ role, commands ] : roles_json.items() )
        {
            if( !commands.is_array() )
            {
                m_Logger->warn( "Ignoring role \"{}\" is not an array", role );
                continue;
            }

            StringPoolList role_ccmmands;

            for( const auto& cmd : commands )
            {
                if( !cmd.is_string() )
                    continue;

                if( std::string value = cmd.get<std::string>(); !value.empty() )
                {
                    if( auto clientCommand = g_ClientCommands.Find( value ); clientCommand )
                    {
                        if( ( clientCommand->Flags & ClientCommandFlag::AdminInterface ) == 0 )
                        {
                            m_Logger->warn( "Failed to add command \"{}\" is not marked with struct flags ClientCommandFlag::AdminInterface in the SDK.", value );
                            continue;
                        }
                        else if( auto it = m_CommandsPool.find( value ); it == m_CommandsPool.end() )
                        {
                            StringPtr interned = std::make_shared<std::string>( value );

                            if( auto it2 = m_CommandsPool.emplace( *interned, std::move( interned ) ); it2.second )
                            {
                                role_ccmmands.push_back( it2.first->second );
                            }
                            else
                            {
                                m_Logger->warn( "Failed to register command \"{}\"", value );
                                continue;
                            }
                        }

                        m_Logger->debug( "Added command \"{}\" to role \"{}\"", value, role );
                    }
                    else
                    {
                        m_Logger->warn( "Unknown command \"{}\"", value );
                    }
                }
            }

            if( role_ccmmands.size() > 0 )
            {
                m_Logger->debug( "Registered role \"{}\"", role );
                ParsedRoles.emplace( role, std::move( role_ccmmands ) );
            }
        }
    }
    else
    {
        m_Logger->error( "Failed to open \"cfg/server/admin/roles.json\"" );
        return;
    }

    m_Logger->info( "Reading \"cfg/server/admin/admins.json\"" );

    if( std::optional<json> json_opt = g_JSON.LoadJSONFile( "cfg/server/admin/admins.json",
        { .SchemaName = AdminsSchemaName, .PathID = "GAMECONFIG" } );
            json_opt.has_value() && json_opt.value().is_object() )
    {
        const json& roles_json = json_opt.value();

        for( const auto& [ admin, roles ] : roles_json.items() )
        {
            if( !roles.is_array() )
            {
                m_Logger->warn( "Ignoring admin \"{}\" is not an array", admin );
                continue;
            }

            StringPoolList admin_commands;

            for( const auto& role : roles )
            {
                if( !role.is_string() )
                    continue;

                if( std::string value = role.get<std::string>(); !value.empty() )
                {
                    if( auto role_commands = ParsedRoles.find( value ); role_commands != ParsedRoles.end() )
                    {
                        for( const StringPtr& command : role_commands->second )
                        {
                            auto it = std::find_if( admin_commands.begin(), admin_commands.end(), [&]( const StringPtr& var )
                                { return var == command; } );

                            if( it == admin_commands.end() )
                            {
                                admin_commands.push_back( command );
                            }
                        }

                        if( admin == "default" )
                        {
                            m_Logger->debug( "Added role \"{}\" to default permissions.", value );
                        }
                        else
                        {
                            m_Logger->debug( "Added role \"{}\" to admin \"{}\"", value, admin );
                        }
                    }
                    else
                    {
                        m_Logger->warn( "Unknown role \"{}\"", value );
                    }
                }
            }

            if( admin_commands.size() > 0 )
            {
                if( admin == "default" )
                {
                    m_Logger->warn( "Registered default permissions" );
                }
                else
                {
                    m_Logger->warn( "Registered administrator \"{}\"", admin ); // -TODO Should we add nicknames to json?
                }

                m_ParsedAdmins.emplace( admin, std::move( admin_commands ) );
            }
        }
    }
    else
    {
        m_Logger->error( "Failed to open \"cfg/server/admin/admins.json\"" );
        return;
    }
}
#endif

bool CAdminInterface::HasAccess( CBasePlayer* player, std::string_view command )
{
#ifdef CLIENT_DLL
#else
    // This command doesn't exists. Should it log?
    if( m_CommandsPool.find( command ) == m_CommandsPool.end() ) {
        return false;
    }

    if( player != nullptr )
    {
        // Host player has full permisions in a listen server or singleplayer
        if( ( !IS_DEDICATED_SERVER() && UTIL_GetLocalPlayer() == player ) || !g_GameMode->IsMultiplayer() )
        {
            return true;
        }

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

        return ( HasAccessByRole( player->GetSteamID()->SteamFormat() ) || HasAccessByRole( "default" ) );
    }
#endif
    return false;
}

void CAdminInterface::RegisterCommands()
{
    CClientCommandCreateArguments fCheats{ .Flags = ( ClientCommandFlag::Cheat | ClientCommandFlag::AdminInterface ) };
    CClientCommandCreateArguments fDefault{ .Flags = ( ClientCommandFlag::AdminInterface ) };

    g_ClientCommands.Create( "give"sv, []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() > 1 )
        {
            string_t iszItem = ALLOC_STRING( args.Argument( 1 ) ); // Make a copy of the classname
            player->GiveNamedItem( STRING( iszItem ) );
        }
        else
        {
            UTIL_ConsolePrint( player, "Usage: give <classname>\n" );
// This needs a utility method
//            UTIL_ConsolePrint( player, "or give <classname> \"{ \"<key>\": \"<value>\", \"<key2>\": \"<value2>\" }\"\n" );
        }
    }, fCheats );
}
