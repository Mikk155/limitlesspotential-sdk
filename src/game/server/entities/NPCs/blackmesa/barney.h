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

#pragma once

#include "talkmonster.h"

// first flag is barney dying for scripted sequences?
#define BARNEY_AE_DRAW (2)
#define BARNEY_AE_SHOOT (3)
#define BARNEY_AE_HOLSTER (4)

namespace GuardBodyGroup
{
enum GuardBodyGroup
{
    Weapons = 1
};
}

class CBarney : public CTalkMonster
{
    DECLARE_CLASS( CBarney, CTalkMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    int ISoundMask() override;

    bool HasHumanGibs() override { return true; }

    /**
     *    @brief shoots one round from the pistol at the enemy barney is facing.
     */
    virtual void GuardFirePistol();

    /**
     *    @brief barney says "Freeze!"
     */
    void AlertSound() override;

    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;

    void RunTask( const Task_t* pTask ) override;
    void StartTask( const Task_t* pTask ) override;
    int ObjectCaps() override { return CTalkMonster::ObjectCaps() | FCAP_IMPULSE_USE; }
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    bool CheckRangeAttack1( float flDot, float flDist ) override;

    void DeclineFollowing() override;

    // Override these to set behavior
    const Schedule_t* GetScheduleOfType( int Type ) override;
    const Schedule_t* GetSchedule() override;
    MONSTERSTATE GetIdealState() override;

    void DeathSound() override;
    void PainSound() override;

    void TalkInit() override;

    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;
    void Killed( CBaseEntity* attacker, int iGib ) override;

    bool KeyValue( KeyValueData* pkvd ) override;

    float m_painTime;
    float m_checkAttackTime;
    bool m_lastAttackCheck;


    // Used during spawn to set initial values, not restored
    int m_iGuardBody;

protected:
    virtual void DropWeapon();

    virtual void SpeakKilledEnemy();
};
