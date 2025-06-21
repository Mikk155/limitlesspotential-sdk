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

/**
 *    @file
 *    frequently used global functions
 */

#include "cbase.h"
#include "nodes.h"
#include "doors.h"

SpawnAction CPointEntity::Spawn()
{
    pev->solid = SOLID_NOT;
    //    SetSize(g_vecZero, g_vecZero);

    return SpawnAction::Spawn;
}

/**
 *    @brief Null Entity, remove on startup
 */
class CNullEntity : public CBaseEntity
{
public:
    SpawnAction Spawn() override;
};

SpawnAction CNullEntity::Spawn()
{
    return SpawnAction::RemoveNow;
}

LINK_ENTITY_TO_CLASS( info_null, CNullEntity );

void CBaseEntity::UpdateOnRemove()
{
    // tell owner (if any) that we're dead.This is mostly for MonsterMaker functionality.
    MaybeNotifyOwnerOfDeath();

    if( FBitSet( pev->flags, FL_GRAPHED ) )
    {
        // this entity was a LinkEnt in the world node graph, so we must remove it from
        // the graph since we are removing it from the world.
        for( int i = 0; i < WorldGraph.m_cLinks; i++ )
        {
            if( WorldGraph.m_pLinkPool[i].m_pLinkEnt == pev )
            {
                // if this link has a link ent which is the same ent that is removing itself, remove it!
                WorldGraph.m_pLinkPool[i].m_pLinkEnt = nullptr;
            }
        }
    }
    if( !FStringNull( pev->globalname ) )
        gGlobalState.EntitySetState( pev->globalname, GLOBAL_DEAD );
}

void CBaseEntity::SUB_Remove()
{
    UpdateOnRemove();
    if( pev->health > 0 )
    {
        // this situation can screw up monsters who can't tell their entity pointers are invalid.
        pev->health = 0;
        Logger->debug( "SUB_Remove called on entity \"{}\" ({}) with health > 0", STRING( pev->targetname ), STRING( pev->classname ) );
    }

    REMOVE_ENTITY( edict() );
}

BEGIN_DATAMAP(CBaseDelay)
    DEFINE_FIELD( m_flDelay, FIELD_FLOAT ),
    DEFINE_FIELD( m_iszKillTarget, FIELD_STRING ),
    DEFINE_FUNCTION( DelayThink ),
END_DATAMAP();

bool CBaseDelay::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "delay" ) )
    {
        m_flDelay = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "killtarget" ) )
    {
        m_iszKillTarget = ALLOC_STRING( pkvd->szValue );
        return true;
    }

    return CBaseEntity::KeyValue( pkvd );
}

void CBaseEntity::SUB_UseTargets( CBaseEntity* pActivator, USE_TYPE useType, UseValue value )
{
    if( !FStringNull( pev->target ) )
    {
        FireTargets( STRING( pev->target ), pActivator, this, useType, value );
    }
}

USE_TYPE ToUseType( std::variant<int, const char*> from, std::optional<USE_TYPE> default_value )
{
    auto itout = [&]( int value ) -> USE_TYPE
    {
        if( default_value.has_value() )
        {
            if( value >= USE_UNKNOWN || value <= USE_UNSET )
            {
                return default_value.value();
            }
        }

        return static_cast<USE_TYPE>( std::clamp( value, USE_UNSET + 1, USE_UNKNOWN - 1 ) ) ;
    };

    if( std::holds_alternative<int>( from ) )
    {
        return itout( std::get<int>( from ) );
    }
    else
    {
        const char* value = std::get<const char*>( from );

        if( FStrEq( value, "USE_OFF" ) )
            return USE_OFF;
        if( FStrEq( value, "USE_SET" ) )
            return USE_SET;
        if( FStrEq( value, "USE_TOGGLE" ) )
            return USE_TOGGLE;
        if( FStrEq( value, "USE_KILL" ) )
            return USE_KILL;
        if( FStrEq( value, "USE_SAME" ) )
            return USE_SAME;
        if( FStrEq( value, "USE_OPPOSITE" ) )
            return USE_OPPOSITE;
        if( FStrEq( value, "USE_TOUCH" ) )
            return USE_TOUCH;
        if( FStrEq( value, "USE_LOCK" ) )
            return USE_LOCK;
        if( FStrEq( value, "USE_UNLOCK" ) )
            return USE_UNLOCK;

        return itout( atoi( value ) );
    }

    return USE_TOGGLE;
}

void FireTargets( const char* target, CBaseEntity* activator, CBaseEntity* caller, USE_TYPE use_type, UseValue value )
{
    if( !target )
        return;

    // If has semicolon then it's multiple targets
    if( std::strchr( target, ';' ) )
    {
        CBaseEntity::IOLogger->debug( "[FireTargets] Firing multi-targets: ({})", target );

        char buffer[256];
        std::strncpy( buffer, target, sizeof( buffer ) - 1 );
        buffer[ sizeof( buffer ) - 1 ] = '\0';

        char* token = std::strtok( buffer, ";" );

        while( token )
        {
            FireTargets( token, activator, caller, use_type, value );
            token = std::strtok( nullptr, ";" );
        }

        return;
    }

    // if custom USE_TYPE is sent then let's hack it here.
    if( caller != nullptr && caller->m_UseType != USE_UNSET )
    {
        use_type = caller->m_UseType;
    }

    // Do we have a custom USE_TYPE for this specific target?
    if( std::strchr( target, '#' ) != nullptr )
    {
        std::string target_copy = std::string( target );

        auto hash_index = target_copy.find( "#" );

        const char* pszUseType = target_copy.substr( hash_index + 1 ).c_str();

        USE_TYPE use_value = ToUseType( pszUseType, USE_UNSET ); // USE_UNSET = do not clamp, return unset if invalid

        if( use_value == USE_UNKNOWN || use_value == USE_UNSET )
        {
            CBaseEntity::IOLogger->debug( "[FireTargets] Invalid USE_TYPE index #{} at {}. Ignoring custom USE_TYPE...", pszUseType, target );
        }
        else
        {
            // Momentarly override USE_TYPE
            value.UseType = static_cast<USE_TYPE>( use_value );
        }

        FireTargets( target_copy.substr( 0, hash_index ).c_str(), activator, caller, use_type, value );

        value.UseType = std::nullopt;

        return;
    }

    // Should we override the USE_TYPE?
    if( value.UseType.has_value() && value.UseType != USE_UNSET )
    {
        use_type = *value.UseType;
    }

    CBaseEntity::IOLogger->debug( "[FireTargets] Firing: ({})", target );

    // Lambda used for debugging purposes
    auto lUseType = []( USE_TYPE UseType ) -> std::string
    {
        switch( UseType )
        {
            case USE_OFF:       return "USE_OFF (0)";
            case USE_ON:        return "USE_ON (1)";
            case USE_SET:       return "USE_SET (2)";
            case USE_TOGGLE:    return "USE_TOGGLE (3)";
            case USE_KILL:      return "USE_KILL (4)";
            case USE_SAME:      return "USE_SAME (5)";
            case USE_OPPOSITE:  return "USE_OPPOSITE (6)";
            case USE_TOUCH:     return "USE_TOUCH (7)";
            case USE_LOCK:      return "USE_LOCK (8)";
            case USE_UNLOCK:    return "USE_UNLOCK (9)";
        }
        return "USE_UNKNOWN";
    };

    auto lUseLock = []( int value ) -> std::string
    {
        return ( value == 0 ? "USE_VALUE_UNKNOWN" : 
            fmt::format( "( {}{}{}{})",
                ( FBitSet( value, USE_VALUE_MASTER ) ? "Master " : "" ),
                ( FBitSet( value, USE_VALUE_TOUCH ) ? "Touch " : "" ),
                ( FBitSet( value, USE_VALUE_USE ) ? "Use " : "" ),
                ( FBitSet( value, USE_VALUE_THINK ) ? "Think " : "" )
            )
        );
    };

    CBaseEntity* entity = nullptr;

    auto PrintUseValueData = [&]( UseValue value ) -> std::string
    {
        std::ostringstream oss;

        if( value.Float.has_value() ) {
            oss << "Float(" << value.Float.value() << ") ";
        }

        if( value.String.has_value() ) {
            oss << "String(\"" << value.String.value() << "\") ";
        }

        if( value.Vector3D.has_value() ) {
            oss << "Vector(" << value.Vector3D.value().MakeString() << ") ";
        }

        if( value.EntityHandle.has_value() ) {
            if( auto ent = value.EntityHandle.value().Get(); ent != nullptr ) {
                oss << "Entity(" << UTIL_GetBestEntityName( ent ) << ") ";
            }
        }

        if( value.UseType.has_value()) {
            oss << "UseType(" << lUseType( value.UseType.value() ) << ") ";
        }

        return oss.str();
    };

    while( ( entity = UTIL_FindEntityByTargetname( entity, target, activator, caller ) ) != nullptr )
    {
        if( !entity || FBitSet( entity->pev->flags, FL_KILLME ) )
        {
            continue; // Don't use dying ents
        }

        if( FBitSet( entity->m_UseLocked, USE_VALUE_USE ) && use_type != USE_UNLOCK ) // Entity locked for using.
        {
            CBaseEntity::IOLogger->debug( "Entity {} locked from Use. Skipping.", UTIL_GetBestEntityName( entity, false ) );
            continue; // Do this check in here instead so if USE_UNLOCK is sent we catch it
        }

        entity->m_hActivator = activator;

        switch( use_type )
        {
            case USE_ON:
            case USE_OFF:
            case USE_TOGGLE:
            case USE_SET:
            {
                entity->m_UseTypeLast = use_type; // Store the last USE_TYPE that we got.

                CBaseEntity::IOLogger->debug( 
                    "{}->Use( {}, {}, {}, {})",
                    UTIL_GetBestEntityName( entity, false ),
                    UTIL_GetBestEntityName( activator, false ),
                    UTIL_GetBestEntityName( caller, false ),
                    lUseType( use_type ),
                    PrintUseValueData( value )
                );

                entity->Use( activator, caller, use_type, value );

                break;
            }
            case USE_KILL:
            {
                entity->m_UseTypeLast = USE_KILL;

                CBaseEntity::IOLogger->debug( "{}->UpdateOnRemove()", UTIL_GetBestEntityName( entity, false ) );

                UTIL_Remove( entity );

                break;
            }
            case USE_SAME:
            {
                if( !caller || caller == nullptr || caller->m_UseTypeLast == USE_UNSET || caller->m_UseTypeLast == USE_SAME ) // Avoid repeating
                {
                    CBaseEntity::IOLogger->debug( "Got {} on caller! setting as USE_TOGGLE.", ( caller != nullptr ? lUseType( caller->m_UseTypeLast ) : "nullptr" ) );
                    entity->m_UseTypeLast = USE_TOGGLE;
                }
                else
                {
                    entity->m_UseTypeLast = caller->m_UseTypeLast;
                }

                CBaseEntity::IOLogger->debug( 
                    "{}->Use( {}, {}, {} > {}, {})",
                    UTIL_GetBestEntityName( entity, false ),
                    UTIL_GetBestEntityName( activator, false ),
                    UTIL_GetBestEntityName( caller, false ),
                    lUseType( USE_SAME ),
                    lUseType( entity->m_UseTypeLast ),
                    PrintUseValueData( value )
                );

                entity->Use( activator, caller, entity->m_UseTypeLast, value );

                break;
            }
            case USE_OPPOSITE:
            {
                entity->m_UseTypeLast = USE_OPPOSITE;

                if( !caller || caller == nullptr )
                {
                    CBaseEntity::IOLogger->debug( "Can't invert USE_TYPE with a null caller! Skipping." );
                    break;
                }

                if( caller->m_UseTypeLast != USE_ON
                && caller->m_UseTypeLast != USE_OFF
                && caller->m_UseTypeLast != USE_LOCK
                && caller->m_UseTypeLast != USE_UNLOCK )
                {
                    CBaseEntity::IOLogger->debug( 
                        "Can't invert USE_TYPE {}. only {}, {}, {} and {} are supported! Skipping.",
                        lUseType( entity->m_UseTypeLast ),
                        lUseType( USE_ON ),
                        lUseType( USE_OFF ),
                        lUseType( USE_LOCK ),
                        lUseType( USE_UNLOCK )
                    );
                    break;
                }

                // will be USE_OPPOSITE only if failed.
                entity->m_UseTypeLast = ( caller->m_UseTypeLast == USE_ON ? USE_OFF : caller->m_UseTypeLast == USE_OFF ? USE_ON :
                    caller->m_UseTypeLast == USE_LOCK ? USE_UNLOCK : USE_LOCK );

                CBaseEntity::IOLogger->debug( 
                    "{}->Use( {}, {}, {} > {}, {})",
                    UTIL_GetBestEntityName( entity, false ),
                    UTIL_GetBestEntityName( activator, false ),
                    UTIL_GetBestEntityName( caller, false ),
                    lUseType( USE_OPPOSITE ),
                    lUseType( entity->m_UseTypeLast ),
                    PrintUseValueData( value )
                );

                entity->Use( activator, caller, entity->m_UseTypeLast );
                break;
            }
            case USE_TOUCH:
            {
                entity->m_UseTypeLast = USE_TOUCH;

                CBaseEntity::IOLogger->debug( "{}->Touch( {})", UTIL_GetBestEntityName( entity, false ), UTIL_GetBestEntityName( activator, false ) );

                if( FBitSet( entity->m_UseLocked, USE_VALUE_TOUCH ) ) // Entity locked for Touching.
                {
                    CBaseEntity::IOLogger->debug( "Entity locked from Touch. Skipping." );
                    break;
                }

                entity->Touch( activator );

                break;
            }
            case USE_LOCK:
            {
                entity->m_UseTypeLast = USE_LOCK;

                if( !caller || caller == nullptr )
                {
                    CBaseEntity::IOLogger->debug( "{}->{} but got a nullptr caller! Skipping.", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_LOCK ) );
                    break;
                }

                if( !caller || caller == nullptr )
                {
                    CBaseEntity::IOLogger->debug( "{}->{} but got a zero value on caller! Skipping.", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_LOCK ) );
                    break;
                }

                CBaseEntity::IOLogger->debug( "{}->{} Locked. won't receive any calls until get {}", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_LOCK ), lUseType( USE_UNLOCK ) );

                // -TODO Does this work with multiple? Or i have to manualy remove each one?
                SetBits( entity->m_UseLocked, caller->m_UseLockType );

                break;
            }
            case USE_UNLOCK:
            {
                entity->m_UseTypeLast = USE_UNLOCK;

                if( !caller || caller == nullptr )
                {
                    CBaseEntity::IOLogger->debug( "{}->{} but got a nullptr caller! Skipping.", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_UNLOCK ) );
                    break;
                }

                if( !caller || caller == nullptr )
                {
                    CBaseEntity::IOLogger->debug( "{}->{} but got a zero value on caller! Skipping.", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_UNLOCK ) );
                    break;
                }

                CBaseEntity::IOLogger->debug( "{}->{} Un-Locked. will receive calls now", UTIL_GetBestEntityName( entity, false ), lUseLock( USE_UNLOCK ) );

                ClearBits( entity->m_UseLocked, caller->m_UseLockType );

                break;
            }
            default:
            {
                CBaseEntity::IOLogger->debug( 
                    "{}->Use( {}, {}, {})",
                    UTIL_GetBestEntityName( entity, false ),
                    UTIL_GetBestEntityName( activator, false ),
                    UTIL_GetBestEntityName( caller, false ),
                    lUseType( use_type )
                );
    
                entity->m_UseTypeLast = use_type;
                entity->Use( activator, caller, use_type, value );
                break;
            }
        }
    }
}

LINK_ENTITY_TO_CLASS( delayed_use, CBaseDelay );

void CBaseDelay::SUB_UseTargets( CBaseEntity* pActivator, USE_TYPE useType, UseValue value )
{
    //
    // exit immediatly if we don't have a target or kill target
    //
    if( FStringNull( pev->target ) && FStringNull( m_iszKillTarget ) )
        return;

    //
    // check for a delay
    //
    if( m_flDelay != 0 )
    {
        // create a temp object to fire at a later time
        CBaseDelay* pTemp = g_EntityDictionary->Create<CBaseDelay>( "delayed_use" );

        pTemp->pev->nextthink = gpGlobals->time + m_flDelay;

        pTemp->SetThink( &CBaseDelay::DelayThink );

        // Save the useType
        pTemp->pev->button = static_cast<int>( useType );
        pTemp->m_iszKillTarget = m_iszKillTarget;
        pTemp->m_flDelay = 0; // prevent "recursion"
        pTemp->pev->target = pev->target;
        pTemp->m_hActivator = pActivator;

        return;
    }

    //
    // kill the killtargets
    //

    if( !FStringNull( m_iszKillTarget ) )
    {
        CBaseEntity::IOLogger->debug( "KillTarget: {}", STRING( m_iszKillTarget ) );

        CBaseEntity* killTarget = nullptr;

        while( ( killTarget = UTIL_FindEntityByTargetname( killTarget, STRING( m_iszKillTarget ), pActivator, this ) ) != nullptr )
        {
            if( UTIL_IsRemovableEntity( killTarget ) )
            {
                UTIL_Remove( killTarget );
                CBaseEntity::IOLogger->debug( "killing {}", STRING( killTarget->pev->classname ) );
            }
            else
            {
                CBaseEntity::IOLogger->debug( "Can't kill \"{}\": not allowed to remove entities of this type",
                    STRING( killTarget->pev->classname ) );
            }
        }
    }

    //
    // fire targets
    //
    if( !FStringNull( pev->target ) )
    {
        FireTargets( GetTarget(), pActivator, this, useType, value );
    }
}

void CBaseDelay::DelayThink()
{
    // If a player activated this on delay
    CBaseEntity* pActivator = m_hActivator;

    // The use type is cached (and stashed) in pev->button
    SUB_UseTargets( pActivator, ( USE_TYPE )pev->button );
    REMOVE_ENTITY( edict() );
}

BEGIN_DATAMAP(CBaseToggle)
    DEFINE_FIELD( m_toggle_state, FIELD_INTEGER ),
    DEFINE_FIELD( m_flActivateFinished, FIELD_TIME ),
    DEFINE_FIELD( m_flMoveDistance, FIELD_FLOAT ),
    DEFINE_FIELD( m_flWait, FIELD_FLOAT ),
    DEFINE_FIELD( m_flLip, FIELD_FLOAT ),
    DEFINE_FIELD( m_vecPosition1, FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_vecPosition2, FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_vecAngle1, FIELD_VECTOR ), // UNDONE: Position could go through transition, but also angle?
    DEFINE_FIELD( m_vecAngle2, FIELD_VECTOR ), // UNDONE: Position could go through transition, but also angle?
    DEFINE_FIELD( m_pfnCallWhenMoveDone, FIELD_FUNCTIONPOINTER ),
    DEFINE_FIELD( m_vecFinalDest, FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_vecFinalAngle, FIELD_VECTOR ),
    DEFINE_FUNCTION( LinearMoveDone ),
    DEFINE_FUNCTION( AngularMoveDone ),
END_DATAMAP();

bool CBaseToggle::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "lip" ) )
    {
        m_flLip = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "wait" ) )
    {
        m_flWait = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "distance" ) )
    {
        m_flMoveDistance = atof( pkvd->szValue );
        return true;
    }

    return CBaseDelay::KeyValue( pkvd );
}

void CBaseToggle::LinearMove( Vector vecDest, float flSpeed )
{
    ASSERTSZ( flSpeed != 0, "LinearMove:  no speed is defined!" );
    //    ASSERTSZ(m_pfnCallWhenMoveDone != nullptr, "LinearMove: no post-move function defined");

    m_vecFinalDest = vecDest;

    // Already there?
    if( vecDest == pev->origin )
    {
        LinearMoveDone();
        return;
    }

    // set destdelta to the vector needed to move
    Vector vecDestDelta = vecDest - pev->origin;

    // divide vector length by speed to get time to reach dest
    float flTravelTime = vecDestDelta.Length() / flSpeed;

    // set nextthink to trigger a call to LinearMoveDone when dest is reached
    pev->nextthink = pev->ltime + flTravelTime;
    SetThink( &CBaseToggle::LinearMoveDone );

    // scale the destdelta vector by the time spent traveling to get velocity
    pev->velocity = vecDestDelta / flTravelTime;
}

void CBaseToggle::LinearMoveDone()
{
    Vector delta = m_vecFinalDest - pev->origin;
    float error = delta.Length();
    if( error > 0.03125 )
    {
        LinearMove( m_vecFinalDest, 100 );
        return;
    }

    SetOrigin( m_vecFinalDest );
    pev->velocity = g_vecZero;
    pev->nextthink = -1;
    if( m_pfnCallWhenMoveDone )
        ( this->*m_pfnCallWhenMoveDone )();
}

void CBaseToggle::AngularMove( Vector vecDestAngle, float flSpeed )
{
    ASSERTSZ( flSpeed != 0, "AngularMove:  no speed is defined!" );
    //    ASSERTSZ(m_pfnCallWhenMoveDone != nullptr, "AngularMove: no post-move function defined");

    m_vecFinalAngle = vecDestAngle;

    // Already there?
    if( vecDestAngle == pev->angles )
    {
        AngularMoveDone();
        return;
    }

    // set destdelta to the vector needed to move
    Vector vecDestDelta = vecDestAngle - pev->angles;

    // divide by speed to get time to reach dest
    float flTravelTime = vecDestDelta.Length() / flSpeed;

    // set nextthink to trigger a call to AngularMoveDone when dest is reached
    pev->nextthink = pev->ltime + flTravelTime;
    SetThink( &CBaseToggle::AngularMoveDone );

    // scale the destdelta vector by the time spent traveling to get velocity
    pev->avelocity = vecDestDelta / flTravelTime;
}

void CBaseToggle::AngularMoveDone()
{
    pev->angles = m_vecFinalAngle;
    pev->avelocity = g_vecZero;
    pev->nextthink = -1;
    if( m_pfnCallWhenMoveDone )
        ( this->*m_pfnCallWhenMoveDone )();
}

float CBaseToggle::AxisValue( int flags, const Vector& angles )
{
    if( FBitSet( flags, SF_DOOR_ROTATE_Z ) )
        return angles.z;
    if( FBitSet( flags, SF_DOOR_ROTATE_X ) )
        return angles.x;

    return angles.y;
}

void CBaseToggle::AxisDir( CBaseEntity* entity )
{
    if( FBitSet( entity->pev->spawnflags, SF_DOOR_ROTATE_Z ) )
        entity->pev->movedir = Vector( 0, 0, 1 ); // around z-axis
    else if( FBitSet( entity->pev->spawnflags, SF_DOOR_ROTATE_X ) )
        entity->pev->movedir = Vector( 1, 0, 0 ); // around x-axis
    else
        entity->pev->movedir = Vector( 0, 1, 0 ); // around y-axis
}

float CBaseToggle::AxisDelta( int flags, const Vector& angle1, const Vector& angle2 )
{
    if( FBitSet( flags, SF_DOOR_ROTATE_Z ) )
        return angle1.z - angle2.z;

    if( FBitSet( flags, SF_DOOR_ROTATE_X ) )
        return angle1.x - angle2.x;

    return angle1.y - angle2.y;
}

/**
 *    @brief returns true if the passed entity is visible to caller, even if not infront ()
 */
bool FEntIsVisible( CBaseEntity* entity, CBaseEntity* target )
{
    Vector vecSpot1 = entity->pev->origin + entity->pev->view_ofs;
    Vector vecSpot2 = target->pev->origin + target->pev->view_ofs;
    TraceResult tr;

    UTIL_TraceLine( vecSpot1, vecSpot2, ignore_monsters, entity->edict(), &tr );

    if( 0 != tr.fInOpen && 0 != tr.fInWater )
        return false; // sight line crossed contents

    if( tr.flFraction == 1 )
        return true;

    return false;
}
