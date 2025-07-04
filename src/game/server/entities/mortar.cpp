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

/**
 *    @brief the "LaBuznik" mortar device
 *    Drops bombs from above
 */
class CFuncMortarField : public CBaseToggle
{
    DECLARE_CLASS( CFuncMortarField, CBaseToggle );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;
    bool KeyValue( KeyValueData* pkvd ) override;

    // Bmodels don't go across transitions
    int ObjectCaps() override { return CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

    /**
     *    @brief If connected to a table, then use the table controllers, else hit where the trigger is.
     */
    void FieldUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );

    string_t m_iszXController;
    string_t m_iszYController;
    float m_flSpread;
    float m_flDelay;
    int m_iCount;
    int m_fControl;
};

LINK_ENTITY_TO_CLASS( func_mortar_field, CFuncMortarField );

BEGIN_DATAMAP( CFuncMortarField )
    DEFINE_FIELD( m_iszXController, FIELD_STRING ),
    DEFINE_FIELD( m_iszYController, FIELD_STRING ),
    DEFINE_FIELD( m_flSpread, FIELD_FLOAT ),
    DEFINE_FIELD( m_flDelay, FIELD_FLOAT ),
    DEFINE_FIELD( m_iCount, FIELD_INTEGER ),
    DEFINE_FIELD( m_fControl, FIELD_INTEGER ),
    DEFINE_FUNCTION( FieldUse ),
END_DATAMAP();

bool CFuncMortarField::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_iszXController" ) )
    {
        m_iszXController = ALLOC_STRING( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_iszYController" ) )
    {
        m_iszYController = ALLOC_STRING( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_flSpread" ) )
    {
        m_flSpread = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_fControl" ) )
    {
        m_fControl = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_iCount" ) )
    {
        m_iCount = atoi( pkvd->szValue );
        return true;
    }

    return false;
}

SpawnAction CFuncMortarField::Spawn()
{
    pev->solid = SOLID_NOT;
    SetModel( STRING( pev->model ) ); // set size and link into world
    pev->movetype = MOVETYPE_NONE;
    SetBits( pev->effects, EF_NODRAW );
    SetUse( &CFuncMortarField::FieldUse );
    Precache();

    return SpawnAction::Spawn;
}

void CFuncMortarField::Precache()
{
    PrecacheSound( "weapons/mortar.wav" );
    PrecacheSound( "weapons/mortarhit.wav" );
    PrecacheModel( "sprites/lgtning.spr" );
}

void CFuncMortarField::FieldUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    Vector vecStart;

    vecStart.x = RANDOM_FLOAT( pev->mins.x, pev->maxs.x );
    vecStart.y = RANDOM_FLOAT( pev->mins.y, pev->maxs.y );
    vecStart.z = pev->maxs.z;

    switch ( m_fControl )
    {
    case 0: // random
        break;
    case 1: // Trigger Activator
        if( pActivator != nullptr )
        {
            vecStart.x = pActivator->pev->origin.x;
            vecStart.y = pActivator->pev->origin.y;
        }
        break;
    case 2: // table
    {
        CBaseEntity* pController;

        if( !FStringNull( m_iszXController ) )
        {
            pController = UTIL_FindEntityByTargetname( nullptr, STRING( m_iszXController ) );
            if( pController != nullptr )
            {
                vecStart.x = pev->mins.x + pController->pev->ideal_yaw * ( pev->size.x );
            }
        }
        if( !FStringNull( m_iszYController ) )
        {
            pController = UTIL_FindEntityByTargetname( nullptr, STRING( m_iszYController ) );
            if( pController != nullptr )
            {
                vecStart.y = pev->mins.y + pController->pev->ideal_yaw * ( pev->size.y );
            }
        }
    }
    break;
    }

    int pitch = RANDOM_LONG( 95, 124 );

    EmitSoundDyn( CHAN_VOICE, "weapons/mortar.wav", 1.0, ATTN_NONE, 0, pitch );

    float t = 2.5;
    for( int i = 0; i < m_iCount; i++ )
    {
        Vector vecSpot = vecStart;
        vecSpot.x += RANDOM_FLOAT( -m_flSpread, m_flSpread );
        vecSpot.y += RANDOM_FLOAT( -m_flSpread, m_flSpread );

        TraceResult tr;
        UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -1 ) * 4096, ignore_monsters, edict(), &tr );

        CBaseEntity* pMortar = Create( "monster_mortar", tr.vecEndPos, Vector( 0, 0, 0 ), pActivator );
        pMortar->pev->nextthink = gpGlobals->time + t;
        t += RANDOM_FLOAT( 0.2, 0.5 );

        if( i == 0 )
            CSoundEnt::InsertSound( bits_SOUND_DANGER, tr.vecEndPos, 400, 0.3 );
    }
}

class CMortar : public CGrenade
{
    DECLARE_CLASS( CMortar, CGrenade );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;

    void MortarExplode();

    int m_spriteTexture;
};

BEGIN_DATAMAP( CMortar )
    DEFINE_FUNCTION( MortarExplode ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( monster_mortar, CMortar );

SpawnAction CMortar::Spawn()
{
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_NOT;

    pev->dmg = 200;

    SetThink( &CMortar::MortarExplode );
    pev->nextthink = 0;

    Precache();

    return SpawnAction::Spawn;
}

void CMortar::Precache()
{
    m_spriteTexture = PrecacheModel( "sprites/lgtning.spr" );
}

void CMortar::MortarExplode()
{
#if 1
    // mortar beam
    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    WRITE_BYTE( TE_BEAMPOINTS );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 1024 );
    WRITE_SHORT( m_spriteTexture );
    WRITE_BYTE( 0 );     // framerate
    WRITE_BYTE( 0 );     // framerate
    WRITE_BYTE( 1 );     // life
    WRITE_BYTE( 40 );     // width
    WRITE_BYTE( 0 );     // noise
    WRITE_BYTE( 255 ); // r, g, b
    WRITE_BYTE( 160 ); // r, g, b
    WRITE_BYTE( 100 ); // r, g, b
    WRITE_BYTE( 128 ); // brightness
    WRITE_BYTE( 0 );     // speed
    MESSAGE_END();
#endif

#if 0
    // blast circle
    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    WRITE_BYTE( TE_BEAMTORUS );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 32 );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 32 + pev->dmg * 2 / .2 ); // reach damage radius over .3 seconds
    WRITE_SHORT( m_spriteTexture );
    WRITE_BYTE( 0 ); // startframe
    WRITE_BYTE( 0 ); // framerate
    WRITE_BYTE( 2 ); // life
    WRITE_BYTE( 12 );  // width
    WRITE_BYTE( 0 );   // noise
    WRITE_BYTE( 255 );   // r, g, b
    WRITE_BYTE( 160 );   // r, g, b
    WRITE_BYTE( 100 );   // r, g, b
    WRITE_BYTE( 255 );    // brightness
    WRITE_BYTE( 0 );        // speed
    MESSAGE_END();
#endif

    TraceResult tr;
    UTIL_TraceLine( pev->origin + Vector( 0, 0, 1024 ), pev->origin - Vector( 0, 0, 1024 ), dont_ignore_monsters, edict(), &tr );

    Explode( &tr, DMG_BLAST | DMG_MORTAR );
    UTIL_ScreenShake( tr.vecEndPos, 25.0, 150.0, 1.0, 750 );

#if 0
    int pitch = RANDOM_LONG( 95, 124 );
    EmitSoundDyn( CHAN_VOICE, "weapons/mortarhit.wav", 1.0, 0.55, 0, pitch );

    // ForceSound( SNDRADIUS_MP5, bits_SOUND_COMBAT );

    // ExplodeModel( pev->origin, 400, g_sModelIndexShrapnel, 30 );

    RadiusDamage( this, GetOwner(), pev->dmg, DMG_BLAST );

    /*
    if ( RANDOM_FLOAT ( 0 , 1 ) < 0.5 )
    {
        UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
    }
    else
    {
        UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
    }
    */

    SetThink( &CMortar::SUB_Remove );
    pev->nextthink = gpGlobals->time + 0.1;
#endif
}

#if 0
void CMortar::ShootTimed( EVARS* pevOwner, Vector vecStart, float time )
{
    CMortar* pMortar = g_EntityDictionary->Create<CMortar>( "monster_mortar" );
    pMortar->Spawn();

    TraceResult tr;
    UTIL_TraceLine( vecStart, vecStart + Vector( 0, 0, -1 ) * 4096, ignore_monsters, ENT( pMortar->pev ), &tr );

    pMortar->pev->nextthink = gpGlobals->time + time;

    pMortar->SetOrigin( tr.vecEndPos );
}
#endif
