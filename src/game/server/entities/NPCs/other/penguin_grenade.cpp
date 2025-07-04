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

#define PENGUIN_DETONATE_DELAY 15.0

enum MonsterPenguinAnim
{
    MONSTERPENGUIN_IDLE1 = 0,
    MONSTERPENGUIN_FIDGET,
    MONSTERPENGUIN_JUMP,
    MONSTERPENGUIN_RUN,
};

class CPenguinGrenade : public CGrenade
{
    DECLARE_CLASS( CPenguinGrenade, CGrenade );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    void Precache() override;
    void GibMonster() override;
    void SuperBounceTouch( CBaseEntity* pOther );
    SpawnAction Spawn() override;

    bool HasAlienGibs() override { return true; }

    bool IsBioWeapon() const override { return true; }

    Relationship IRelationship( CBaseEntity* pTarget ) override;
    void Killed( CBaseEntity* attacker, int iGib ) override;
    void HuntThink();
    void Smoke();
    int BloodColor() override;

    static float m_flNextBounceSoundTime;

    float m_flDie;
    Vector m_vecTarget;
    float m_flNextHunt;
    float m_flNextHit;
    Vector m_posPrev;
    EHANDLE m_hOwner;
};

float CPenguinGrenade::m_flNextBounceSoundTime = 0;

BEGIN_DATAMAP( CPenguinGrenade )
    DEFINE_FIELD( m_flDie, FIELD_TIME ),
    DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
    DEFINE_FIELD( m_flNextHunt, FIELD_TIME ),
    DEFINE_FIELD( m_flNextHit, FIELD_TIME ),
    DEFINE_FIELD( m_posPrev, FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
    DEFINE_FUNCTION( SuperBounceTouch ),
    DEFINE_FUNCTION( HuntThink ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( monster_penguin, CPenguinGrenade );

void CPenguinGrenade::OnCreate()
{
    CGrenade::OnCreate();

    pev->model = MAKE_STRING( "models/w_penguin.mdl" );

    SetClassification( "alien_bioweapon" );
}

void CPenguinGrenade::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "squeek/sqk_blast1.wav" );
    PrecacheSound( "common/bodysplat.wav" );
    PrecacheSound( "squeek/sqk_die1.wav" );
    PrecacheSound( "squeek/sqk_hunt1.wav" );
    PrecacheSound( "squeek/sqk_hunt2.wav" );
    PrecacheSound( "squeek/sqk_hunt3.wav" );
    PrecacheSound( "squeek/sqk_deploy1.wav" );
}

void CPenguinGrenade::GibMonster()
{
    EmitSoundDyn( CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200 );
}

void CPenguinGrenade::SuperBounceTouch( CBaseEntity* pOther )
{
    TraceResult tr = UTIL_GetGlobalTrace();

    // don't hit the guy that launched this grenade
    if( pev->owner && pOther->edict() == pev->owner )
        return;

    // at least until we've bounced once
    pev->owner = nullptr;

    pev->angles.x = 0;
    pev->angles.z = 0;

    // avoid bouncing too much
    if( m_flNextHit > gpGlobals->time )
        return;

    // higher pitch as squeeker gets closer to detonation time
    const float flpitch = 155.0 - 60.0 * ( ( m_flDie - gpGlobals->time ) / PENGUIN_DETONATE_DELAY );

    if( 0 != pOther->pev->takedamage && m_flNextAttack < gpGlobals->time )
    {
        // attack!

        bool hurtTarget = true;

        if( g_GameMode->IsMultiplayer() )
        {
            // TODO: set to null earlier on, so this can never be valid
            auto owner = ToBasePlayer( pev->owner );

            hurtTarget = true;

            if( auto otherPlayer = ToBasePlayer( pOther ); otherPlayer )
            {
                if( owner )
                {
                    hurtTarget = false;
                    if( owner != otherPlayer )
                    {
                        hurtTarget = g_pGameRules->FPlayerCanTakeDamage( otherPlayer, owner );
                    }
                }
            }
        }

        // make sure it's me who has touched them
        if( tr.pHit == pOther->edict() )
        {
            // and it's not another squeakgrenade
            if( tr.pHit->v.modelindex != pev->modelindex )
            {
                // AILogger->debug("hit enemy");
                ClearMultiDamage();
                pOther->TraceAttack( this, g_cfg.GetValue<float>( "snark_dmg_bite"sv, 10, this ), gpGlobals->v_forward, &tr, DMG_SLASH );
                if( m_hOwner != nullptr )
                    ApplyMultiDamage( this, m_hOwner );
                else
                    ApplyMultiDamage( this, this );

                // add more explosion damage
                if( hurtTarget )
                    pev->dmg += g_cfg.GetValue<float>( "plr_hand_grenade"sv, 100, this );
                else
                    pev->dmg += g_cfg.GetValue<float>( "plr_hand_grenade"sv, 100, this ) / 5.0;

                if( pev->dmg > 500.0 )
                {
                    pev->dmg = 500.0;
                }

                // m_flDie += 2.0; // add more life

                // make bite sound
                EmitSoundDyn( CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch );
                m_flNextAttack = gpGlobals->time + 0.5;
            }
        }
        else
        {
            // AILogger->debug("been hit");
        }
    }

    m_flNextHit = gpGlobals->time + 0.1;
    m_flNextHunt = gpGlobals->time;

    if( g_GameMode->IsMultiplayer() )
    {
        // in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
        if( gpGlobals->time < m_flNextBounceSoundTime )
        {
            // too soon!
            return;
        }
    }

    if( ( pev->flags & FL_ONGROUND ) == 0 )
    {
        // play bounce sound
        float flRndSound = RANDOM_FLOAT( 0, 1 );

        if( flRndSound <= 0.33 )
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch );
        else if( flRndSound <= 0.66 )
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch );
        else
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch );
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
    }
    else
    {
        // skittering sound
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 100, 0.1 );
    }

    m_flNextBounceSoundTime = gpGlobals->time + 0.5; // half second.
}

SpawnAction CPenguinGrenade::Spawn()
{
    Precache();
    // motor
    pev->movetype = MOVETYPE_BOUNCE;
    pev->solid = SOLID_BBOX;

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "snark_health"sv, 2, this );
    SetModel( STRING( pev->model ) );
    SetSize( Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );
    SetOrigin( pev->origin );

    SetTouch( &CPenguinGrenade::SuperBounceTouch );
    SetThink( &CPenguinGrenade::HuntThink );
    pev->nextthink = gpGlobals->time + 0.1;
    m_flNextHunt = gpGlobals->time + 1E6;

    pev->flags |= FL_MONSTER;
    pev->takedamage = DAMAGE_AIM;
    pev->gravity = 0.5;
    pev->friction = 0.5;

    pev->dmg = g_cfg.GetValue<float>( "plr_hand_grenade"sv, 100, this );

    m_flDie = gpGlobals->time + PENGUIN_DETONATE_DELAY;

    m_flFieldOfView = 0; // 180 degrees

    if( pev->owner )
        m_hOwner = Instance( pev->owner );

    m_flNextBounceSoundTime = gpGlobals->time; // reset each time a snark is spawned.

    // TODO: shouldn't use index
    pev->sequence = MONSTERPENGUIN_RUN;
    ResetSequenceInfo();

    return SpawnAction::Spawn;
}

Relationship CPenguinGrenade::IRelationship( CBaseEntity* pTarget )
{
    const auto classification = pTarget->Classify();

    if( g_EntityClassifications.ClassNameIs( classification, {"alien_military"} ) )
    {
        return Relationship::Dislike;
    }
    else if( g_EntityClassifications.ClassNameIs( classification, {"player_ally"} ) )
    {
        return Relationship::Ally;
    }
    else
    {
        return CBaseMonster::IRelationship( pTarget );
    }
}

void CPenguinGrenade::Killed( CBaseEntity* attacker, int iGib )
{
    if( m_hOwner != nullptr )
        pev->owner = m_hOwner->edict();

    Detonate();

    UTIL_BloodDrips( pev->origin, g_vecZero, BloodColor(), 80 );

    // reset owner so death message happens
    if( m_hOwner != nullptr )
        pev->owner = m_hOwner->edict();
}

void CPenguinGrenade::HuntThink()
{
    // AILogger->debug("think");

    if( !IsInWorld() )
    {
        SetTouch( nullptr );
        UTIL_Remove( this );
        return;
    }

    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.1;

    // explode when ready
    if( gpGlobals->time >= m_flDie )
    {
        g_vecAttackDir = pev->velocity.Normalize();
        pev->health = -1;
        Killed( this, 0 );
        return;
    }

    // float
    if( pev->waterlevel != WaterLevel::Dry )
    {
        if( pev->movetype == MOVETYPE_BOUNCE )
        {
            pev->movetype = MOVETYPE_FLY;
        }
        pev->velocity = pev->velocity * 0.9;
        pev->velocity.z += 8.0;
    }
    else if( pev->movetype == MOVETYPE_FLY )
    {
        pev->movetype = MOVETYPE_BOUNCE;
    }

    // return if not time to hunt
    if( m_flNextHunt > gpGlobals->time )
        return;

    m_flNextHunt = gpGlobals->time + 2.0;

    Vector vecDir;
    Vector vecFlat = pev->velocity;
    vecFlat.z = 0;
    vecFlat = vecFlat.Normalize();

    UTIL_MakeVectors( pev->angles );

    if( m_hEnemy == nullptr || !m_hEnemy->IsAlive() )
    {
        // find target, bounce a bit towards it.
        Look( 512 );
        m_hEnemy = BestVisibleEnemy();
    }

    // squeek if it's about time blow up
    if( ( m_flDie - gpGlobals->time <= 0.5 ) && ( m_flDie - gpGlobals->time >= 0.3 ) )
    {
        EmitSoundDyn( CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( 0, 0x3F ) );
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
    }

    // higher pitch as squeeker gets closer to detonation time
    float flpitch = 155.0 - 60.0 * ( ( m_flDie - gpGlobals->time ) / PENGUIN_DETONATE_DELAY );
    if( flpitch < 80 )
        flpitch = 80;

    if( m_hEnemy != nullptr )
    {
        if( FVisible( m_hEnemy ) )
        {
            vecDir = m_hEnemy->EyePosition() - pev->origin;
            m_vecTarget = vecDir.Normalize();
        }

        float flVel = pev->velocity.Length();
        float flAdj = 50.0 / ( flVel + 10.0 );

        if( flAdj > 1.2 )
            flAdj = 1.2;

        // AILogger->debug("think : enemy");

        // AILogger->debug("{:.0f} {:.2f}", flVel, m_vecTarget);

        pev->velocity = pev->velocity * flAdj + m_vecTarget * 300;
    }

    if( ( pev->flags & FL_ONGROUND ) != 0 )
    {
        pev->avelocity = Vector( 0, 0, 0 );
    }
    else
    {
        if( pev->avelocity == Vector( 0, 0, 0 ) )
        {
            pev->avelocity.x = RANDOM_FLOAT( -100, 100 );
            pev->avelocity.z = RANDOM_FLOAT( -100, 100 );
        }
    }

    if( ( pev->origin - m_posPrev ).Length() < 1.0 )
    {
        pev->velocity.x = RANDOM_FLOAT( -100, 100 );
        pev->velocity.y = RANDOM_FLOAT( -100, 100 );
    }
    m_posPrev = pev->origin;

    pev->angles = UTIL_VecToAngles( pev->velocity );
    pev->angles.z = 0;
    pev->angles.x = 0;

    // Update classification
    if( !HasCustomClassification() )
    {
        // TODO: maybe use a different classification that has the expected relationships for these classes.
        const char* classification = "alien_bioweapon";

        if( m_hEnemy != nullptr )
        {
            if( g_EntityClassifications.ClassNameIs( m_hEnemy->Classify(), {"player", "human_passive", "human_military"} ) )
            {
                // barney's get mad, grunts get mad at it
                classification = "alien_military";
            }
        }

        SetClassification( classification );
    }
}

void CPenguinGrenade::Smoke()
{
    if( UTIL_PointContents( pev->origin ) == CONTENTS_WATER )
    {
        UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
        UTIL_Remove( this );
    }
    else
    {
        MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
        g_engfuncs.pfnWriteByte( TE_SMOKE );
        g_engfuncs.pfnWriteCoord( pev->origin.x );
        g_engfuncs.pfnWriteCoord( pev->origin.y );
        g_engfuncs.pfnWriteCoord( pev->origin.z );
        g_engfuncs.pfnWriteShort( g_sModelIndexSmoke );
        g_engfuncs.pfnWriteByte( ( pev->dmg - 50.0 ) * 0.8 );
        g_engfuncs.pfnWriteByte( 12 );
        g_engfuncs.pfnMessageEnd();

        UTIL_Remove( this );
    }
}

int CPenguinGrenade::BloodColor()
{
    return BLOOD_COLOR_RED;
}
