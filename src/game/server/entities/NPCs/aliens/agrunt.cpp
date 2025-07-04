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
#include "hornet.h"

enum
{
    SCHED_AGRUNT_SUPPRESS = LAST_COMMON_SCHEDULE + 1,
    SCHED_AGRUNT_THREAT_DISPLAY,
};

enum
{
    TASK_AGRUNT_SETUP_HIDE_ATTACK = LAST_COMMON_TASK + 1,
    TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE,
};

int iAgruntMuzzleFlash;

#define AGRUNT_AE_HORNET1 (1)
#define AGRUNT_AE_HORNET2 (2)
#define AGRUNT_AE_HORNET3 (3)
#define AGRUNT_AE_HORNET4 (4)
#define AGRUNT_AE_HORNET5 (5)
// some events are set up in the QC file that aren't recognized by the code yet.
#define AGRUNT_AE_PUNCH (6)
#define AGRUNT_AE_BITE (7)

#define AGRUNT_AE_LEFT_FOOT (10)
#define AGRUNT_AE_RIGHT_FOOT (11)

#define AGRUNT_AE_LEFT_PUNCH (12)
#define AGRUNT_AE_RIGHT_PUNCH (13)

#define AGRUNT_MELEE_DIST 100

/**
 *    @brief Dominant, warlike alien grunt monster
 */
class CAGrunt : public CSquadMonster
{
    DECLARE_CLASS( CAGrunt, CSquadMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    int ISoundMask() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    void SetObjectCollisionBox() override
    {
        pev->absmin = pev->origin + Vector( -32, -32, 0 );
        pev->absmax = pev->origin + Vector( 32, 32, 85 );
    }

    bool HasAlienGibs() override { return true; }

    const Schedule_t* GetSchedule() override;
    const Schedule_t* GetScheduleOfType( int Type ) override;

    /**
     *    @brief this is overridden for alien grunts because they can use their smart weapons against unseen enemies.
     *    Base class doesn't attack anyone it can't see.
     */
    bool FCanCheckAttacks() override;

    /**
     *    @brief alien grunts zap the crap out of any enemy that gets too close.
     */
    bool CheckMeleeAttack1( float flDot, float flDist ) override;
    bool CheckRangeAttack1( float flDot, float flDist ) override;
    void StartTask( const Task_t* pTask ) override;
    void AlertSound() override;
    void DeathSound() override;
    void PainSound() override;
    void AttackSound();
    void PrescheduleThink() override;
    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;
    Relationship IRelationship( CBaseEntity* pTarget ) override;
    void StopTalking();
    bool ShouldSpeak();

    static const char* pAttackHitSounds[];
    static const char* pAttackMissSounds[];
    static const char* pAttackSounds[];
    static const char* pDieSounds[];
    static const char* pPainSounds[];
    static const char* pIdleSounds[];
    static const char* pAlertSounds[];

    bool m_fCanHornetAttack;
    float m_flNextHornetAttackCheck;

    float m_flNextPainTime;

    // three hacky fields for speech stuff. These don't really need to be saved.
    float m_flNextSpeakTime;
    float m_flNextWordTime;
    int m_iLastWord;
};

LINK_ENTITY_TO_CLASS( monster_alien_grunt, CAGrunt );

BEGIN_DATAMAP( CAGrunt )
    DEFINE_FIELD( m_fCanHornetAttack, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flNextHornetAttackCheck, FIELD_TIME ),
    DEFINE_FIELD( m_flNextPainTime, FIELD_TIME ),
    DEFINE_FIELD( m_flNextSpeakTime, FIELD_TIME ),
    DEFINE_FIELD( m_flNextWordTime, FIELD_TIME ),
    DEFINE_FIELD( m_iLastWord, FIELD_INTEGER ),
END_DATAMAP();

const char* CAGrunt::pAttackHitSounds[] =
    {
        "zombie/claw_strike1.wav",
        "zombie/claw_strike2.wav",
        "zombie/claw_strike3.wav",
};

const char* CAGrunt::pAttackMissSounds[] =
    {
        "zombie/claw_miss1.wav",
        "zombie/claw_miss2.wav",
};

const char* CAGrunt::pAttackSounds[] =
    {
        "agrunt/ag_attack1.wav",
        "agrunt/ag_attack2.wav",
        "agrunt/ag_attack3.wav",
};

const char* CAGrunt::pDieSounds[] =
    {
        "agrunt/ag_die1.wav",
        "agrunt/ag_die4.wav",
        "agrunt/ag_die5.wav",
};

const char* CAGrunt::pPainSounds[] =
    {
        "agrunt/ag_pain1.wav",
        "agrunt/ag_pain2.wav",
        "agrunt/ag_pain3.wav",
        "agrunt/ag_pain4.wav",
        "agrunt/ag_pain5.wav",
};

const char* CAGrunt::pIdleSounds[] =
    {
        "agrunt/ag_idle1.wav",
        "agrunt/ag_idle2.wav",
        "agrunt/ag_idle3.wav",
        "agrunt/ag_idle4.wav",
};

const char* CAGrunt::pAlertSounds[] =
    {
        "agrunt/ag_alert1.wav",
        "agrunt/ag_alert3.wav",
        "agrunt/ag_alert4.wav",
        "agrunt/ag_alert5.wav",
};

void CAGrunt::OnCreate()
{
    CSquadMonster::OnCreate();

    pev->model = MAKE_STRING( "models/agrunt.mdl" );

    SetClassification( "alien_military" );
}

Relationship CAGrunt::IRelationship( CBaseEntity* pTarget )
{
    if( pTarget->ClassnameIs( "monster_human_grunt" ) )
    {
        return Relationship::Nemesis;
    }

    return CSquadMonster::IRelationship( pTarget );
}

int CAGrunt::ISoundMask()
{
    return bits_SOUND_WORLD |
           bits_SOUND_COMBAT |
           bits_SOUND_PLAYER |
           bits_SOUND_DANGER;
}

void CAGrunt::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    if( ptr->iHitgroup == 10 && ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB ) ) != 0 )
    {
        // hit armor
        if( pev->dmgtime != gpGlobals->time || ( RANDOM_LONG( 0, 10 ) < 1 ) )
        {
            UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2 ) );
            pev->dmgtime = gpGlobals->time;
        }

        if( RANDOM_LONG( 0, 1 ) == 0 )
        {
            Vector vecTracerDir = vecDir;

            vecTracerDir.x += RANDOM_FLOAT( -0.3, 0.3 );
            vecTracerDir.y += RANDOM_FLOAT( -0.3, 0.3 );
            vecTracerDir.z += RANDOM_FLOAT( -0.3, 0.3 );

            vecTracerDir = vecTracerDir * -512;

            MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
            WRITE_BYTE( TE_TRACER );
            WRITE_COORD( ptr->vecEndPos.x );
            WRITE_COORD( ptr->vecEndPos.y );
            WRITE_COORD( ptr->vecEndPos.z );

            WRITE_COORD( vecTracerDir.x );
            WRITE_COORD( vecTracerDir.y );
            WRITE_COORD( vecTracerDir.z );
            MESSAGE_END();
        }

        if( int deduction = g_cfg.GetValue<float>( "agrunt_dmg_armor"sv, 20, this ); deduction > 0 )
        {
            flDamage -= deduction;
            if( flDamage <= 0 )
                flDamage = 0.1; // don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
        }
    }
    else
    {
        SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage ); // a little surface blood.
        TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
    }

    AddMultiDamage( attacker, this, flDamage, bitsDamageType );
}

void CAGrunt::StopTalking()
{
    m_flNextWordTime = m_flNextSpeakTime = gpGlobals->time + 10 + RANDOM_LONG( 0, 10 );
}

bool CAGrunt::ShouldSpeak()
{
    if( m_flNextSpeakTime > gpGlobals->time )
    {
        // my time to talk is still in the future.
        return false;
    }

    if( ( pev->spawnflags & SF_MONSTER_GAG ) != 0 )
    {
        if( m_MonsterState != MONSTERSTATE_COMBAT )
        {
            // if gagged, don't talk outside of combat.
            // if not going to talk because of this, put the talk time
            // into the future a bit, so we don't talk immediately after
            // going into combat
            m_flNextSpeakTime = gpGlobals->time + 3;
            return false;
        }
    }

    return true;
}

void CAGrunt::PrescheduleThink()
{
    if( ShouldSpeak() )
    {
        if( m_flNextWordTime < gpGlobals->time )
        {
            int num = -1;

            do
            {
                num = RANDOM_LONG( 0, std::size( pIdleSounds ) - 1 );
            } while( num == m_iLastWord );

            m_iLastWord = num;

            // play a new sound
            EmitSound( CHAN_VOICE, pIdleSounds[num], 1.0, ATTN_NORM );

            // is this word our last?
            if( RANDOM_LONG( 1, 10 ) <= 1 )
            {
                // stop talking.
                StopTalking();
            }
            else
            {
                m_flNextWordTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
            }
        }
    }
}

void CAGrunt::DeathSound()
{
    StopTalking();

    EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pDieSounds ), 1.0, ATTN_NORM );
}

void CAGrunt::AlertSound()
{
    StopTalking();

    EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0, ATTN_NORM );
}

void CAGrunt::AttackSound()
{
    StopTalking();

    EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_NORM );
}

void CAGrunt::PainSound()
{
    if( m_flNextPainTime > gpGlobals->time )
    {
        return;
    }

    m_flNextPainTime = gpGlobals->time + 0.6;

    StopTalking();

    EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM );
}

void CAGrunt::SetYawSpeed()
{
    int ys;

    switch ( m_Activity )
    {
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        ys = g_cfg.GetValue<int>( "agrunt_yawspd"sv, 110, this );
        break;
    default:
        ys = g_cfg.GetValue<int>( "agrunt_yawspd_normal"sv, 100, this );
    }

    pev->yaw_speed = ys;
}

void CAGrunt::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case AGRUNT_AE_HORNET1:
    case AGRUNT_AE_HORNET2:
    case AGRUNT_AE_HORNET3:
    case AGRUNT_AE_HORNET4:
    case AGRUNT_AE_HORNET5:
    {
        // m_vecEnemyLKP should be center of enemy body
        Vector vecArmPos, vecArmDir;
        Vector vecDirToEnemy;
        Vector angDir;

        if( HasConditions( bits_COND_SEE_ENEMY ) )
        {
            vecDirToEnemy = ( ( m_vecEnemyLKP )-pev->origin );
            angDir = UTIL_VecToAngles( vecDirToEnemy );
            vecDirToEnemy = vecDirToEnemy.Normalize();
        }
        else
        {
            angDir = pev->angles;
            UTIL_MakeAimVectors( angDir );
            vecDirToEnemy = gpGlobals->v_forward;
        }

        pev->effects = EF_MUZZLEFLASH;

        // make angles +-180
        if( angDir.x > 180 )
        {
            angDir.x = angDir.x - 360;
        }

        SetBlending( 0, angDir.x );
        GetAttachment( 0, vecArmPos, vecArmDir );

        vecArmPos = vecArmPos + vecDirToEnemy * 32;
        MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecArmPos );
        WRITE_BYTE( TE_SPRITE );
        WRITE_COORD( vecArmPos.x ); // pos
        WRITE_COORD( vecArmPos.y );
        WRITE_COORD( vecArmPos.z );
        WRITE_SHORT( iAgruntMuzzleFlash ); // model
        WRITE_BYTE( 6 );                     // size * 10
        WRITE_BYTE( 128 );                 // brightness
        MESSAGE_END();

        CBaseEntity* pHornet = CBaseEntity::Create( "hornet", vecArmPos, UTIL_VecToAngles( vecDirToEnemy ), this );
        UTIL_MakeVectors( pHornet->pev->angles );
        pHornet->pev->velocity = gpGlobals->v_forward * 300;



        switch ( RANDOM_LONG( 0, 2 ) )
        {
        case 0:
            EmitSound( CHAN_WEAPON, "agrunt/ag_fire1.wav", 1.0, ATTN_NORM );
            break;
        case 1:
            EmitSound( CHAN_WEAPON, "agrunt/ag_fire2.wav", 1.0, ATTN_NORM );
            break;
        case 2:
            EmitSound( CHAN_WEAPON, "agrunt/ag_fire3.wav", 1.0, ATTN_NORM );
            break;
        }

        CBaseMonster* pHornetMonster = pHornet->MyMonsterPointer();

        if( pHornetMonster )
        {
            pHornetMonster->m_hEnemy = m_hEnemy;
        }
    }
    break;

    case AGRUNT_AE_LEFT_FOOT:
        switch ( RANDOM_LONG( 0, 1 ) )
        {
            // left foot
        case 0:
            EmitSoundDyn( CHAN_BODY, "player/pl_ladder2.wav", 1, ATTN_NORM, 0, 70 );
            break;
        case 1:
            EmitSoundDyn( CHAN_BODY, "player/pl_ladder4.wav", 1, ATTN_NORM, 0, 70 );
            break;
        }
        break;
    case AGRUNT_AE_RIGHT_FOOT:
        // right foot
        switch ( RANDOM_LONG( 0, 1 ) )
        {
        case 0:
            EmitSoundDyn( CHAN_BODY, "player/pl_ladder1.wav", 1, ATTN_NORM, 0, 70 );
            break;
        case 1:
            EmitSoundDyn( CHAN_BODY, "player/pl_ladder3.wav", 1, ATTN_NORM, 0, 70 );
            break;
        }
        break;

    case AGRUNT_AE_LEFT_PUNCH:
    {
        CBaseEntity* pHurt = CheckTraceHullAttack( AGRUNT_MELEE_DIST, g_cfg.GetValue<float>( "agrunt_dmg_punch"sv, 20, this ), DMG_CLUB );

        if( pHurt )
        {
            pHurt->pev->punchangle.y = -25;
            pHurt->pev->punchangle.x = 8;

            // OK to use gpGlobals without calling MakeVectors, cause CheckTraceHullAttack called it above.
            if( pHurt->IsPlayer() )
            {
                // this is a player. Knock him around.
                pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * g_cfg.GetValue<float>( "agrunt_player_knock"sv, 250, this );
            }

            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

            Vector vecArmPos, vecArmAng;
            GetAttachment( 0, vecArmPos, vecArmAng );
            SpawnBlood( vecArmPos, pHurt->BloodColor(), 25 ); // a little surface blood.
        }
        else
        {
            // Play a random attack miss sound
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
        }
    }
    break;

    case AGRUNT_AE_RIGHT_PUNCH:
    {
        CBaseEntity* pHurt = CheckTraceHullAttack( AGRUNT_MELEE_DIST, g_cfg.GetValue<float>( "agrunt_dmg_punch"sv, 20, this ), DMG_CLUB );

        if( pHurt )
        {
            pHurt->pev->punchangle.y = 25;
            pHurt->pev->punchangle.x = 8;

            // OK to use gpGlobals without calling MakeVectors, cause CheckTraceHullAttack called it above.
            if( pHurt->IsPlayer() )
            {
                // this is a player. Knock him around.
                pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * -g_cfg.GetValue<float>( "agrunt_player_knock"sv, 250, this );
            }

            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

            Vector vecArmPos, vecArmAng;
            GetAttachment( 0, vecArmPos, vecArmAng );
            SpawnBlood( vecArmPos, pHurt->BloodColor(), 25 ); // a little surface blood.
        }
        else
        {
            // Play a random attack miss sound
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
        }
    }
    break;

    default:
        CSquadMonster::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CAGrunt::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "agrunt_health"sv, 120, this );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_GREEN;
    pev->effects = 0;
    m_flFieldOfView = g_cfg.GetValue<float>( "agrunt_fov"sv, 0.2, this ); // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;
    m_afCapability = 0;
    m_afCapability |= bits_CAP_SQUAD;

    m_HackedGunPos = Vector( 24, 64, 48 );

    m_flNextSpeakTime = m_flNextWordTime = gpGlobals->time + 10 + RANDOM_LONG( 0, 10 );


    MonsterInit();

    return SpawnAction::Spawn;
}

void CAGrunt::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PRECACHE_SOUND_ARRAY( pAttackHitSounds );
    PRECACHE_SOUND_ARRAY( pAttackMissSounds );
    PRECACHE_SOUND_ARRAY( pIdleSounds );
    PRECACHE_SOUND_ARRAY( pDieSounds );
    PRECACHE_SOUND_ARRAY( pPainSounds );
    PRECACHE_SOUND_ARRAY( pAttackSounds );
    PRECACHE_SOUND_ARRAY( pAlertSounds );

    PrecacheSound( "hassault/hw_shoot1.wav" );

    iAgruntMuzzleFlash = PrecacheModel( "sprites/muz4.spr" );

    UTIL_PrecacheOther( "hornet" );
}

Task_t tlAGruntFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slAGruntFail[] =
    {
        {tlAGruntFail,
            std::size( tlAGruntFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK1,
            0,
            "AGrunt Fail"},
};

Task_t tlAGruntCombatFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slAGruntCombatFail[] =
    {
        {tlAGruntCombatFail,
            std::size( tlAGruntCombatFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK1,
            0,
            "AGrunt Combat Fail"},
};

Task_t tlAGruntStandoff[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)2},
};

/**
 *    @brief Used in combat when a monster is hiding in cover or the enemy has moved out of sight.
 *    Should we look around in this schedule?
 */
Schedule_t slAGruntStandoff[] =
    {
        {tlAGruntStandoff,
            std::size( tlAGruntStandoff ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_SEE_ENEMY |
                bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "Agrunt Standoff"}};

Task_t tlAGruntSuppressHornet[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slAGruntSuppress[] =
    {
        {
            tlAGruntSuppressHornet,
            std::size( tlAGruntSuppressHornet ),
            0,
            0,
            "AGrunt Suppress Hornet",
        },
};

Task_t tlAGruntRangeAttack1[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slAGruntRangeAttack1[] =
    {
        {tlAGruntRangeAttack1,
            std::size( tlAGruntRangeAttack1 ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_HEAVY_DAMAGE,

            0,
            "AGrunt Range Attack1"},
};


Task_t tlAGruntHiddenRangeAttack1[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_STANDOFF},
        {TASK_AGRUNT_SETUP_HIDE_ATTACK, 0},
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_IDEAL, 0},
        {TASK_RANGE_ATTACK1_NOTURN, (float)0},
};

Schedule_t slAGruntHiddenRangeAttack[] =
    {
        {tlAGruntHiddenRangeAttack1,
            std::size( tlAGruntHiddenRangeAttack1 ),
            bits_COND_NEW_ENEMY |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "AGrunt Hidden Range Attack1"},
};

Task_t tlAGruntTakeCoverFromEnemy[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_WAIT, (float)0.2},
        {TASK_FIND_COVER_FROM_ENEMY, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
        {TASK_FACE_ENEMY, (float)0},
};

/**
 *    @brief Tries lateral cover before node cover!
 */
Schedule_t slAGruntTakeCoverFromEnemy[] =
    {
        {tlAGruntTakeCoverFromEnemy,
            std::size( tlAGruntTakeCoverFromEnemy ),
            bits_COND_NEW_ENEMY,
            0,
            "AGruntTakeCoverFromEnemy"},
};

Task_t tlAGruntVictoryDance[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_AGRUNT_THREAT_DISPLAY},
        {TASK_WAIT, (float)0.2},
        {TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE, (float)0},
        {TASK_WALK_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_CROUCH},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_STAND},
        {TASK_PLAY_SEQUENCE, (float)ACT_THREAT_DISPLAY},
        {TASK_PLAY_SEQUENCE, (float)ACT_CROUCH},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
        {TASK_PLAY_SEQUENCE, (float)ACT_STAND},
};

Schedule_t slAGruntVictoryDance[] =
    {
        {tlAGruntVictoryDance,
            std::size( tlAGruntVictoryDance ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "AGruntVictoryDance"},
};

Task_t tlAGruntThreatDisplay[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_THREAT_DISPLAY},
};

Schedule_t slAGruntThreatDisplay[] =
    {
        {tlAGruntThreatDisplay,
            std::size( tlAGruntThreatDisplay ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,

            bits_SOUND_PLAYER |
                bits_SOUND_COMBAT |
                bits_SOUND_WORLD,
            "AGruntThreatDisplay"},
};

BEGIN_CUSTOM_SCHEDULES( CAGrunt )
    slAGruntFail,
    slAGruntCombatFail,
    slAGruntStandoff,
    slAGruntSuppress,
    slAGruntRangeAttack1,
    slAGruntHiddenRangeAttack,
    slAGruntTakeCoverFromEnemy,
    slAGruntVictoryDance,
    slAGruntThreatDisplay
END_CUSTOM_SCHEDULES();

bool CAGrunt::FCanCheckAttacks()
{
    if( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool CAGrunt::CheckMeleeAttack1( float flDot, float flDist )
{
    if( HasConditions( bits_COND_SEE_ENEMY ) && flDist <= AGRUNT_MELEE_DIST && flDot >= 0.6 && m_hEnemy != nullptr )
    {
        return true;
    }
    return false;
}

bool CAGrunt::CheckRangeAttack1( float flDot, float flDist )
{
    //!!!LATER - we may want to load balance this.Several
    // tracelines are done, so we may not want to do this every
    // server frame. Definitely not while firing.
    if( gpGlobals->time < m_flNextHornetAttackCheck )
    {
        return m_fCanHornetAttack;
    }

    if( HasConditions( bits_COND_SEE_ENEMY ) && flDist >= AGRUNT_MELEE_DIST && flDist <= 1024 && flDot >= 0.5 && NoFriendlyFire() )
    {
        TraceResult tr;
        Vector vecArmPos, vecArmDir;

        // verify that a shot fired from the gun will hit the enemy before the world.
        // !!!LATER - we may wish to do something different for projectile weapons as opposed to instant-hit
        UTIL_MakeVectors( pev->angles );
        GetAttachment( 0, vecArmPos, vecArmDir );
        //        UTIL_TraceLine( vecArmPos, vecArmPos + gpGlobals->v_forward * 256, ignore_monsters, edict(), &tr);
        UTIL_TraceLine( vecArmPos, m_hEnemy->BodyTarget( vecArmPos ), dont_ignore_monsters, edict(), &tr );

        if( tr.flFraction == 1.0 || tr.pHit == m_hEnemy->edict() )
        {
            m_flNextHornetAttackCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 );
            m_fCanHornetAttack = true;
            return m_fCanHornetAttack;
        }
    }

    m_flNextHornetAttackCheck = gpGlobals->time + 0.2; // don't check for half second if this check wasn't successful
    m_fCanHornetAttack = false;
    return m_fCanHornetAttack;
}

void CAGrunt::StartTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_AGRUNT_GET_PATH_TO_ENEMY_CORPSE:
    {
        UTIL_MakeVectors( pev->angles );
        if( BuildRoute( m_vecEnemyLKP - gpGlobals->v_forward * 50, bits_MF_TO_LOCATION, nullptr ) )
        {
            TaskComplete();
        }
        else
        {
            AILogger->debug( "AGruntGetPathToEnemyCorpse failed!!" );
            TaskFail();
        }
    }
    break;

    case TASK_AGRUNT_SETUP_HIDE_ATTACK:
        // alien grunt shoots hornets back out into the open from a concealed location.
        // try to find a spot to throw that gives the smart weapon a good chance of finding the enemy.
        // ideally, this spot is along a line that is perpendicular to a line drawn from the agrunt to the enemy.

        CBaseMonster* pEnemyMonsterPtr;

        pEnemyMonsterPtr = m_hEnemy->MyMonsterPointer();

        if( pEnemyMonsterPtr )
        {
            Vector vecCenter;
            TraceResult tr;
            bool fSkip;

            fSkip = false;
            vecCenter = Center();

            UTIL_VecToAngles( m_vecEnemyLKP - pev->origin );

            UTIL_TraceLine( Center() + gpGlobals->v_forward * 128, m_vecEnemyLKP, ignore_monsters, edict(), &tr );
            if( tr.flFraction == 1.0 )
            {
                MakeIdealYaw( pev->origin + gpGlobals->v_right * 128 );
                fSkip = true;
                TaskComplete();
            }

            if( !fSkip )
            {
                UTIL_TraceLine( Center() - gpGlobals->v_forward * 128, m_vecEnemyLKP, ignore_monsters, edict(), &tr );
                if( tr.flFraction == 1.0 )
                {
                    MakeIdealYaw( pev->origin - gpGlobals->v_right * 128 );
                    fSkip = true;
                    TaskComplete();
                }
            }

            if( !fSkip )
            {
                UTIL_TraceLine( Center() + gpGlobals->v_forward * 256, m_vecEnemyLKP, ignore_monsters, edict(), &tr );
                if( tr.flFraction == 1.0 )
                {
                    MakeIdealYaw( pev->origin + gpGlobals->v_right * 256 );
                    fSkip = true;
                    TaskComplete();
                }
            }

            if( !fSkip )
            {
                UTIL_TraceLine( Center() - gpGlobals->v_forward * 256, m_vecEnemyLKP, ignore_monsters, edict(), &tr );
                if( tr.flFraction == 1.0 )
                {
                    MakeIdealYaw( pev->origin - gpGlobals->v_right * 256 );
                    fSkip = true;
                    TaskComplete();
                }
            }

            if( !fSkip )
            {
                TaskFail();
            }
        }
        else
        {
            AILogger->debug( "AGRunt - no enemy monster ptr!!!" );
            TaskFail();
        }
        break;

    default:
        CSquadMonster::StartTask( pTask );
        break;
    }
}

const Schedule_t* CAGrunt::GetSchedule()
{
    if( HasConditions( bits_COND_HEAR_SOUND ) )
    {
        CSound* pSound;
        pSound = PBestSound();

        ASSERT( pSound != nullptr );
        if( pSound && ( pSound->m_iType & bits_SOUND_DANGER ) != 0 )
        {
            // dangerous sound nearby!
            return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
        }
    }

    switch ( m_MonsterState )
    {
    case MONSTERSTATE_COMBAT:
    {
        // dead enemy
        if( HasConditions( bits_COND_ENEMY_DEAD ) )
        {
            // call base class, all code to handle dead enemies is centralized there.
            return CBaseMonster::GetSchedule();
        }

        if( HasConditions( bits_COND_NEW_ENEMY ) )
        {
            return GetScheduleOfType( SCHED_WAKE_ANGRY );
        }

        // zap player!
        if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
        {
            AttackSound(); // this is a total hack. Should be parto f the schedule
            return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
        }

        if( HasConditions( bits_COND_HEAVY_DAMAGE ) )
        {
            return GetScheduleOfType( SCHED_SMALL_FLINCH );
        }

        // can attack
        if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_AGRUNT_HORNET ) )
        {
            return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
        }

        if( OccupySlot( bits_SLOT_AGRUNT_CHASE ) )
        {
            return GetScheduleOfType( SCHED_CHASE_ENEMY );
        }

        return GetScheduleOfType( SCHED_STANDOFF );
    }
    }

    return CSquadMonster::GetSchedule();
}

const Schedule_t* CAGrunt::GetScheduleOfType( int Type )
{
    switch ( Type )
    {
    case SCHED_TAKE_COVER_FROM_ENEMY:
        return &slAGruntTakeCoverFromEnemy[0];
        break;

    case SCHED_RANGE_ATTACK1:
        if( HasConditions( bits_COND_SEE_ENEMY ) )
        {
            // normal attack
            return &slAGruntRangeAttack1[0];
        }
        else
        {
            // attack an unseen enemy
            // return &slAGruntHiddenRangeAttack[ 0 ];
            return &slAGruntRangeAttack1[0];
        }
        break;

    case SCHED_AGRUNT_THREAT_DISPLAY:
        return &slAGruntThreatDisplay[0];
        break;

    case SCHED_AGRUNT_SUPPRESS:
        return &slAGruntSuppress[0];
        break;

    case SCHED_STANDOFF:
        return &slAGruntStandoff[0];
        break;

    case SCHED_VICTORY_DANCE:
        return &slAGruntVictoryDance[0];
        break;

    case SCHED_FAIL:
        // no fail schedule specified, so pick a good generic one.
        {
            if( m_hEnemy != nullptr )
            {
                // I have an enemy
                // !!!LATER - what if this enemy is really far away and i'm chasing him?
                // this schedule will make me stop, face his last known position for 2
                // seconds, and then try to move again
                return &slAGruntCombatFail[0];
            }

            return &slAGruntFail[0];
        }
        break;
    }

    return CSquadMonster::GetScheduleOfType( Type );
}
