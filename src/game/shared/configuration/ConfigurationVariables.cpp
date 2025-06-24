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

void ConfigurationVariables::RegisterNetworkedVariables()
{
    RegisterVariable( Variable( "fall_damage", 0.22522522522f, VarTypes::Float, VarFlags{ .network = true }) );
}

#ifndef CLIENT_DLL
void ConfigurationVariables::RegisterVariables()
{
}
#endif