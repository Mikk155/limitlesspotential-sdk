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
 *    The Halflife Cycler Monsters
 */

#include "cbase.h"

class CCycler : public CBaseMonster
{
    DECLARE_CLASS( CCycler, CBaseMonster );
    DECLARE_DATAMAP();

public:
    int ObjectCaps() override { return ( CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE ); }

    /**
     *    @brief changes sequences when shot
     */
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Think() override;
    // void Pain( float flDamage );

    /**
     *    @brief starts a rotation trend
     */
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    // Don't treat as a live target
    bool IsAlive() override { return false; }
    bool IsMonster() override { return false; }

    bool m_animate;
};

LINK_ENTITY_TO_CLASS( cycler, CCycler );

BEGIN_DATAMAP( CCycler )
    DEFINE_FIELD( m_animate, FIELD_BOOLEAN ),
END_DATAMAP();

void CCycler::OnCreate()
{
    CBaseMonster::OnCreate();

    pev->health = 80000; // no cycler should die
}

SpawnAction CCycler::Spawn()
{
    const char* szModel = STRING( pev->model );

    if( !szModel || '\0' == *szModel )
    {
        CBaseEntity::Logger->error( "cycler at {} missing modelname", pev->origin.MakeString() );
        return SpawnAction::DelayRemove;
    }

    const Vector vecMin( -16, -16, 0 );
    const Vector vecMax( 16, 16, 72 );

    PrecacheModel( szModel );
    SetModel( szModel );

    InitBoneControllers();
    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_NONE;
    pev->takedamage = DAMAGE_YES;
    pev->effects = 0;
    pev->yaw_speed = 5;
    pev->ideal_yaw = pev->angles.y;
    ChangeYaw( 360 );

    m_flFrameRate = 75;
    m_flGroundSpeed = 0;

    pev->nextthink += 1.0;

    ResetSequenceInfo();

    if( pev->sequence != 0 || pev->frame != 0 )
    {
        m_animate = false;
        pev->framerate = 0;
    }
    else
    {
        m_animate = true;
    }

    SetSize( vecMin, vecMax );

    return SpawnAction::Spawn;
}

void CCycler::Think()
{
    pev->nextthink = gpGlobals->time + 0.1;

    if( m_animate )
    {
        StudioFrameAdvance();
    }

    UpdateShockEffect();

    if( m_fSequenceFinished && !m_fSequenceLoops )
    {
        // ResetSequenceInfo();
        // hack to avoid reloading model every frame
        pev->animtime = gpGlobals->time;
        pev->framerate = 1.0;
        m_fSequenceFinished = false;
        m_flLastEventCheck = gpGlobals->time;
        pev->frame = 0;
        if( !m_animate )
            pev->framerate = 0.0; // FIX: don't reset framerate
    }
}

void CCycler::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    m_animate = !m_animate;
    if( m_animate )
        pev->framerate = 1.0;
    else
        pev->framerate = 0.0;
}

bool CCycler::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    if( m_animate )
    {
        pev->sequence++;

        ResetSequenceInfo();

        if( m_flFrameRate == 0.0 )
        {
            pev->sequence = 0;
            ResetSequenceInfo();
        }
        pev->frame = 0;
    }
    else
    {
        pev->framerate = 1.0;
        StudioFrameAdvance( 0.1 );
        pev->framerate = 0;
        CBaseEntity::Logger->debug( "sequence: {}, frame {:.0f}", pev->sequence, pev->frame );
    }

    return false;
}

class CCyclerSprite : public CBaseEntity
{
    DECLARE_CLASS( CCyclerSprite, CBaseEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Think() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    int ObjectCaps() override { return ( CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE ); }
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void Animate( float frames );

    inline bool ShouldAnimate() { return m_animate && m_maxFrame > 1.0; }
    bool m_animate;
    float m_lastTime;
    float m_maxFrame;
};

LINK_ENTITY_TO_CLASS( cycler_sprite, CCyclerSprite );

BEGIN_DATAMAP( CCyclerSprite )
    DEFINE_FIELD( m_animate, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_lastTime, FIELD_TIME ),
    DEFINE_FIELD( m_maxFrame, FIELD_FLOAT ),
END_DATAMAP();

SpawnAction CCyclerSprite::Spawn()
{
    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_NONE;
    pev->takedamage = DAMAGE_YES;
    pev->effects = 0;

    pev->frame = 0;
    pev->nextthink = gpGlobals->time + 0.1;
    m_animate = true;
    m_lastTime = gpGlobals->time;

    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;

    return SpawnAction::Spawn;
}

void CCyclerSprite::Think()
{
    if( ShouldAnimate() )
        Animate( pev->framerate * ( gpGlobals->time - m_lastTime ) );

    pev->nextthink = gpGlobals->time + 0.1;
    m_lastTime = gpGlobals->time;
}

void CCyclerSprite::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    m_animate = !m_animate;
    CBaseEntity::Logger->debug( "Sprite: {}", STRING( pev->model ) );
}

bool CCyclerSprite::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    if( m_maxFrame > 1.0 )
    {
        Animate( 1.0 );
    }
    return true;
}

void CCyclerSprite::Animate( float frames )
{
    pev->frame += frames;
    if( m_maxFrame > 0 )
        pev->frame = fmod( pev->frame, m_maxFrame );
}

/**
 *    @brief Flaming Wreakage
 */
class CWreckage : public CBaseMonster
{
    DECLARE_CLASS( CWreckage, CBaseMonster );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;
    void Think() override;
    bool IsMonster() override { return false; }

private:
    float m_flStartTime;
};

BEGIN_DATAMAP( CWreckage )
    DEFINE_FIELD( m_flStartTime, FIELD_TIME ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( cycler_wreckage, CWreckage );

SpawnAction CWreckage::Spawn()
{
    pev->solid = SOLID_NOT;
    pev->movetype = MOVETYPE_NONE;
    pev->takedamage = 0;
    pev->effects = 0;

    pev->frame = 0;
    pev->nextthink = gpGlobals->time + 0.1;

    if( !FStringNull( pev->model ) )
    {
        PrecacheModel( STRING( pev->model ) );
        SetModel( STRING( pev->model ) );
    }
    // pev->scale = 5.0;

    m_flStartTime = gpGlobals->time;

    return SpawnAction::Spawn;
}

void CWreckage::Precache()
{
    if( !FStringNull( pev->model ) )
        PrecacheModel( STRING( pev->model ) );
}

void CWreckage::Think()
{
    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.2;

    UpdateShockEffect();

    if( 0 != pev->dmgtime )
    {
        if( pev->dmgtime < gpGlobals->time )
        {
            UTIL_Remove( this );
            return;
        }
        else if( RANDOM_FLOAT( 0, pev->dmgtime - m_flStartTime ) > pev->dmgtime - gpGlobals->time )
        {
            return;
        }
    }

    Vector VecSrc;

    VecSrc.x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
    VecSrc.y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
    VecSrc.z = RANDOM_FLOAT( pev->absmin.z, pev->absmax.z );

    MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, VecSrc );
    WRITE_BYTE( TE_SMOKE );
    WRITE_COORD( VecSrc.x );
    WRITE_COORD( VecSrc.y );
    WRITE_COORD( VecSrc.z );
    WRITE_SHORT( g_sModelIndexSmoke );
    WRITE_BYTE( RANDOM_LONG( 0, 49 ) + 50 ); // scale * 10
    WRITE_BYTE( RANDOM_LONG( 0, 3 ) + 8 );     // framerate
    MESSAGE_END();
}
