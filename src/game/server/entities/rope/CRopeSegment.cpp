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

#include "CRopeSample.h"
#include "CRope.h"

#include "CRopeSegment.h"

BEGIN_DATAMAP( CRopeSegment )
    DEFINE_FIELD( m_pSample, FIELD_CLASSPTR ),
    DEFINE_FIELD( m_iszModelName, FIELD_STRING ),
    DEFINE_FIELD( m_flDefaultMass, FIELD_FLOAT ),
    DEFINE_FIELD( m_bCauseDamage, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_bCanBeGrabbed, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_LastDamageTime, FIELD_TIME ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( rope_segment, CRopeSegment );

CRopeSegment::CRopeSegment()
{
    // TODO: move to in-class initializer?
    m_iszModelName = MAKE_STRING( "models/rope16.mdl" );
}

void CRopeSegment::Precache()
{
    BaseClass::Precache();

    PrecacheModel( STRING( m_iszModelName ) );
    PrecacheSound( "items/grab_rope.wav" );
}

SpawnAction CRopeSegment::Spawn()
{
    Precache();

    SetModel( STRING( m_iszModelName ) );

    pev->movetype = MOVETYPE_NOCLIP;
    pev->solid = SOLID_TRIGGER;
    pev->flags |= FL_ALWAYSTHINK;
    pev->effects = EF_NODRAW;
    SetOrigin( pev->origin );

    SetSize( Vector( -30, -30, -30 ), Vector( 30, 30, 30 ) );

    pev->nextthink = gpGlobals->time + 0.5;

    return SpawnAction::Spawn;
}

void CRopeSegment::Think()
{
    // Do nothing.
}

void CRopeSegment::Touch( CBaseEntity* pOther )
{
    auto player = ToBasePlayer( pOther );

    if( !player )
    {
        return;
    }

    // Electrified wires deal damage.
    if( m_bCauseDamage )
    {
        // Like trigger_hurt we need to deal half a second's worth of damage per touch to make this frametime-independent.
        if( m_LastDamageTime < gpGlobals->time )
        {
            // 1 damage per tick is 30 damage per second at 30 FPS.
            const float damagePerHalfSecond = 30.f / 2;
            pOther->TakeDamage( this, this, damagePerHalfSecond, DMG_SHOCK );
            m_LastDamageTime = gpGlobals->time + 0.5f;
        }
    }

    if( m_pSample->GetMasterRope()->IsAcceptingAttachment() && !player->IsOnRope() )
    {
        if( m_bCanBeGrabbed )
        {
            auto& data = m_pSample->GetData();

            pOther->SetOrigin( data.mPosition );

            player->SetOnRopeState( true );
            player->SetRope( m_pSample->GetMasterRope() );
            m_pSample->GetMasterRope()->AttachObjectToSegment( this );

            const Vector& vecVelocity = pOther->pev->velocity;

            if( vecVelocity.Length() > 0.5 )
            {
                // Apply some external force to move the rope.
                data.mApplyExternalForce = true;

                data.mExternalForce = data.mExternalForce + vecVelocity * 750;
            }

            if( m_pSample->GetMasterRope()->IsSoundAllowed() )
            {
                EmitSound( CHAN_BODY, "items/grab_rope.wav", 1.0, ATTN_NORM );
            }
        }
        else
        {
            // This segment cannot be grabbed, so grab the highest one if possible.
            auto pRope = m_pSample->GetMasterRope();

            CRopeSegment* pSegment;

            if( pRope->GetNumSegments() <= 4 )
            {
                // Fewer than 5 segments exist, so allow grabbing the last one.
                pSegment = pRope->GetSegments()[pRope->GetNumSegments() - 1];
                pSegment->SetCanBeGrabbed( true );
            }
            else
            {
                pSegment = pRope->GetSegments()[4];
            }

            pSegment->Touch( pOther );
        }
    }
}

CRopeSegment* CRopeSegment::CreateSegment( CRopeSample* pSample, string_t iszModelName )
{
    auto pSegment = static_cast<CRopeSegment*>( g_EntityDictionary->Create( "rope_segment" ) );

    pSegment->m_iszModelName = iszModelName;

    pSegment->Spawn();

    pSegment->m_pSample = pSample;

    pSegment->m_bCauseDamage = false;
    pSegment->m_bCanBeGrabbed = true;
    pSegment->m_flDefaultMass = pSample->GetData().mMassReciprocal;

    return pSegment;
}

void CRopeSegment::ApplyExternalForce( const Vector& vecForce )
{
    m_pSample->GetData().mApplyExternalForce = true;

    m_pSample->GetData().mExternalForce = m_pSample->GetData().mExternalForce + vecForce;
}

void CRopeSegment::SetMassToDefault()
{
    m_pSample->GetData().mMassReciprocal = m_flDefaultMass;
}

void CRopeSegment::SetDefaultMass( const float flDefaultMass )
{
    m_flDefaultMass = flDefaultMass;
}

void CRopeSegment::SetMass( const float flMass )
{
    m_pSample->GetData().mMassReciprocal = flMass;
}

void CRopeSegment::SetCauseDamageOnTouch( const bool bCauseDamage )
{
    m_bCauseDamage = bCauseDamage;
}

void CRopeSegment::SetCanBeGrabbed( const bool bCanBeGrabbed )
{
    m_bCanBeGrabbed = bCanBeGrabbed;
}
