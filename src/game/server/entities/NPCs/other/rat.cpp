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

/**
 *    @brief environmental monster
 */
class CRat : public CBaseMonster
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;

    bool HasHumanGibs() override { return true; }
};
LINK_ENTITY_TO_CLASS( monster_rat, CRat );

void CRat::OnCreate()
{
    CBaseMonster::OnCreate();

    pev->health = 8;
    pev->model = MAKE_STRING( "models/bigrat.mdl" );

    SetClassification( "insect" );
}

void CRat::SetYawSpeed()
{
    int ys;

    switch ( m_Activity )
    {
    case ACT_IDLE:
    default:
        ys = 45;
        break;
    }

    pev->yaw_speed = ys;
}

SpawnAction CRat::Spawn()
{
    Precache();

    SetModel( STRING( pev->model ) );
    SetSize( Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_RED;
    pev->view_ofs = Vector( 0, 0, 6 ); // position of the eyes relative to monster's origin.
    m_flFieldOfView = 0.5;             // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CRat::Precache()
{
    PrecacheModel( STRING( pev->model ) );
}
