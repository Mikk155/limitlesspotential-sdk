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
#include "squadmonster.h"

#define ISLAVE_AE_CLAW (1)
#define ISLAVE_AE_CLAWRAKE (2)
#define ISLAVE_AE_ZAP_POWERUP (3)
#define ISLAVE_AE_ZAP_SHOOT (4)
#define ISLAVE_AE_ZAP_DONE (5)

#define ISLAVE_MAX_BEAMS 8

/**
 *    @brief Alien slave monster
 */
class CISlave : public CSquadMonster
{
    DECLARE_CLASS( CISlave, CSquadMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    int ISoundMask() override;
    Relationship IRelationship( CBaseEntity* pTarget ) override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    bool CheckRangeAttack1( float flDot, float flDist ) override; //!< normal beam attack
    bool CheckRangeAttack2( float flDot, float flDist ) override; //!< check bravery and try to resurect dead comrades
    void CallForHelp( const char* szClassname, float flDist, EHANDLE hEnemy, Vector& vecLocation );
    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;

    bool HasAlienGibs() override { return true; }

    void DeathSound() override;
    void PainSound() override;
    void AlertSound() override;
    void IdleSound() override;

    void Killed( CBaseEntity* attacker, int iGib ) override;

    void StartTask( const Task_t* pTask ) override;
    const Schedule_t* GetSchedule() override;
    const Schedule_t* GetScheduleOfType( int Type ) override;

    void UpdateOnRemove() override;

    /**
     *    @brief remove all beams
     */
    void ClearBeams();

    /**
     *    @brief small beam from arm to nearby geometry
     */
    void ArmBeam( int side );

    /**
     *    @brief regenerate dead colleagues
     */
    void WackBeam( int side, CBaseEntity* pEntity );

    /**
     *    @brief heavy damage directly forward
     */
    void ZapBeam( int side );

    /**
     *    @brief brighten all beams
     */
    void BeamGlow();

    int m_iBravery;

    CBeam* m_pBeam[ISLAVE_MAX_BEAMS];

    int m_iBeams;
    float m_flNextAttack;

    int m_voicePitch;

    EHANDLE m_hDead;

    static const char* pAttackHitSounds[];
    static const char* pAttackMissSounds[];
    static const char* pPainSounds[];
    static const char* pDeathSounds[];
};

LINK_ENTITY_TO_CLASS( monster_alien_slave, CISlave );

BEGIN_DATAMAP( CISlave )
    DEFINE_FIELD( m_iBravery, FIELD_INTEGER ),

    DEFINE_ARRAY( m_pBeam, FIELD_CLASSPTR, ISLAVE_MAX_BEAMS ),
    DEFINE_FIELD( m_iBeams, FIELD_INTEGER ),
    DEFINE_FIELD( m_flNextAttack, FIELD_TIME ),

    DEFINE_FIELD( m_voicePitch, FIELD_INTEGER ),

    DEFINE_FIELD( m_hDead, FIELD_EHANDLE ),
END_DATAMAP();

const char* CISlave::pAttackHitSounds[] =
    {
        "zombie/claw_strike1.wav",
        "zombie/claw_strike2.wav",
        "zombie/claw_strike3.wav",
};

const char* CISlave::pAttackMissSounds[] =
    {
        "zombie/claw_miss1.wav",
        "zombie/claw_miss2.wav",
};

const char* CISlave::pPainSounds[] =
    {
        "aslave/slv_pain1.wav",
        "aslave/slv_pain2.wav",
};

const char* CISlave::pDeathSounds[] =
    {
        "aslave/slv_die1.wav",
        "aslave/slv_die2.wav",
};

void CISlave::OnCreate()
{
    CSquadMonster::OnCreate();

    pev->model = MAKE_STRING( "models/islave.mdl" );

    SetClassification( "alien_military" );
}

Relationship CISlave::IRelationship( CBaseEntity* pTarget )
{
    if( ( pTarget->IsPlayer() ) )
        if( ( pev->spawnflags & SF_MONSTER_WAIT_UNTIL_PROVOKED ) != 0 && ( m_afMemory & bits_MEMORY_PROVOKED ) == 0 )
            return Relationship::None;
    return CBaseMonster::IRelationship( pTarget );
}

void CISlave::CallForHelp( const char* szClassname, float flDist, EHANDLE hEnemy, Vector& vecLocation )
{
    // AILogger->debug("help");

    // skip ones not on my netname
    if( FStringNull( pev->netname ) )
        return;

    CBaseEntity* pEntity = nullptr;

    while( ( pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ) ) ) != nullptr )
    {
        float d = ( pev->origin - pEntity->pev->origin ).Length();
        if( d < flDist )
        {
            CBaseMonster* pMonster = pEntity->MyMonsterPointer();
            if( pMonster )
            {
                pMonster->m_afMemory |= bits_MEMORY_PROVOKED;
                pMonster->PushEnemy( hEnemy, vecLocation );
            }
        }
    }
}

void CISlave::AlertSound()
{
    if( m_hEnemy != nullptr )
    {
        sentences::g_Sentences.PlayRndSz( this, "SLV_ALERT", 0.85, ATTN_NORM, 0, m_voicePitch );

        CallForHelp( "monster_alien_slave", 512, m_hEnemy, m_vecEnemyLKP );
    }
}

void CISlave::IdleSound()
{
    if( RANDOM_LONG( 0, 2 ) == 0 )
    {
        sentences::g_Sentences.PlayRndSz( this, "SLV_IDLE", 0.85, ATTN_NORM, 0, m_voicePitch );
    }

#if 0
    int side = RANDOM_LONG( 0, 1 ) * 2 - 1;

    ClearBeams();
    ArmBeam( side );

    UTIL_MakeAimVectors( pev->angles );
    Vector vecSrc = pev->origin + gpGlobals->v_right * 2 * side;
    MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
    WRITE_BYTE( TE_DLIGHT );
    WRITE_COORD( vecSrc.x );    // X
    WRITE_COORD( vecSrc.y );    // Y
    WRITE_COORD( vecSrc.z );    // Z
    WRITE_BYTE( 8 );        // radius * 0.1
    WRITE_BYTE( 255 );        // r
    WRITE_BYTE( 180 );        // g
    WRITE_BYTE( 96 );        // b
    WRITE_BYTE( 10 );        // time * 10
    WRITE_BYTE( 0 );        // decay * 0.1
    MESSAGE_END();

    EmitSound( CHAN_WEAPON, "debris/zap1.wav", 1, ATTN_NORM );
#endif
}

void CISlave::PainSound()
{
    if( RANDOM_LONG( 0, 2 ) == 0 )
    {
        EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
    }
}

void CISlave::DeathSound()
{
    EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pDeathSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
}

int CISlave::ISoundMask()
{
    return bits_SOUND_WORLD |
           bits_SOUND_COMBAT |
           bits_SOUND_DANGER |
           bits_SOUND_PLAYER;
}

void CISlave::Killed( CBaseEntity* attacker, int iGib )
{
    ClearBeams();
    CSquadMonster::Killed( attacker, iGib );
}

void CISlave::SetYawSpeed()
{
    int ys;

    switch ( m_Activity )
    {
    case ACT_WALK:
        ys = 50;
        break;
    case ACT_RUN:
        ys = 70;
        break;
    case ACT_IDLE:
        ys = 50;
        break;
    default:
        ys = 90;
        break;
    }

    pev->yaw_speed = ys;
}

void CISlave::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    // AILogger->debug("event {} : {}", pEvent->event, pev->frame);
    switch ( pEvent->event )
    {
    case ISLAVE_AE_CLAW:
    {
        // SOUND HERE!
        CBaseEntity* pHurt = CheckTraceHullAttack( 70, g_cfg.GetValue<float>( "islave_dmg_claw"sv, 10, this ), DMG_SLASH );
        if( pHurt )
        {
            if( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
            {
                pHurt->pev->punchangle.z = -18;
                pHurt->pev->punchangle.x = 5;
            }
            // Play a random attack hit sound
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
        }
        else
        {
            // Play a random attack miss sound
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
        }
    }
    break;

    case ISLAVE_AE_CLAWRAKE:
    {
        CBaseEntity* pHurt = CheckTraceHullAttack( 70, g_cfg.GetValue<float>( "islave_dmg_clawrake"sv, 25, this ), DMG_SLASH );
        if( pHurt )
        {
            if( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
            {
                pHurt->pev->punchangle.z = -18;
                pHurt->pev->punchangle.x = 5;
            }
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
        }
        else
        {
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, m_voicePitch );
        }
    }
    break;

    case ISLAVE_AE_ZAP_POWERUP:
    {
        // speed up attack when on hard
        if( g_cfg.GetSkillLevel() == SkillLevel::Hard )
            pev->framerate = 1.5;

        UTIL_MakeAimVectors( pev->angles );

        if( m_iBeams == 0 )
        {
            Vector vecSrc = pev->origin + gpGlobals->v_forward * 2;
            MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
            WRITE_BYTE( TE_DLIGHT );
            WRITE_COORD( vecSrc.x );             // X
            WRITE_COORD( vecSrc.y );             // Y
            WRITE_COORD( vecSrc.z );             // Z
            WRITE_BYTE( 12 );                     // radius * 0.1
            WRITE_BYTE( 255 );                 // r
            WRITE_BYTE( 180 );                 // g
            WRITE_BYTE( 96 );                     // b
            WRITE_BYTE( 20 / pev->framerate ); // time * 10
            WRITE_BYTE( 0 );                     // decay * 0.1
            MESSAGE_END();
        }
        if( m_hDead != nullptr )
        {
            WackBeam( -1, m_hDead );
            WackBeam( 1, m_hDead );
        }
        else
        {
            ArmBeam( -1 );
            ArmBeam( 1 );
            BeamGlow();
        }

        EmitSoundDyn( CHAN_WEAPON, "debris/zap4.wav", 1, ATTN_NORM, 0, 100 + m_iBeams * 10 );
        pev->skin = m_iBeams / 2;
    }
    break;

    case ISLAVE_AE_ZAP_SHOOT:
    {
        ClearBeams();

        if( m_hDead != nullptr )
        {
            Vector vecDest = m_hDead->pev->origin + Vector( 0, 0, 38 );
            TraceResult trace;
            UTIL_TraceHull( vecDest, vecDest, dont_ignore_monsters, human_hull, m_hDead->edict(), &trace );

            if( 0 == trace.fStartSolid )
            {
                CBaseEntity* pNew = Create( "monster_alien_slave", m_hDead->pev->origin, m_hDead->pev->angles );
                pNew->pev->spawnflags |= 1;
                WackBeam( -1, pNew );
                WackBeam( 1, pNew );
                UTIL_Remove( m_hDead );
                EmitSoundDyn( CHAN_WEAPON, "hassault/hw_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 130, 160 ) );
                break;
            }
        }
        ClearMultiDamage();

        UTIL_MakeAimVectors( pev->angles );

        ZapBeam( -1 );
        ZapBeam( 1 );

        EmitSoundDyn( CHAN_WEAPON, "hassault/hw_shoot1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 130, 160 ) );
        // StopSound(CHAN_WEAPON, "debris/zap4.wav");
        ApplyMultiDamage( this, this );

        m_flNextAttack = gpGlobals->time + RANDOM_FLOAT( 0.5, 4.0 );
    }
    break;

    case ISLAVE_AE_ZAP_DONE:
    {
        ClearBeams();
    }
    break;

    default:
        CSquadMonster::HandleAnimEvent( pEvent );
        break;
    }
}

bool CISlave::CheckRangeAttack1( float flDot, float flDist )
{
    if( m_flNextAttack > gpGlobals->time )
    {
        return false;
    }

    return CSquadMonster::CheckRangeAttack1( flDot, flDist );
}

bool CISlave::CheckRangeAttack2( float flDot, float flDist )
{
    return false;

    /*
    if (m_flNextAttack > gpGlobals->time)
    {
        return false;
    }

    m_hDead = nullptr;
    m_iBravery = 0;

    CBaseEntity* pEntity = nullptr;
    while ((pEntity = UTIL_FindEntityByClassname(pEntity, "monster_alien_slave")) != nullptr)
    {
        TraceResult tr;

        UTIL_TraceLine(EyePosition(), pEntity->EyePosition(), ignore_monsters, edict(), &tr);
        if (tr.flFraction == 1.0 || tr.pHit == pEntity->edict())
        {
            if (pEntity->pev->deadflag == DEAD_DEAD)
            {
                float d = (pev->origin - pEntity->pev->origin).Length();
                if (d < flDist)
                {
                    m_hDead = pEntity;
                    flDist = d;
                }
                m_iBravery--;
            }
            else
            {
                m_iBravery++;
            }
        }
    }
    if (m_hDead != nullptr)
        return true;
    else
        return false;
    */
}

void CISlave::StartTask( const Task_t* pTask )
{
    ClearBeams();

    CSquadMonster::StartTask( pTask );
}

SpawnAction CISlave::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "islave_health"sv, 60, this );

    SetModel( STRING( pev->model ) );
    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_GREEN;
    pev->effects = 0;
    pev->view_ofs = Vector( 0, 0, 64 );  // position of the eyes relative to monster's origin.
    m_flFieldOfView = VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
    m_MonsterState = MONSTERSTATE_NONE;
    m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK2 | bits_CAP_DOORS_GROUP;

    m_voicePitch = RANDOM_LONG( 85, 110 );

    MonsterInit();

    return SpawnAction::Spawn;
}

void CISlave::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheModel( "sprites/lgtning.spr" );
    PrecacheSound( "debris/zap1.wav" );
    PrecacheSound( "debris/zap4.wav" );
    PrecacheSound( "weapons/electro4.wav" );
    PrecacheSound( "hassault/hw_shoot1.wav" );
    PrecacheSound( "zombie/zo_pain2.wav" );
    PrecacheSound( "headcrab/hc_headbite.wav" );
    PrecacheSound( "weapons/cbar_miss1.wav" );

    PRECACHE_SOUND_ARRAY( pAttackHitSounds );
    PRECACHE_SOUND_ARRAY( pAttackMissSounds );
    PRECACHE_SOUND_ARRAY( pPainSounds );
    PRECACHE_SOUND_ARRAY( pDeathSounds );

    UTIL_PrecacheOther( "test_effect" );
}

bool CISlave::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    // don't slash one of your own
    if( ( bitsDamageType & DMG_SLASH ) != 0 && attacker && IRelationship( Instance( attacker ) ) < Relationship::Dislike )
        return false;

    // get provoked when injured
    m_afMemory |= bits_MEMORY_PROVOKED;
    return CSquadMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CISlave::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    if( ( bitsDamageType & DMG_SHOCK ) != 0 )
        return;

    CSquadMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
}

Task_t tlSlaveAttack1[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slSlaveAttack1[] =
    {
        {tlSlaveAttack1,
            std::size( tlSlaveAttack1 ),
            bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_HEAR_SOUND |
                bits_COND_HEAVY_DAMAGE,

            bits_SOUND_DANGER,
            "Slave Range Attack1"},
};

BEGIN_CUSTOM_SCHEDULES( CISlave )
slSlaveAttack1
END_CUSTOM_SCHEDULES();

const Schedule_t* CISlave::GetSchedule()
{
    ClearBeams();

    /*
        if (pev->spawnflags)
        {
            pev->spawnflags = 0;
            return GetScheduleOfType( SCHED_RELOAD );
        }
    */

    if( HasConditions( bits_COND_HEAR_SOUND ) )
    {
        CSound* pSound;
        pSound = PBestSound();

        ASSERT( pSound != nullptr );

        if( pSound && ( pSound->m_iType & bits_SOUND_DANGER ) != 0 )
            return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
        if( ( pSound->m_iType & bits_SOUND_COMBAT ) != 0 )
            m_afMemory |= bits_MEMORY_PROVOKED;
    }

    switch ( m_MonsterState )
    {
    case MONSTERSTATE_COMBAT:
        // dead enemy
        if( HasConditions( bits_COND_ENEMY_DEAD ) )
        {
            // call base class, all code to handle dead enemies is centralized there.
            return CBaseMonster::GetSchedule();
        }

        if( pev->health < 20 || m_iBravery < 0 )
        {
            if( !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
            {
                m_failSchedule = SCHED_CHASE_ENEMY;
                if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
                {
                    return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
                }
                if( HasConditions( bits_COND_SEE_ENEMY ) && HasConditions( bits_COND_ENEMY_FACING_ME ) )
                {
                    // AILogger->debug("exposed");
                    return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
                }
            }
        }
        break;
    }
    return CSquadMonster::GetSchedule();
}

const Schedule_t* CISlave::GetScheduleOfType( int Type )
{
    switch ( Type )
    {
    case SCHED_FAIL:
        if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
        {
            return CSquadMonster::GetScheduleOfType( SCHED_MELEE_ATTACK1 );
        }
        break;
    case SCHED_RANGE_ATTACK1:
        return slSlaveAttack1;
    case SCHED_RANGE_ATTACK2:
        return slSlaveAttack1;
    }
    return CSquadMonster::GetScheduleOfType( Type );
}

void CISlave::ArmBeam( int side )
{
    TraceResult tr;
    float flDist = 1.0;

    if( m_iBeams >= ISLAVE_MAX_BEAMS )
        return;

    UTIL_MakeAimVectors( pev->angles );
    Vector vecSrc = pev->origin + gpGlobals->v_up * 36 + gpGlobals->v_right * side * 16 + gpGlobals->v_forward * 32;

    for( int i = 0; i < 3; i++ )
    {
        Vector vecAim = gpGlobals->v_right * side * RANDOM_FLOAT( 0, 1 ) + gpGlobals->v_up * RANDOM_FLOAT( -1, 1 );
        TraceResult tr1;
        UTIL_TraceLine( vecSrc, vecSrc + vecAim * 512, dont_ignore_monsters, edict(), &tr1 );
        if( flDist > tr1.flFraction )
        {
            tr = tr1;
            flDist = tr.flFraction;
        }
    }

    // Couldn't find anything close enough
    if( flDist == 1.0 )
        return;

    DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

    m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );
    if( !m_pBeam[m_iBeams] )
        return;

    m_pBeam[m_iBeams]->PointEntInit( tr.vecEndPos, entindex() );
    m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
    // m_pBeam[m_iBeams]->SetColor( 180, 255, 96 );
    m_pBeam[m_iBeams]->SetColor( 96, 128, 16 );
    m_pBeam[m_iBeams]->SetBrightness( 64 );
    m_pBeam[m_iBeams]->SetNoise( 80 );
    m_pBeam[m_iBeams]->pev->spawnflags |= SF_BEAM_TEMPORARY; // Flag these to be destroyed on save/restore or level transition
    m_iBeams++;
}

void CISlave::BeamGlow()
{
    int b = m_iBeams * 32;
    if( b > 255 )
        b = 255;

    for( int i = 0; i < m_iBeams; i++ )
    {
        if( m_pBeam[i]->GetBrightness() != 255 )
        {
            m_pBeam[i]->SetBrightness( b );
        }
    }
}

void CISlave::WackBeam( int side, CBaseEntity* pEntity )
{
    if( m_iBeams >= ISLAVE_MAX_BEAMS )
        return;

    if( pEntity == nullptr )
        return;

    m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 30 );
    if( !m_pBeam[m_iBeams] )
        return;

    m_pBeam[m_iBeams]->PointEntInit( pEntity->Center(), entindex() );
    m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
    m_pBeam[m_iBeams]->SetColor( 180, 255, 96 );
    m_pBeam[m_iBeams]->SetBrightness( 255 );
    m_pBeam[m_iBeams]->SetNoise( 80 );
    m_pBeam[m_iBeams]->pev->spawnflags |= SF_BEAM_TEMPORARY; // Flag these to be destroyed on save/restore or level transition
    m_iBeams++;
}

void CISlave::ZapBeam( int side )
{
    Vector vecSrc, vecAim;
    TraceResult tr;
    CBaseEntity* pEntity;

    if( m_iBeams >= ISLAVE_MAX_BEAMS )
        return;

    vecSrc = pev->origin + gpGlobals->v_up * 36;
    vecAim = ShootAtEnemy( vecSrc );
    float deflection = 0.01;
    vecAim = vecAim + side * gpGlobals->v_right * RANDOM_FLOAT( 0, deflection ) + gpGlobals->v_up * RANDOM_FLOAT( -deflection, deflection );
    UTIL_TraceLine( vecSrc, vecSrc + vecAim * 1024, dont_ignore_monsters, edict(), &tr );

    m_pBeam[m_iBeams] = CBeam::BeamCreate( "sprites/lgtning.spr", 50 );
    if( !m_pBeam[m_iBeams] )
        return;

    m_pBeam[m_iBeams]->PointEntInit( tr.vecEndPos, entindex() );
    m_pBeam[m_iBeams]->SetEndAttachment( side < 0 ? 2 : 1 );
    m_pBeam[m_iBeams]->SetColor( 180, 255, 96 );
    m_pBeam[m_iBeams]->SetBrightness( 255 );
    m_pBeam[m_iBeams]->SetNoise( 20 );
    m_pBeam[m_iBeams]->pev->spawnflags |= SF_BEAM_TEMPORARY; // Flag these to be destroyed on save/restore or level transition
    m_iBeams++;

    pEntity = CBaseEntity::Instance( tr.pHit );
    if( pEntity != nullptr && 0 != pEntity->pev->takedamage )
    {
        pEntity->TraceAttack( this, g_cfg.GetValue<float>( "islave_dmg_zap"sv, 15, this ), vecAim, &tr, DMG_SHOCK );
    }
    EmitAmbientSound( tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );
}

void CISlave::UpdateOnRemove()
{
    ClearBeams();
    CSquadMonster::UpdateOnRemove();
}

void CISlave::ClearBeams()
{
    for( int i = 0; i < ISLAVE_MAX_BEAMS; i++ )
    {
        if( m_pBeam[i] )
        {
            UTIL_Remove( m_pBeam[i] );
            m_pBeam[i] = nullptr;
        }
    }
    m_iBeams = 0;
    pev->skin = 0;

    StopSound( CHAN_WEAPON, "debris/zap4.wav" );
}

/**
 *    @brief DEAD ALIEN SLAVE PROP
 *    @details Designer selects a pose in worldcraft, 0 through num_poses-1
 *    this value is added to what is selected as the 'first dead pose' among the monster's normal animations.
 *    All dead poses must appear sequentially in the model file.
 *    Be sure and set the m_iFirstPose properly!
 */
class CDeadISlave : public CBaseMonster
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;

    bool HasAlienGibs() override { return true; }

    bool KeyValue( KeyValueData* pkvd ) override;

    int m_iPose; // which sequence to display    -- temporary, don't need to save
    static const char* m_szPoses[1];
};

const char* CDeadISlave::m_szPoses[] = {"dead_on_stomach"};

bool CDeadISlave::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_alien_slave_dead, CDeadISlave );

void CDeadISlave::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8;
    pev->model = MAKE_STRING( "models/islave.mdl" );

    SetClassification( "alien_passive" );
}

SpawnAction CDeadISlave::Spawn()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    pev->effects = 0;
    pev->sequence = 0;
    m_bloodColor = BLOOD_COLOR_GREEN;

    pev->sequence = LookupSequence( m_szPoses[m_iPose] );
    if( pev->sequence == -1 )
    {
        AILogger->debug( "Dead slave with bad pose" );
    }

    MonsterInitDead();

    return SpawnAction::Spawn;
}
