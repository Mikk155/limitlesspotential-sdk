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

#include "com_model.h"

#include "CGrappleTip.h"

BEGIN_DATAMAP( CGrappleTip )
    DEFINE_FUNCTION( FlyThink ),
    DEFINE_FUNCTION( OffsetThink ),
    DEFINE_FUNCTION( TongueTouch ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( grapple_tip, CGrappleTip );

namespace
{
// TODO: this should be handled differently. A method that returns an overall size, another whether it's fixed, etc.
const char* const grapple_small[] =
    {
        "monster_bloater",
        "monster_snark",
        "monster_shockroach",
        "monster_rat",
        "monster_alien_babyvoltigore",
        "monster_babycrab",
        "monster_cockroach",
        "monster_flyer_flock",
        "monster_headcrab",
        "monster_leech",
        "monster_penguin"};

const char* const grapple_medium[] =
    {
        "monster_alien_controller",
        "monster_alien_slave",
        "monster_barney",
        "monster_bullchicken",
        "monster_cleansuit_scientist",
        "monster_houndeye",
        "monster_human_assassin",
        "monster_human_grunt",
        "monster_human_grunt_ally",
        "monster_human_medic_ally",
        "monster_human_torch_ally",
        "monster_male_assassin",
        "monster_otis",
        "monster_pitdrone",
        "monster_scientist",
        "monster_zombie",
        "monster_zombie_soldier"};

const char* const grapple_large[] =
    {
        "monster_alien_grunt",
        "monster_alien_voltigore",
        "monster_assassin_repel",
        "monster_grunt_ally_repel",
        "monster_bigmomma",
        "monster_gargantua",
        "monster_geneworm",
        "monster_gonome",
        "monster_grunt_repel",
        "monster_ichthyosaur",
        "monster_nihilanth",
        "monster_pitworm",
        "monster_pitworm_up",
        "monster_shocktrooper"};

const char* const grapple_fixed[] =
    {
        "monster_barnacle",
        "monster_sitting_cleansuit_scientist",
        "monster_sitting_scientist",
        "monster_tentacle",
        "ammo_spore"};
}

void CGrappleTip::Precache()
{
    PrecacheModel( "models/shock_effect.mdl" );
}

SpawnAction CGrappleTip::Spawn()
{
    Precache();

    pev->movetype = MOVETYPE_FLY;
    pev->solid = SOLID_BBOX;

    SetModel( "models/shock_effect.mdl" );

    SetSize( g_vecZero, g_vecZero );

    SetOrigin( pev->origin );

    SetThink( &CGrappleTip::FlyThink );
    SetTouch( &CGrappleTip::TongueTouch );

    pev->angles.x -= 30.0;

    UTIL_MakeVectors( pev->angles );

    pev->angles.x = -( 30.0 + pev->angles.x );

    pev->velocity = g_vecZero;

    pev->gravity = 1;

    pev->nextthink = gpGlobals->time + 0.02;

    m_bIsStuck = false;
    m_bMissed = false;

    return SpawnAction::Spawn;
}

void CGrappleTip::FlyThink()
{
    UTIL_MakeAimVectors( pev->angles );

    pev->angles = UTIL_VecToAngles( gpGlobals->v_forward );

    const float flNewVel = ( ( pev->velocity.Length() * 0.8 ) + 400.0 );

    pev->velocity = pev->velocity * 0.2 + ( flNewVel * gpGlobals->v_forward );

    const float maxSpeed = g_cfg.GetValue<bool>( "grapple_fast"sv, false ) ? 2000 : 1600;

    // TODO: should probably clamp at sv_maxvelocity to prevent the tip from going off course.
    if( pev->velocity.Length() > maxSpeed )
    {
        pev->velocity = pev->velocity.Normalize() * maxSpeed;
    }

    pev->nextthink = gpGlobals->time + 0.02;
}

void CGrappleTip::OffsetThink()
{
    // Nothing
}

void CGrappleTip::TongueTouch( CBaseEntity* pOther )
{
    TargetClass targetClass;

    if( !pOther )
    {
        targetClass = TargetClass::NOT_A_TARGET;
        m_bMissed = true;
    }
    else
    {
        if( pOther->IsPlayer() )
        {
            targetClass = TargetClass::MEDIUM;

            m_hGrappleTarget = pOther;

            m_bIsStuck = true;
        }
        else
        {
            targetClass = ClassifyTarget( pOther );

            if( targetClass != TargetClass::NOT_A_TARGET )
            {
                m_bIsStuck = true;
            }
            else
            {
                m_bMissed = true;
            }
        }
    }

    pev->velocity = g_vecZero;

    m_GrappleType = targetClass;

    SetThink( &CGrappleTip::OffsetThink );
    pev->nextthink = gpGlobals->time + 0.02;

    SetTouch( nullptr );
}

CGrappleTip::TargetClass CGrappleTip::ClassifyTarget( CBaseEntity* pTarget )
{
    if( !pTarget )
        return TargetClass::NOT_A_TARGET;

    if( pTarget->IsPlayer() )
    {
        m_hGrappleTarget = pTarget;

        return TargetClass::MEDIUM;
    }

    const Vector vecEnd = pev->origin + pev->velocity * 1024.0;

    TraceResult tr;

    UTIL_TraceLine( pev->origin, vecEnd, ignore_monsters, edict(), &tr );

    auto pHit = tr.pHit;

    if( !tr.pHit )
        pHit = World->edict();

    const auto pszTexture = TRACE_TEXTURE( pHit, pev->origin, vecEnd );

    bool bIsFixed = false;

    if( pszTexture && strnicmp( pszTexture, "xeno_grapple", 12 ) == 0 )
    {
        bIsFixed = true;
    }
    else
    {
        for( size_t uiIndex = 0; uiIndex < std::size( grapple_small ); ++uiIndex )
        {
            if( pTarget->ClassnameIs( grapple_small[uiIndex] ) )
            {
                m_hGrappleTarget = pTarget;
                m_vecOriginOffset = pev->origin - pTarget->pev->origin;

                return TargetClass::SMALL;
            }
        }

        for( size_t uiIndex = 0; uiIndex < std::size( grapple_medium ); ++uiIndex )
        {
            if( pTarget->ClassnameIs( grapple_medium[uiIndex] ) )
            {
                m_hGrappleTarget = pTarget;
                m_vecOriginOffset = pev->origin - pTarget->pev->origin;

                return TargetClass::MEDIUM;
            }
        }

        for( size_t uiIndex = 0; uiIndex < std::size( grapple_large ); ++uiIndex )
        {
            if( pTarget->ClassnameIs( grapple_large[uiIndex] ) )
            {
                m_hGrappleTarget = pTarget;
                m_vecOriginOffset = pev->origin - pTarget->pev->origin;

                return TargetClass::LARGE;
            }
        }

        for( size_t uiIndex = 0; uiIndex < std::size( grapple_fixed ); ++uiIndex )
        {
            if( pTarget->ClassnameIs( grapple_fixed[uiIndex] ) )
            {
                bIsFixed = true;
                break;
            }
        }
    }

    if( bIsFixed )
    {
        m_hGrappleTarget = pTarget;
        m_vecOriginOffset = g_vecZero;

        return TargetClass::FIXED;
    }

    return TargetClass::NOT_A_TARGET;
}

void CGrappleTip::SetPosition( const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* pOwner )
{
    SetOrigin( vecOrigin );
    pev->angles = vecAngles;
    pev->owner = pOwner->edict();
}
