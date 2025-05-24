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

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <regex>
#include <tuple>

#include <EASTL/fixed_vector.h>

#include "cbase.h"
#include "ConfigurationSystem.h"

#include "ConCommandSystem.h"
#include "GameLibrary.h"
#include "JSONSystem.h"

#ifndef CLIENT_DLL
#include "UserMessages.h"
#else
#include "networking/ClientUserMessages.h"
#endif

// idk. Random number sent just to tell the client to read or not a string.
#define NETWORK_STRING_VARIABLE -3

using namespace std::literals;

constexpr std::string_view ConfigSchemaName{"cfg"sv};

constexpr std::string_view ConfigVariableNameRegexPattern{"^([a-zA-Z_](?:[a-zA-Z_0-9]*[a-zA-Z_]))([123]?)$"};
const std::regex ConfigVariableNameRegex{ConfigVariableNameRegexPattern.data(), ConfigVariableNameRegexPattern.length()};

static std::string GetConfigSchema()
{
    return fmt::format( R"(
{{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "Configuration System",
    "type": "object",
    "$ref": "#/$defs/configValue",
    "properties": {{
        "cooperative": {{ "type": "object", "$ref": "#/$defs/configValue" }},
        "multiplayer": {{ "type": "object", "$ref": "#/$defs/configValue"}},
        "ctf": {{ "type": "object", "$ref": "#/$defs/configValue" }},
        "deathmatch": {{ "type": "object", "$ref": "#/$defs/configValue" }},
        "teamplay": {{ "type": "object", "$ref": "#/$defs/configValue" }},
        "$schema": {{ "type": "string" }}
    }},
    "$defs": {{
        "configValue": {{
            "patternProperties": {{
                "{}": {{
                    "oneOf": [ {{ "type": "string" }}, {{ "type": "number" }}, {{ "type": "boolean" }} ],
                    "description": "Configuration value. Could end with a number from 1 to 3 representing the value used for the current skill level."
                }}
            }}
        }}
    }},
    "additionalProperties": false
}}
)",
        ConfigVariableNameRegexPattern );
}

static std::optional<std::tuple<std::string_view, std::optional<SkillLevel>>> TryParseConfigVariableName( std::string_view key, spdlog::logger& logger )
{
    std::match_results<std::string_view::const_iterator> matches;

    if( !std::regex_match( key.begin(), key.end(), matches, ConfigVariableNameRegex ) )
    {
        return {};
    }

    const std::string_view baseName{matches[1].first, matches[1].second};
    const std::string_view skillLevelString{matches[2].first, matches[2].second};

    if( skillLevelString.empty() )
    {
        // Only a name, no skill level.
        return {{baseName, {}}};
    }

    int skillLevel = 0;
    if( const auto result = std::from_chars( skillLevelString.data(), skillLevelString.data() + skillLevelString.length(), skillLevel );
        result.ec != std::errc() )
    {
        if( result.ec != std::errc::result_out_of_range )
        {
            // In case something else goes wrong.
            logger.error( "Invalid configuration variable name \"{}\": {}", key, std::make_error_code( result.ec ).message() );
        }

        return {};
    }

    return {{baseName, static_cast<SkillLevel>( skillLevel )}};
}

bool ConfigurationSystem::Initialize()
{
    m_Logger = g_Logging.CreateLogger( "cfg" );

    g_JSON.RegisterSchema( ConfigSchemaName, &GetConfigSchema );

    g_NetworkData.RegisterHandler( "Configuration", this );

#ifndef CLIENT_DLL
    g_ConCommands.CreateCommand( 
        "cfg_find", [this]( const auto& args )
        {
            if( args.Count() < 2 )
            {
                Con_Printf( "Usage: %s <search_term> [filter]\nUse * to list all keys\nFilters: networkedonly, all\n", args.Argument( 0 ) );
                return;
            }

            const std::string_view searchTerm{args.Argument( 1 )};

            bool networkedOnly = false;

            if( args.Count() >= 3 )
            {
                if( FStrEq( "networkedonly", args.Argument( 2 ) ) )
                {
                    networkedOnly = true;
                }
                else if( !FStrEq( "all", args.Argument( 2 ) ) )
                {
                    Con_Printf( "Unknown filter option\n" );
                    return;
                }
            }

            const auto printer = [=]( const ConfigVariable& variable )
            {
                if( networkedOnly && variable.NetworkIndex == NotNetworkedIndex )
                {
                    return;
                }

                Con_Printf( "%s = %.2f%s\n",
                    variable.Name.c_str(), variable.CurrentValue,
                    variable.NetworkIndex != NotNetworkedIndex ? " (Networked)"  : "" );
            };

            //TODO: maybe replace this with proper wildcard searching at some point.
            if( searchTerm == "*" )
            {
                //List all keys.
                for( const auto& variable : m_ConfigVariables )
                {
                    printer( variable );
                }
            }
            else
            {
                //List all keys that are a full or partial match.
                for( const auto& variable : m_ConfigVariables )
                {
                    if( variable.Name.find( searchTerm ) != std::string::npos )
                    {
                        printer( variable );
                    }
                }
            } },
        CommandLibraryPrefix::No );

    g_ConCommands.CreateCommand( 
        "cfg_set", [this]( const auto& args )
        {
            if( args.Count() != 3 )
            {
                Con_Printf( "Usage: %s <name> <value>\n", args.Argument( 0 ) );
                return;
            }

            const std::string_view name{args.Argument( 1 )};
            const std::string_view valueString{args.Argument( 2 )};

            float value = 0;
            if( const auto result = std::from_chars( valueString.data(), valueString.data() + valueString.length(), value );
                result.ec != std::errc() )
            {
                Con_Printf( "Invalid value\n" );
                return;
            }

            SetValue( name, value ); },
        CommandLibraryPrefix::No );
#else
    g_ClientUserMessages.RegisterHandler( "ConfigVars", &ConfigurationSystem::MsgFunc_ConfigVars, this );
#endif

    return true;
}

void ConfigurationSystem::Shutdown()
{
    m_Logger.reset();
}

void ConfigurationSystem::HandleNetworkDataBlock( NetworkDataBlock& block )
{
    if( block.Mode == NetworkDataMode::Serialize )
    {
        eastl::fixed_string<ConfigVariable*, MaxNetworkedVariables> variables;

        for( auto& variable : m_ConfigVariables )
        {
            if( variable.NetworkIndex != NotNetworkedIndex )
            {
                variables.push_back( &variable );
            }
        }

        std::sort( variables.begin(), variables.end(), []( const auto lhs, const auto rhs )
            { return lhs->NetworkIndex < rhs->NetworkIndex; } );

        block.Data = json::array();

        for( auto variable : variables )
        {
            json varData = json::object();

            varData.emplace( "Name", variable->Name );
            varData.emplace( "Value", variable->CurrentValue );

            // So SendAllNetworkedConfigVars only sends variables that have changed compared to the configured value.
            variable->NetworkedValue = variable->CurrentValue;

            block.Data.push_back( std::move( varData ) );
        }
    }
    else
    {
        m_ConfigVariables.clear();
        m_NextNetworkedIndex = 0;

        for( const auto& varData : block.Data )
        {
            auto name = varData.value<std::string>( "Name", "" );
            const float value = varData.value<float>( "Value", 0.f );

            if( name.empty() )
            {
                block.ErrorMessage = "Invalid configuration variable name";
                return;
            }

            DefineVariable( std::move( name ), value, {.Networked = true} );
        }
    }
}

void ConfigurationSystem::LoadConfigurationFiles()
{
    std::vector<std::string> fileNames;
    fileNames.push_back( "cfg/default_configuration.json" );
    fileNames.push_back( fmt::format( "cfg/maps/{}_cfg.json", STRING( gpGlobals->mapname ) ) );

    // Refresh skill level setting first.
    int iSkill = (int)CVAR_GET_FLOAT( "skill" );

    iSkill = std::clamp( iSkill, static_cast<int>( SkillLevel::Easy ), static_cast<int>( SkillLevel::Hard ) );

    SetSkillLevel( static_cast<SkillLevel>( iSkill ) );

    // Erase all previous data.
    for( auto it = m_ConfigVariables.begin(); it != m_ConfigVariables.end(); )
    {
        if( ( it->Flags & VarFlag_IsExplicitlyDefined ) != 0 )
        {
            it->CurrentValue = it->InitialValue;
            ++it;
        }
        else
        {
            it = m_ConfigVariables.erase( it );
        }
    }
    m_CustomMaps.clear();
    m_CustomMapIndex.clear();

    m_LoadingConfigurationFiles = true;

    for( const auto& fileName : fileNames )
    {
        m_Logger->trace( "Loading {}", fileName );

        if( const auto result = g_JSON.ParseJSONFile( fileName.c_str(),
                {.SchemaName = ConfigSchemaName, .PathID = "GAMECONFIG"},
                [this]( json& input )
                { return ParseConfiguration( input, false ); } );
            !result.value_or( false ) )
        {
            m_Logger->error( "Error loading configuration file \"{}\"", fileName );
        }
        else
        {
            m_skill_files.push_back( std::move( fileName ) );
        }
    }

    m_LoadingConfigurationFiles = false;
}

void ConfigurationSystem::DefineVariable( std::string name, float initialValue, const ConfigVarConstraints& constraints )
{
    auto it = std::find_if( m_ConfigVariables.begin(), m_ConfigVariables.end(), [&]( const auto& variable )
        { return variable.Name == name; } );

    if( it != m_ConfigVariables.end() )
    {
        m_Logger->error( "Cannot define variable \"{}\": already defined", name );
        assert( !"Variable already defined" );
        return;
    }

    ConfigVarConstraints updatedConstraints = constraints;

    int networkIndex = NotNetworkedIndex;

    if( updatedConstraints.Networked )
    {
        if( m_NextNetworkedIndex < LargestNetworkedIndex )
        {
            networkIndex = m_NextNetworkedIndex++;
        }
        else
        {
            m_Logger->error( "Cannot define variable \"{}\": Too many networked variables", name );
            assert( !"Too many networked variables" );
        }
    }

    if( updatedConstraints.Minimum && updatedConstraints.Maximum )
    {
        if( updatedConstraints.Minimum > updatedConstraints.Maximum )
        {
            m_Logger->warn( "Variable \"{}\" has inverted bounds", name );
            std::swap( updatedConstraints.Minimum, updatedConstraints.Maximum );
        }
    }

    initialValue = ClampValue( initialValue, updatedConstraints );

    ConfigVariable variable{
        .Name = std::move( name ),
        .CurrentValue = initialValue,
        .InitialValue = initialValue,
        .Constraints = updatedConstraints,
        .NetworkIndex = networkIndex,
        .Flags = VarFlag_IsExplicitlyDefined};

    m_ConfigVariables.emplace_back( std::move( variable ) );
}

template <typename T>
T ConfigurationSystem::GetValue(
    std::string_view name,
    std::optional<T> defaultValue,
    CBaseEntity* entity
) const
{
    std::optional<ConfigVariable> variable;

#ifndef CLIENT_DLL
    if( entity != nullptr )
    {
        std::vector<ConfigVariable>& config_map = entity->m_ConfigVariables;

        if( const auto it = std::find_if( config_map.begin(), config_map.end(),
            [&]( const auto& variable )
                { return variable.Name == name; } );
                    it != config_map.end() )
                        { variable = *it; }

        if( !variable.has_value() && entity->m_config >= 0 && entity->m_config < (int)m_CustomMaps.size() )
        {
            config_map = m_CustomMaps.at( entity->m_config );

            if( const auto it = std::find_if( config_map.begin(), config_map.end(),
                [&]( const auto& variable )
                    { return variable.Name == name; } );
                        it != config_map.end() )
                            { variable = *it; }
        }
    }
#endif

    if( !variable.has_value() )
    {
        if( const auto it = std::find_if( m_ConfigVariables.begin(), m_ConfigVariables.end(),
            [&]( const auto& variable )
                { return variable.Name == name; } );
                    it != m_ConfigVariables.end() )
                        { variable = *it; }
    }

    if( variable.has_value() )
    {
        if constexpr ( std::is_same_v<T, bool> )
            return ( static_cast<int>( variable.value().CurrentValue ) >= 1 );
        if constexpr ( std::is_same_v<T, int> )
            return static_cast<int>( variable.value().CurrentValue );
        if constexpr ( std::is_same_v<T, float> )
            return variable.value().CurrentValue;
        if constexpr ( std::is_same_v<T, std::string> )
            return variable.value().StringValue;
    }

    m_Logger->debug( "Undefined variable {}{}", name, m_SkillLevel );

    if( !defaultValue.has_value() )
    {
        m_Logger->debug( "Unable to get value for variable \"{}\" and no default value were provided", name );
        assert( !"Unable to get variable. no default value provided!" );
    }

    return defaultValue.value();
}

template float ConfigurationSystem::GetValue<float>( std::string_view name, std::optional<float> defaultValue, CBaseEntity* entity ) const;
template int ConfigurationSystem::GetValue<int>( std::string_view name, std::optional<int> defaultValue, CBaseEntity* entity ) const;
template bool ConfigurationSystem::GetValue<bool>( std::string_view name, std::optional<bool> defaultValue, CBaseEntity* entity ) const;
template std::string ConfigurationSystem::GetValue<std::string>( std::string_view name, std::optional<std::string> defaultValue, CBaseEntity* entity ) const;

void ConfigurationSystem::SetValue( std::string_view name, std::variant<float, int, bool, std::string_view> value, std::optional<CBaseEntity*> target )
{
    std::vector<ConfigVariable>& m_TargetMap = m_ConfigVariables;

#ifndef CLIENT_DLL
    if( target.has_value() )
    {
        if( auto entity = target.value(); entity != nullptr )
        {
            m_TargetMap = entity->m_ConfigVariables;
        }
    }
#endif

    auto it = std::find_if( m_TargetMap.begin(), m_TargetMap.end(), [&]( const auto& variable )
        { return variable.Name == name; } );

    if( it == m_TargetMap.end() )
    {
        ConfigVariable variable{
            .Name = std::string{name},
            .CurrentValue = 0,
            .InitialValue = 0
        };

        m_TargetMap.emplace_back( std::move( variable ) );

        it = m_TargetMap.end() - 1;
    }

    float fValue = 0;

    if( std::holds_alternative<float>(value) )
    {
        fValue = std::get<float>(value);
    }
    else if( std::holds_alternative<int>(value) )
    {
        fValue = std::get<int>(value);
    }
    else if( std::holds_alternative<bool>(value) )
    {
        fValue = ( std::get<bool>(value) ? 1 : 0 );
        it->Constraints.Type = ConfigVarType::Boolean;
    }
    else if( std::holds_alternative<std::string_view>(value) )
    {
        it->StringValue = std::string( std::get<std::string_view>(value) );
        it->Constraints.Type = ConfigVarType::String;
    }

    fValue = ClampValue( fValue, it->Constraints );

    if( it->CurrentValue != fValue )
    {
        m_Logger->debug( "Config value \"{}\" changed to \"{}\"{}",
            name, fValue, it->NetworkIndex != NotNetworkedIndex ? " (Networked)" : "" );

        it->CurrentValue = fValue;

#ifndef CLIENT_DLL
        if( !m_LoadingConfigurationFiles )
        {
            if( it->NetworkIndex != NotNetworkedIndex )
            {
                MESSAGE_BEGIN( MSG_ALL, gmsgConfigVars );
                WRITE_BYTE( it->NetworkIndex );

                if( it->Constraints.Type == ConfigVarType::String )
                {
                    WRITE_FLOAT( NETWORK_STRING_VARIABLE );
                    WRITE_STRING( it->StringValue.c_str() );
                }
                else
                {
                    WRITE_FLOAT( it->CurrentValue );
                }

                MESSAGE_END();
            }
        }
#endif
    }
}

#ifndef CLIENT_DLL
void ConfigurationSystem::SendAllNetworkedConfigVars( CBasePlayer* player )
{
    // Send config vars in bursts.
    const int maxMessageSize = int( MaxUserMessageLength ) / SingleMessageSize;

    int totalMessageSize = 0;

    MESSAGE_BEGIN( MSG_ONE, gmsgConfigVars, nullptr, player );

    for( const auto& variable : m_ConfigVariables )
    {
        if( variable.NetworkIndex == NotNetworkedIndex )
        {
            continue;
        }

        // Player already has these from networked data.
        if( variable.CurrentValue == variable.NetworkedValue )
        {
            continue;
        }

        if( totalMessageSize >= maxMessageSize )
        {
            MESSAGE_END();
            MESSAGE_BEGIN( MSG_ONE, gmsgConfigVars, nullptr, player );
            totalMessageSize = 0;
        }

        WRITE_BYTE( variable.NetworkIndex );

        if( variable.Constraints.Type == ConfigVarType::String )
        {
            WRITE_FLOAT( NETWORK_STRING_VARIABLE );
            WRITE_STRING( variable.StringValue.c_str() );
        }
        else
        {
            WRITE_FLOAT( variable.CurrentValue );
        }

        totalMessageSize += SingleMessageSize;
    }

    MESSAGE_END();
}
#endif

float ConfigurationSystem::ClampValue( float value, const ConfigVarConstraints& constraints )
{
    if( constraints.Type == ConfigVarType::Integer || constraints.Type == ConfigVarType::Boolean )
    {
        // Round value to integer.
        value = int( value );
    }

    if( constraints.Minimum )
    {
        value = std::max( *constraints.Minimum, value );
    }

    if( constraints.Maximum )
    {
        value = std::min( *constraints.Maximum, value );
    }

    return value;
}

bool ConfigurationSystem::ParseConfiguration( json& input, const bool CustomMap )
{
    if( !input.is_object() )
    {
        return false;
    }

    std::vector<ConfigVariable> config_map;

    // Parse gamerule blocks and override outside's variables
    std::unordered_map<std::string_view, bool> gamemodes;
    gamemodes[ "multiplayer" ] = g_GameMode->IsMultiplayer();
    gamemodes[ "cooperative" ] = g_GameMode->IsGamemode( "coop"sv );
    gamemodes[ "ctf" ] = g_pGameRules->IsCTF();
    gamemodes[ "deathmatch" ] = g_GameMode->IsGamemode( "deathmatch"sv );
    gamemodes[ "teamplay" ] = ( g_GameMode->IsGamemode( "teamplay"sv ) );

    for( const auto& gamemode :  gamemodes )
    {
        if( const auto index = input.find( gamemode.first ); index != input.end() )
        {
            if( gamemode.second )
            {
                m_Logger->debug( "File got an override gamemode rule: {}", gamemode.first );
                if( const json& gm_json = input[ gamemode.first ]; gm_json.is_object() )
                    input.update( gm_json );
            }
            input.erase( index );
        }
    }

    for( const auto& item : input.items() )
    {
        const json value = item.value();

        // Get the config variable base name and skill level.
        const auto maybeVariableName = TryParseConfigVariableName( item.key(), *m_Logger );

        if( !maybeVariableName.has_value() )
        {
            continue;
        }

        const auto& variableName = maybeVariableName.value();

        std::string_view name = std::get<0>( variableName );

        const auto& skillLevel = std::get<1>( variableName );

        if( !skillLevel.has_value() || skillLevel.value() == GetSkillLevel() )
        {
            if( CustomMap )
            {
                // -TODO Should move this in within SetValue and just target the right vector.
                auto it = std::find_if( config_map.begin(), config_map.end(), [&]( const auto& variable )
                    { return variable.Name == name; } );

                if( it == config_map.end() )
                {
                    ConfigVariable variable{
                        .Name = std::string(name),
                        .CurrentValue = 0,
                        .InitialValue = 0
                    };
            
                    config_map.emplace_back( std::move( variable ) );
            
                    it = config_map.end() - 1;
                }

                m_Logger->debug( "Config value \"{}\" changed to \"{}\"", name, value.get<float>() );

                it->CurrentValue = ClampValue( value.get<float>(), it->Constraints );
            }
            else
            {
                if( value.is_number() )
                    SetValue( name, value.get<float>() );
                else if( value.is_string() )
                    SetValue( name, value.get<std::string>() );
                else if( value.is_boolean() )
                    SetValue( name, value.get<bool>() );
                else
                    m_Logger->warn( "Config value \"{}\" is not one of float, int, string or boolean.", name );
            }
        }
    }

    if( CustomMap )
    {
        m_CustomMaps.emplace_back( std::move( config_map ) );
    }

    return true;
}

#ifdef CLIENT_DLL
void ConfigurationSystem::MsgFunc_ConfigVars( BufferReader& reader )
{
    const int messageCount = reader.GetSize() / SingleMessageSize;

    for( int i = 0; i < messageCount; ++i )
    {
        [&]()
        {
            const int networkIndex = reader.ReadByte();
            const float value = reader.ReadFloat();

            if( networkIndex < 0 || networkIndex >= m_NextNetworkedIndex )
            {
                m_Logger->error( "Invalid network index {} received for networked config variable", networkIndex );
                return;
            }

            for( auto& variable : m_ConfigVariables )
            {
                if( variable.NetworkIndex == networkIndex )
                {
                    // Don't need to log since the server logs the change. The client only follows its lead.
                    if( (int)value == NETWORK_STRING_VARIABLE )
                    {
                        const char* string_value = reader.ReadString();
                        variable.StringValue = std::string( string_value );
                        variable.Constraints.Type = ConfigVarType::String;
                    }
                    else
                    {
                        variable.CurrentValue = value;
                    }
                    return;
                }
            }

            m_Logger->error( "Could not find networked config variable with index {}", networkIndex );
        }();
    }
}
#endif


#ifndef CLIENT_DLL
int ConfigurationSystem::CustomConfigurationFile( const char* filename )
{
    std::string name = std::string( filename );

    if( m_CustomMapIndex.find( name ) != m_CustomMapIndex.end() )
    {
        return m_CustomMapIndex.at( name );
    }

    int map_index = (int)m_CustomMaps.size();
    m_Logger->trace( "Loading custom configuration \"{}\" at index {}", filename, map_index );

    if( const auto result = g_JSON.ParseJSONFile( fmt::format( "cfg/{}.json", filename ).c_str(),
            {.SchemaName = ConfigSchemaName, .PathID = "GAMECONFIG"},
            [this]( json& input )
            { return ParseConfiguration( input, true ); } );
        !result.value_or( false ) )
    {
        m_Logger->error( "Error loading configuration file \"{}\"", filename );
    }
    else
    {
        m_skill_files.push_back( std::move( fmt::format( "[{}] {}", map_index, name ) ) );
    }

    m_CustomMapIndex[ name ] = map_index;
    return map_index;
}
#endif
