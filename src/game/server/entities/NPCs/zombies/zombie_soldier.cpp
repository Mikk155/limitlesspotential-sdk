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
#include "zombie.h"

class CZombieSoldier : public CZombie
{
public:
    void OnCreate() override
    {
        CZombie::OnCreate();

        pev->health = g_cfg.GetValue<float>( "zombie_soldier_health"sv, 120, this );
        pev->model = MAKE_STRING( "models/zombie_soldier.mdl" );
    }

protected:
    float GetOneSlashDamage() override { return g_cfg.GetValue<float>( "zombie_soldier_dmg_one_slash"sv, 20, this ); }
    float GetBothSlashDamage() override { return g_cfg.GetValue<float>( "zombie_soldier_dmg_both_slash"sv, 40, this ); }
};

LINK_ENTITY_TO_CLASS( monster_zombie_soldier, CZombieSoldier );

class CDeadZombieSoldier : public CBaseMonster
{
public:
    void OnCreate() override;
    bool Spawn() override;

    bool HasAlienGibs() override { return true; }

    bool KeyValue( KeyValueData* pkvd ) override;

    int m_iPose; // which sequence to display    -- temporary, don't need to save
    static const char* m_szPoses[2];
};

const char* CDeadZombieSoldier::m_szPoses[] = {"dead_on_stomach", "dead_on_back"};

void CDeadZombieSoldier::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8;
    pev->model = MAKE_STRING( "models/zombie_soldier.mdl" );

    SetClassification( "alien_monster" );
}

bool CDeadZombieSoldier::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_zombie_soldier_dead, CDeadZombieSoldier );

bool CDeadZombieSoldier::Spawn()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    pev->effects = 0;
    pev->yaw_speed = 8;
    pev->sequence = 0;
    m_bloodColor = BLOOD_COLOR_RED;

    pev->sequence = LookupSequence( m_szPoses[m_iPose] );

    if( pev->sequence == -1 )
    {
        AILogger->debug( "Dead hgrunt with bad pose" );
    }

    MonsterInitDead();

    return true;
}
