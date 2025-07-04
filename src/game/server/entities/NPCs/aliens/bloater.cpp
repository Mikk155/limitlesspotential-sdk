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

#define BLOATER_AE_ATTACK_MELEE1 0x01

class CBloater : public CBaseMonster
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;

    bool HasAlienGibs() override { return true; }

    void PainSound() override;
    void AlertSound() override;
    void IdleSound() override;
    void AttackSnd();

    // No range attacks
    bool CheckRangeAttack1( float flDot, float flDist ) override { return false; }
    bool CheckRangeAttack2( float flDot, float flDist ) override { return false; }
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
};

LINK_ENTITY_TO_CLASS( monster_bloater, CBloater );

void CBloater::OnCreate()
{
    CBaseMonster::OnCreate();

    pev->health = 40;
    pev->model = MAKE_STRING( "models/floater.mdl" );

    SetClassification( "alien_monster" );
}

void CBloater::SetYawSpeed()
{
    int ys;

    ys = 120;

#if 0
    switch ( m_Activity )
    {
    }
#endif

    pev->yaw_speed = ys;
}

bool CBloater::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    PainSound();
    return CBaseMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CBloater::PainSound()
{
#if 0    
    int pitch = 95 + RANDOM_LONG( 0, 9 );

    switch ( RANDOM_LONG( 0, 5 ) )
    {
    case 0:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_pain1.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 1:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_pain2.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    default:
        break;
    }
#endif
}

void CBloater::AlertSound()
{
#if 0
    int pitch = 95 + RANDOM_LONG( 0, 9 );

    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_alert10.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 1:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_alert20.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 2:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_alert30.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    }
#endif
}

void CBloater::IdleSound()
{
#if 0
    int pitch = 95 + RANDOM_LONG( 0, 9 );

    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_idle1.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 1:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_idle2.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 2:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_idle3.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    }
#endif
}

void CBloater::AttackSnd()
{
#if 0
    int pitch = 95 + RANDOM_LONG( 0, 9 );

    switch ( RANDOM_LONG( 0, 1 ) )
    {
    case 0:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_attack1.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    case 1:
        EmitSoundDyn( CHAN_VOICE, "zombie/zo_attack2.wav", 1.0, ATTN_NORM, 0, pitch );
        break;
    }
#endif
}

void CBloater::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case BLOATER_AE_ATTACK_MELEE1:
    {
        // do stuff for this event.
        AttackSnd();
    }
    break;

    default:
        CBaseMonster::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CBloater::Spawn()
{
    Precache();

    SetModel( STRING( pev->model ) );
    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_FLY;
    pev->spawnflags |= FL_FLY;
    m_bloodColor = BLOOD_COLOR_GREEN;
    pev->view_ofs = VEC_VIEW; // position of the eyes relative to monster's origin.
    m_flFieldOfView = 0.5;      // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CBloater::Precache()
{
    PrecacheModel( STRING( pev->model ) );
}
