/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *    This product contains software technology licensed from Id
 *    Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *    All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/
#include "cbase.h"

#include "weapons/CDisplacerBall.h"
#include "weapons/CShockBeam.h"
#include "weapons/CSpore.h"

class CBlowerCannon : public CBaseToggle
{
    DECLARE_CLASS( CBlowerCannon, CBaseToggle );
    DECLARE_DATAMAP();

private:
    enum class WeaponType
    {
        SporeRocket = 1,
        SporeGrenade,
        ShockBeam,
        DisplacerBall
    };

    enum class FireType
    {
        Toggle = 1,
        FireOnTrigger,
    };

public:
    bool KeyValue( KeyValueData* pkvd ) override;

    void Precache() override;
    SpawnAction Spawn() override;

    void BlowerCannonStart( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
    void BlowerCannonStop( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
    void BlowerCannonThink();

    // TODO: probably shadowing CBaseDelay
    float m_flDelay;
    int m_iZOffset;
    WeaponType m_iWeaponType;
    FireType m_iFireType;
};

BEGIN_DATAMAP( CBlowerCannon )
    DEFINE_FIELD( m_flDelay, FIELD_FLOAT ),
    DEFINE_FIELD( m_iZOffset, FIELD_INTEGER ),
    DEFINE_FIELD( m_iWeaponType, FIELD_INTEGER ),
    DEFINE_FIELD( m_iFireType, FIELD_INTEGER ),
    DEFINE_FUNCTION( BlowerCannonStart ),
    DEFINE_FUNCTION( BlowerCannonStop ),
    DEFINE_FUNCTION( BlowerCannonThink ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( env_blowercannon, CBlowerCannon );

bool CBlowerCannon::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "delay" ) )
    {
        m_flDelay = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "weaptype" ) )
    {
        m_iWeaponType = static_cast<WeaponType>( atoi( pkvd->szValue ) );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "firetype" ) )
    {
        m_iFireType = static_cast<FireType>( atoi( pkvd->szValue ) );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "zoffset" ) )
    {
        m_iZOffset = atoi( pkvd->szValue );
        return true;
    }

    // TODO: should call base
    return false;
}

void CBlowerCannon::Precache()
{
    UTIL_PrecacheOther( "displacer_ball" );
    UTIL_PrecacheOther( "spore" );
    UTIL_PrecacheOther( "shock_beam" );
}

SpawnAction CBlowerCannon::Spawn()
{
    SetThink( nullptr );
    SetUse( &CBlowerCannon::BlowerCannonStart );

    pev->nextthink = gpGlobals->time + 0.1;

    if( m_flDelay < 0 )
    {
        m_flDelay = 1;
    }

    Precache();

    return SpawnAction::Spawn;
}

void CBlowerCannon::BlowerCannonStart( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    SetUse( &CBlowerCannon::BlowerCannonStop );
    SetThink( &CBlowerCannon::BlowerCannonThink );

    pev->nextthink = gpGlobals->time + m_flDelay;
}

void CBlowerCannon::BlowerCannonStop( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    SetUse( &CBlowerCannon::BlowerCannonStart );
    SetThink( nullptr );
}

void CBlowerCannon::BlowerCannonThink()
{
    // The target may be spawned later on so keep the cannon going.
    if( auto pTarget = GetNextTarget(); pTarget )
    {
        Vector distance = pTarget->pev->origin - pev->origin;
        distance.z += m_iZOffset;

        Vector angles = UTIL_VecToAngles( distance );
        angles.z = -angles.z;

        switch ( m_iWeaponType )
        {
        case WeaponType::SporeRocket:
        case WeaponType::SporeGrenade:
            CSpore::CreateSpore( pev->origin, angles, this,
                m_iWeaponType == WeaponType::SporeRocket ? CSpore::SporeType::ROCKET : CSpore::SporeType::GRENADE,
                false, false );
            break;

        case WeaponType::ShockBeam:
            CShockBeam::CreateShockBeam( pev->origin, angles, this );
            break;

        case WeaponType::DisplacerBall:
            CDisplacerBall::CreateDisplacerBall( pev->origin, angles, this );
            break;

        default:
            break;
        }
    }

    if( m_iFireType == FireType::FireOnTrigger )
    {
        SetUse( &CBlowerCannon::BlowerCannonStart );
        SetThink( nullptr );
    }

    pev->nextthink = gpGlobals->time + m_flDelay;
}
