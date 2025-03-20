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
#include "talkmonster.h"
#include "defaultai.h"
#include "scripted.h"
#include "scientist.h"

/**
 *    @brief human scientist (passive lab worker)
 */
class CCleansuitScientist : public CScientist
{
public:
    void OnCreate() override;

    void Heal() override;
};

LINK_ENTITY_TO_CLASS( monster_cleansuit_scientist, CCleansuitScientist );

void CCleansuitScientist::OnCreate()
{
    CScientist::OnCreate();

    pev->health = g_cfg.GetValue<float>( "cleansuit_scientist_health"sv, 20, this );
    pev->model = MAKE_STRING( "models/cleansuit_scientist.mdl" );
}

void CCleansuitScientist::Heal()
{
    if( !CanHeal() )
        return;

    Vector target = m_hTargetEnt->pev->origin - pev->origin;
    if( target.Length() > 100 )
        return;

    m_hTargetEnt->GiveHealth( g_cfg.GetValue<float>( "cleansuit_scientist_heal"sv, 25, this ), DMG_GENERIC );
    // Don't heal again for 1 minute
    m_healTime = gpGlobals->time + 60;
}

class CDeadCleansuitScientist : public CBaseMonster
{
public:
    void OnCreate() override;
    bool Spawn() override;

    bool HasHumanGibs() override { return true; }

    bool KeyValue( KeyValueData* pkvd ) override;
    int m_iPose; // which sequence to display
    static const char* m_szPoses[9];
};

LINK_ENTITY_TO_CLASS( monster_cleansuit_scientist_dead, CDeadCleansuitScientist );

const char* CDeadCleansuitScientist::m_szPoses[] =
    {
        "lying_on_back",
        "lying_on_stomach",
        "dead_sitting",
        "dead_hang",
        "dead_table1",
        "dead_table2",
        "dead_table3",
        "scientist_deadpose1",
        "dead_against_wall"};

void CDeadCleansuitScientist::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8; // g_cfg.GetValue<float>( "scientist_health"sv, 20, this );
    pev->model = MAKE_STRING( "models/cleansuit_scientist.mdl" );

    SetClassification( "human_passive" );
}

bool CDeadCleansuitScientist::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

bool CDeadCleansuitScientist::Spawn()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    pev->effects = 0;
    pev->sequence = 0;

    m_bloodColor = BLOOD_COLOR_RED;

    if( pev->body == -1 )
    {
        pev->body = 0;
        // -1 chooses a random head
        // pick a head, any head
        SetBodygroup( ScientistBodygroup::Head, RANDOM_LONG( 0, GetBodygroupSubmodelCount( ScientistBodygroup::Head ) - 1 ) );
    }
    // Luther is black, make his hands black
    if( pev->body == HEAD_LUTHER )
        pev->skin = 1;
    else
        pev->skin = 0;

    pev->sequence = LookupSequence( m_szPoses[m_iPose] );
    if( pev->sequence == -1 )
    {
        AILogger->debug( "Dead scientist with bad pose" );
    }

    //    pev->skin += 2; // use bloody skin -- UNDONE: Turn this back on when we have a bloody skin again!
    MonsterInitDead();

    return true;
}

class CSittingCleansuitScientist : public CSittingScientist // kdb: changed from public CBaseMonster so he can speak
{
public:
    void OnCreate()
    {
        CSittingScientist::OnCreate();

        pev->model = MAKE_STRING( "models/cleansuit_scientist.mdl" );
    }
};

LINK_ENTITY_TO_CLASS( monster_sitting_cleansuit_scientist, CSittingCleansuitScientist );
