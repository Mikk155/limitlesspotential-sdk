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

#include "cbase.h"
#include "ConfigurationVariables.h"

#include "ConCommandSystem.h"
#include "GameLibrary.h"
#include "JSONSystem.h"

#ifndef CLIENT_DLL
#include "UserMessages.h"
#else
#include "networking/ClientUserMessages.h"
#endif

using namespace std::literals;

constexpr std::string_view ConfigurationVariablesSchemaName{ "cfgvars"sv };

static std::string GetConfigurationVariablesSchema()
{
    auto GetType = []( ConfigurationVariables::VarTypes type ) -> std::string_view
    {
        switch( type )
        {
            case ConfigurationVariables::VarTypes::String:
                return "string"sv;
            case ConfigurationVariables::VarTypes::Boolean:
                return "boolean"sv;
            case ConfigurationVariables::VarTypes::Integer:
                return "integer"sv;
            case ConfigurationVariables::VarTypes::Float:
            default:
                return "number"sv;
        }
    };

    json variables_object = json::object();

    const std::vector<ConfigurationVariables::Variable>& variables = g_ConfigVars.GetVariables();

    for( const ConfigurationVariables::Variable& variable : variables )
    {
        json variable_object = json::object();

        variable_object[ "type" ] = GetType( variable.type );

        if( variable.flags.Min.has_value() )
            variable_object[ "minimum" ] = *variable.flags.Min;

        if( variable.flags.Max.has_value() )
            variable_object[ "maximum" ] = *variable.flags.Max;

// -TODO Add default value once "GetValue" method is added.
//        variable_object[ "default" ] = variable.default_value;

        variables_object[ variable.name ] = variable_object;
    }

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
            "properties": {}
        }}
    }},
    "additionalProperties": false
}}
)", variables_object.dump() );
}

void ConfigurationVariables::Shutdown()
{
    m_Logger.reset();
}

bool ConfigurationVariables::Initialize()
{
    m_Logger = g_Logging.CreateLogger( "cfgvars" );

    RegisterNetworkedVariables();

#ifndef CLIENT_DLL
    RegisterVariables();
#endif

    m_Logger->info( "Registered {} variables.", m_vars.size() );

    // Last call since variables has to be registered first.
#ifndef CLIENT_DLL
    g_JSON.RegisterSchema( ConfigurationVariablesSchemaName, &GetConfigurationVariablesSchema );
#endif
    return true;
}

ConfigurationVariables::Variable* ConfigurationVariables::RegisterVariable( Variable var )
{
#ifdef CLIENT_DLL
    // The client should only contain networkables
    if( !var.flags.network )
    {
        assert( !"not-networked CFG defined on the client library!" );
        return nullptr;
    }
#endif

#if DEBUG
    if( auto it = std::find_if( m_vars.begin(), m_vars.end(), [&]( const auto& variable ) {
        return variable.name == var.name;
    } ); it != m_vars.end() )
    {
        assert( !"Variable already defined" );
        return nullptr;
    }
#endif

    Variable* pVar = &m_vars.emplace_back( std::move( var ) );

    if( pVar->flags.network )
    {
        // Indexe them to avoid sending variable names through User messages
        m_LastNetworkedVariable++;
        pVar->network_index = m_LastNetworkedVariable;
    }

    m_Logger->debug( "Registered variable \"{}\"", pVar->name );

    return pVar;
}

void ConfigurationVariables::SetVariant( Variable* var, VarVariant value )
{
    float val;
    float old_val;

    switch( var->type )
    {
        case VarTypes::String:
        {
#if DEBUG
            if( !std::holds_alternative<std::string>(value) )
                assert( !"Variable of type String is not std::string" );
#endif
            std::string_view str_old = std::get<std::string>(var->Get());

            std::string str_new = std::get<std::string>(value);

            var->value = std::move( str_new );

            m_Logger->info( "Updated variable \"{}\" from \"{}\" to \"{}\"", var->name, str_old, str_new );

            return;
        }
        case VarTypes::Boolean:
        {
            bool b_old = ( static_cast<int>( std::get<float>(var->Get()) != 0 ) );
            std::string_view f_old = ( b_old ? "true" : "false" );

            var->value = std::max( std::min( 0.f, std::get<float>(value) ), 1.f );

            std::string_view f_new = ( static_cast<int>( std::get<float>(var->Get()) != 0 ) ? "true" : "false" );

            m_Logger->info( "Updated variable \"{}\" from \"{}\" to \"{}\"", var->name, f_old, f_new );

            return;
        }
        case VarTypes::Integer:
        {
            old_val = std::get<float>(var->Get());
            val = std::round( std::get<float>(value) );
            break;
        }
        case VarTypes::Float:
        default:
        {
            old_val = std::get<float>(var->Get());
            val = std::get<float>(value);
            break;
        }
    }

    if( var->flags.Min.has_value() && var->flags.Max.has_value() )
    {
        if( *var->flags.Min > *var->flags.Max )
        {
            std::swap( *var->flags.Min, *var->flags.Max );
            m_Logger->warn( "Variable \"{}\" has inverted bounds. fixed {}-{}", var->name, *var->flags.Min, *var->flags.Max );
        }
    }

    if( var->flags.Min.has_value() )
    {
        val = std::max( *var->flags.Min, val );
    }

    if( var->flags.Max.has_value() )
    {
        val = std::min( *var->flags.Max, val );
    }

    m_Logger->info( "Updated variable \"{}\" from \"{}\" to \"{}\"", var->name, old_val, val );
}

void ConfigurationVariables::RegisterNetworkedVariables()
{
    RegisterVariable( Variable( "fall_damage", 0.22522522522f, VarTypes::Float, VarFlags{ .network = true }) );
}

#ifndef CLIENT_DLL
void ConfigurationVariables::RegisterVariables()
{
}
#endif