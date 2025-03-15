/***
 *
 *  Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *  This product contains software technology licensed from Id
 *  Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *  All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "cbase.h"

#define NAME_FILTER_LIMIT 32

#define SF_START_ON ( 1 << 0 )

#define FILTER_STATUS_ONLY_LIVING 1
#define FILTER_STATUS_ONLY_DEAD 2

#define RUN_MODE_RUN_ONCE 0
#define RUN_MODE_ONCE_MULTI_THREADED 1
#define RUN_MODE_TOGGLE_ON_OFF 2

class CTriggerEntityIterator : public CBaseDelay
{
    DECLARE_CLASS( CTriggerEntityIterator, CBaseDelay );
    DECLARE_DATAMAP();

    public:
        bool Spawn() override;
        void IteratorThink();
        void IteratorFind();
        void IteratorTrigger( CBaseEntity* pTarget );
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        string_t name_filter;
        string_t classname_filter;
        string_t trigger_after_run;
        int status_filter = 0;
        int run_mode = 0;
        int maximum_runs = 0;
        int current_runs = 0;
        float delay_between_runs = 0.5;
        float delay_between_triggers = 0.0;
        float flNextDelayTrigger = 0.0;
        bool IsThinking = false;
};

BEGIN_DATAMAP( CTriggerEntityIterator )
    DEFINE_FUNCTION( IteratorThink ),
    DEFINE_FIELD( run_mode, FIELD_INTEGER ),
    DEFINE_FIELD( IsThinking, FIELD_BOOLEAN ),
    DEFINE_FIELD( name_filter, FIELD_STRING ),
    DEFINE_FIELD( maximum_runs, FIELD_INTEGER ),
    DEFINE_FIELD( current_runs, FIELD_INTEGER ),
    DEFINE_FIELD( status_filter, FIELD_INTEGER ),
    DEFINE_FIELD( classname_filter, FIELD_STRING ),
    DEFINE_FIELD( trigger_after_run, FIELD_STRING ),
    DEFINE_FIELD( delay_between_runs, FIELD_FLOAT ),
    DEFINE_FIELD( delay_between_triggers, FIELD_FLOAT ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( trigger_entity_iterator, CTriggerEntityIterator );

bool CTriggerEntityIterator :: Spawn()
{
    pev->solid = SOLID_NOT;

    if( FBitSet( pev->spawnflags, SF_START_ON ) )
    {
        SetThink( &CTriggerEntityIterator::IteratorThink );
        IsThinking = true;
        pev->nextthink = gpGlobals->time + 0.1;
    }

    return true;
}

void CTriggerEntityIterator :: IteratorThink()
{
    if( run_mode == RUN_MODE_TOGGLE_ON_OFF && ( maximum_runs == 0 || current_runs < maximum_runs ) )
    {
        pev->nextthink = gpGlobals->time + delay_between_runs;

        if( maximum_runs != 0 )
        {
            current_runs++;
        }
    }

    IteratorFind();
}

bool CTriggerEntityIterator :: KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "maximum_runs" ) )
    {
        maximum_runs = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "name_filter" ) )
    {
        name_filter = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "classname_filter" ) )
    {
        classname_filter = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "status_filter" ) )
    {
        status_filter = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "run_mode" ) )
    {
        run_mode = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "maximum_runs" ) )
    {
        maximum_runs = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "delay_between_triggers" ) )
    {
        delay_between_triggers = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "delay_between_runs" ) )
    {
        delay_between_runs = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "trigger_after_run" ) )
    {
        trigger_after_run = ALLOC_STRING( pkvd->szValue );
    }
    else
    {
        return CBaseDelay::KeyValue( pkvd );
    }

    return true;
}

void CTriggerEntityIterator :: Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( run_mode == RUN_MODE_TOGGLE_ON_OFF )
    {
        bool ShouldThink = true;

        switch( useType )
        {
            case USE_OFF:
                ShouldThink = false;
            break;
            case USE_ON:
                ShouldThink = true;
            break;
            default:
                ShouldThink = !IsThinking;
            break;
        }

        if( ShouldThink )
        {
            if( !IsThinking )
            {
                SetThink( &CTriggerEntityIterator::IteratorThink );
                IsThinking = true;
                pev->nextthink = gpGlobals->time + 0.1;
            }
        }
        else
        {
            SetThink( nullptr );
            IsThinking = false;
        }
    }
    else
    {
        IteratorFind();
    }
}

void CTriggerEntityIterator :: IteratorFind()
{
    if( !FStringNull( name_filter ) )
    {
        for( auto pTarget : UTIL_FindEntitiesByTargetname( STRING( name_filter ) ) )
        {
            if( !FStringNull( classname_filter ) && FStrEq( STRING( classname_filter ), STRING( pTarget->pev->classname ) ) )
                continue;

            if( pTarget != nullptr )
            {
                IteratorTrigger( pTarget );
            }
        }
    }
    else if( !FStringNull( classname_filter ) )
    {
        for( auto pTarget : UTIL_FindEntitiesByClassname( STRING( classname_filter ) ) )
        {
            if( pTarget != nullptr )
            {
                IteratorTrigger( pTarget );
            }
        }
    }

    flNextDelayTrigger = 0.0;

    if( !FStringNull( trigger_after_run ) )
    {
        FireTargets( STRING( trigger_after_run ), this, this, USE_TOGGLE );
    }
}

void CTriggerEntityIterator :: IteratorTrigger( CBaseEntity* pTarget )
{
    if(( !FStringNull( classname_filter ) && !pTarget->ClassnameIs( STRING( classname_filter ) ) )
    || ( !FStringNull( name_filter ) && !FStrEq( STRING( pTarget->pev->targetname ), STRING( name_filter ) ) )
    || ( status_filter == FILTER_STATUS_ONLY_DEAD && pTarget->IsAlive() )
    || ( status_filter == FILTER_STATUS_ONLY_LIVING && !pTarget->IsAlive() )
    ) { return; }

    if( delay_between_triggers > 0.0 )
    {
        flNextDelayTrigger += delay_between_triggers;
        m_flDelay = flNextDelayTrigger;
        SUB_UseTargets( pTarget, m_UseType );
    }
    else
    {
        FireTargets( STRING( pev->target ), pTarget, this, m_UseType );
    }
}
