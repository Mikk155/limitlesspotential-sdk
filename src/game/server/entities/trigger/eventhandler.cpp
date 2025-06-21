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

#include "eventhandler.h"

#include "ServerLibrary.h"

LINK_ENTITY_TO_CLASS( trigger_eventhandler, CTriggerEventHandler );

BEGIN_DATAMAP( CTriggerEventHandler )
    DEFINE_FIELD( m_EventType, FIELD_INTEGER ),
    DEFINE_FIELD( InitialState, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_Caller, FIELD_STRING ),
END_DATAMAP();

SpawnAction CTriggerEventHandler::Spawn()
{
    BaseClass::Spawn();

    if( m_EventType == TriggerEventType::None )
    {
        CBaseEntity::Logger->error( "Unexpecified event type in trigger_eventhandler at {}", pev->origin.MakeString() );
        return SpawnAction::RemoveNow;
    }

    CBaseEntity::Logger->debug( "trigger_eventhandler Registering event type {} for {}", static_cast<int>( m_EventType ), STRING( pev->target ) );

    trigger_event_t event{ m_EventType, entindex() };

    g_Server.m_events.push_back( std::move( event ) );

    InitialState = FBitSet( pev->spawnflags, SF_EVENT_STARTOFF );

    return SpawnAction::Spawn;
}

bool CTriggerEventHandler::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "event_type" ) )
    {
        m_EventType = static_cast<TriggerEventType>( atoi( pkvd->szValue ) );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_Caller" ) )
    {
        m_Caller = ALLOC_STRING( pkvd->szValue );
        return true;
    }

    return BaseClass::KeyValue( pkvd );
};

void CTriggerEventHandler::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    switch( useType )
    {
        case USE_OFF:
            SetBits( pev->spawnflags, SF_EVENT_STARTOFF );
        break;
        case USE_ON:
            ClearBits( pev->spawnflags, SF_EVENT_STARTOFF );
        break;
        case USE_SET:
            if( InitialState )
                SetBits( pev->spawnflags, SF_EVENT_STARTOFF );
            else ClearBits( pev->spawnflags, SF_EVENT_STARTOFF );
        break;
        case USE_TOGGLE:
            if( !FBitSet( pev->spawnflags, SF_EVENT_STARTOFF ) )
                SetBits( pev->spawnflags, SF_EVENT_STARTOFF );
            else ClearBits( pev->spawnflags, SF_EVENT_STARTOFF );
        break;
    }
}

void TriggerEvent( TriggerEventType event, CBaseEntity* activator, CBaseEntity* caller, UseValue value )
{
    for( const trigger_event_t& event_data : g_Server.m_events )
    {
        if( event_data.event == event )
        {
            if( edict_t* ent = INDEXENT( event_data.entity ); !FNullEnt( ent ) && !FBitSet( ent->v.spawnflags, SF_EVENT_STARTOFF ) )
            {
                if( CTriggerEventHandler* handler = static_cast<CTriggerEventHandler*>( CBaseEntity::Instance( ent ) ); handler != nullptr )
                {
                    CBaseEntity* MyNewCaller = caller;

                    if( !MyNewCaller || MyNewCaller == nullptr )
                    {
                        MyNewCaller = handler;
                    }

                    FireTargets( 
                        STRING( handler->pev->target ),
                        handler->AllocNewActivator( activator, MyNewCaller, handler->m_sNewActivator, activator ),
                        handler->AllocNewActivator( activator, MyNewCaller, handler->m_Caller, MyNewCaller ),
                        USE_TOGGLE,
                        value
                    );
                }
            }
        }
    }
}
