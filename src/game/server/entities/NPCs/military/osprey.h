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

#define SF_WAITFORTRIGGER 0x40

#define MAX_CARRY 24

class COsprey : public CBaseMonster
{
    DECLARE_CLASS( COsprey, CBaseMonster );
    DECLARE_DATAMAP();

public:
    int ObjectCaps() override { return CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

    bool IsMachine() const override { return true; }

    void OnCreate() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    SpawnAction Spawn() override;
    void Precache() override;
    int BloodColor() override { return DONT_BLEED; }
    void Killed( CBaseEntity* attacker, int iGib ) override;

    void UpdateGoal();
    bool HasDead();
    void FlyThink();
    void DeployThink();
    void Flight();
    void HitTouch( CBaseEntity* pOther );
    void FindAllThink();
    void HoverThink();

    CBaseMonster* MakeGrunt( Vector vecSrc );

    void CrashTouch( CBaseEntity* pOther );
    void DyingThink();
    void CommandUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );

    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;
    void ShowDamage();
    void Update();

    CBaseEntity* m_pGoalEnt;
    Vector m_vel1;
    Vector m_vel2;
    Vector m_pos1;
    Vector m_pos2;
    Vector m_ang1;
    Vector m_ang2;
    float m_startTime;
    float m_dTime;

    Vector m_velocity;

    float m_flIdealtilt;
    float m_flRotortilt;

    float m_flRightHealth;
    float m_flLeftHealth;

    int m_iUnits = 0;
    EHANDLE m_hGrunt[MAX_CARRY];
    Vector m_vecOrigin[MAX_CARRY];
    EHANDLE m_hRepel[4];

    int m_iSoundState;
    int m_iSpriteTexture;

    int m_iPitch;

    int m_iExplode;
    int m_iTailGibs;
    int m_iBodyGibs;
    int m_iEngineGibs;

    int m_iDoLeftSmokePuff;
    int m_iDoRightSmokePuff;

protected:
    /**
     *    @brief Class name of the monster to spawn
     *    @details Must return a string literal
     */
    virtual const char* GetMonsterClassname() const { return "monster_human_grunt"; }

    /**
     *    @brief Precaches all Osprey assets
     *    @details Inputs must be string literals
     */
    void PrecacheCore( const char* tailModel, const char* bodyModel, const char* engineModel );
};
