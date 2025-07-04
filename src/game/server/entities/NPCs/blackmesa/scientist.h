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

#define NUM_SCIENTIST_HEADS 4 //!< four heads available for scientist model
enum
{
    HEAD_GLASSES = 0,
    HEAD_EINSTEIN = 1,
    HEAD_LUTHER = 2,
    HEAD_SLICK = 3
};

namespace ScientistBodygroup
{
enum ScientistBodygroup
{
    Body = 0,
    Head = 1,
    Needle = 2,
    Item = 3,
};
}

namespace ScientistNeedle
{
enum ScientistNeedle
{
    Blank = 0,
    Drawn
};
}

enum class ScientistItem
{
    None = 0,
    Clipboard,
    Stick
};

enum
{
    SCHED_HIDE = LAST_TALKMONSTER_SCHEDULE + 1,
    SCHED_FEAR,
    SCHED_PANIC,
    SCHED_STARTLE,
    SCHED_TARGET_CHASE_SCARED,
    SCHED_TARGET_FACE_SCARED,
};

enum
{
    TASK_SAY_HEAL = LAST_TALKMONSTER_TASK + 1,
    TASK_HEAL,
    TASK_SAY_FEAR,
    TASK_RUN_PATH_SCARED,
    TASK_SCREAM,
    TASK_RANDOM_SCREAM,
    TASK_MOVE_TO_TARGET_RANGE_SCARED,
};

#define SCIENTIST_AE_HEAL (1)
#define SCIENTIST_AE_NEEDLEON (2)
#define SCIENTIST_AE_NEEDLEOFF (3)

/**
 *    @brief human scientist (passive lab worker)
 */
class CScientist : public CTalkMonster
{
    DECLARE_CLASS( CScientist, CTalkMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    SpawnAction Spawn() override;
    void Precache() override;

    bool HasHumanGibs() override { return true; }

    void SetYawSpeed() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    void RunTask( const Task_t* pTask ) override;
    void StartTask( const Task_t* pTask ) override;
    int ObjectCaps() override { return CTalkMonster::ObjectCaps() | FCAP_IMPULSE_USE; }
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void SetActivity( Activity newActivity ) override;
    Activity GetStoppedActivity() override;
    int ISoundMask() override;
    void DeclineFollowing() override;

    float CoverRadius() override { return 1200; } // Need more room for cover because scientists want to get far away!
    bool DisregardEnemy( CBaseEntity* pEnemy ) { return !pEnemy->IsAlive() || ( gpGlobals->time - m_fearTime ) > 15; }

    bool CanHeal();
    virtual void Heal();
    virtual void Scream();

    // Override these to set behavior
    const Schedule_t* GetScheduleOfType( int Type ) override;
    const Schedule_t* GetSchedule() override;
    MONSTERSTATE GetIdealState() override;

    void DeathSound() override;
    void PainSound() override;

    void TalkInit() override;

protected:
    float m_painTime;
    float m_healTime;
    float m_fearTime;

    // Don't save, only used during spawn.
    ScientistItem m_Item = ScientistItem::None;
};

/**
 *    @brief Sitting Scientist PROP
 */
class CSittingScientist : public CScientist // kdb: changed from public CBaseMonster so he can speak
{
    DECLARE_CLASS( CSittingScientist, CScientist );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;

    void SittingThink();

    void SetAnswerQuestion( CTalkMonster* pSpeaker ) override;

    bool FIdleSpeak();
    int m_baseSequence;
    int m_headTurn;
    float m_flResponseDelay;
};
