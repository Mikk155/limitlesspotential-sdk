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
#include "flyingmonster.h"
#include "nodes.h"

#define SEARCH_RETRY 16

#define ICHTHYOSAUR_SPEED 150

#define EYE_MAD 0
#define EYE_BASE 1
#define EYE_CLOSED 2
#define EYE_BACK 3
#define EYE_LOOK 4

/**
 *    @brief evil, satan fish monster
 */
class CIchthyosaur : public CFlyingMonster
{
    DECLARE_CLASS( CIchthyosaur, CFlyingMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;

    bool HasAlienGibs() override { return true; }

    const Schedule_t* GetSchedule() override;
    const Schedule_t* GetScheduleOfType( int Type ) override;

    void Killed( CBaseEntity* attacker, int iGib ) override;
    void BecomeDead() override;

    void CombatUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
    void BiteTouch( CBaseEntity* pOther );

    void StartTask( const Task_t* pTask ) override;
    void RunTask( const Task_t* pTask ) override;

    bool CheckMeleeAttack1( float flDot, float flDist ) override;
    bool CheckRangeAttack1( float flDot, float flDist ) override; //!< swim in for a chomp

    float ChangeYaw( int speed ) override;
    Activity GetStoppedActivity() override;
    void SetActivity( Activity NewActivity ) override;

    void Move( float flInterval ) override;
    void MoveExecute( CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval ) override;
    void MonsterThink() override;
    void Stop() override;
    void Swim();
    Vector DoProbe( const Vector& Probe );

    float VectorToPitch( const Vector& vec );
    float FlPitchDiff();
    float ChangePitch( int speed );

    Vector m_SaveVelocity;
    float m_idealDist;

    float m_flBlink;

    float m_flEnemyTouched;
    bool m_bOnAttack;

    float m_flMaxSpeed;
    float m_flMinSpeed;
    float m_flMaxDist;

    CBeam* m_pBeam;

    float m_flNextAlert;

    float m_flLastPitchTime;

    static const char* pIdleSounds[];
    static const char* pAlertSounds[];
    static const char* pAttackSounds[];
    static const char* pBiteSounds[];
    static const char* pDieSounds[];
    static const char* pPainSounds[];

    void IdleSound() override;
    void AlertSound() override;
    void AttackSound();
    void BiteSound();
    void DeathSound() override;
    void PainSound() override;
};

LINK_ENTITY_TO_CLASS( monster_ichthyosaur, CIchthyosaur );

BEGIN_DATAMAP( CIchthyosaur )
    DEFINE_FIELD( m_SaveVelocity, FIELD_VECTOR ),
    DEFINE_FIELD( m_idealDist, FIELD_FLOAT ),
    DEFINE_FIELD( m_flBlink, FIELD_FLOAT ),
    DEFINE_FIELD( m_flEnemyTouched, FIELD_FLOAT ),
    DEFINE_FIELD( m_bOnAttack, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
    DEFINE_FIELD( m_flMinSpeed, FIELD_FLOAT ),
    DEFINE_FIELD( m_flMaxDist, FIELD_FLOAT ),
    DEFINE_FIELD( m_flNextAlert, FIELD_TIME ),
    DEFINE_FUNCTION( CombatUse ),
    DEFINE_FUNCTION( BiteTouch ),
END_DATAMAP();

const char* CIchthyosaur::pIdleSounds[] =
    {
        "ichy/ichy_idle1.wav",
        "ichy/ichy_idle2.wav",
        "ichy/ichy_idle3.wav",
        "ichy/ichy_idle4.wav",
};

const char* CIchthyosaur::pAlertSounds[] =
    {
        "ichy/ichy_alert2.wav",
        "ichy/ichy_alert3.wav",
};

const char* CIchthyosaur::pAttackSounds[] =
    {
        "ichy/ichy_attack1.wav",
        "ichy/ichy_attack2.wav",
};

const char* CIchthyosaur::pBiteSounds[] =
    {
        "ichy/ichy_bite1.wav",
        "ichy/ichy_bite2.wav",
};

const char* CIchthyosaur::pPainSounds[] =
    {
        "ichy/ichy_pain2.wav",
        "ichy/ichy_pain3.wav",
        "ichy/ichy_pain5.wav",
};

const char* CIchthyosaur::pDieSounds[] =
    {
        "ichy/ichy_die2.wav",
        "ichy/ichy_die4.wav",
};

void CIchthyosaur::OnCreate()
{
    CFlyingMonster::OnCreate();

    pev->model = MAKE_STRING( "models/icky.mdl" );

    SetClassification( "alien_monster" );
}

#define EMIT_ICKY_SOUND(chan, array) \
    EmitSoundDyn( chan, array[RANDOM_LONG( 0, std::size( array ) - 1 )], 1.0, 0.6, 0, RANDOM_LONG( 95, 105 ) );

void CIchthyosaur::IdleSound()
{
    EMIT_ICKY_SOUND( CHAN_VOICE, pIdleSounds );
}

void CIchthyosaur::AlertSound()
{
    EMIT_ICKY_SOUND( CHAN_VOICE, pAlertSounds );
}

void CIchthyosaur::AttackSound()
{
    EMIT_ICKY_SOUND( CHAN_VOICE, pAttackSounds );
}

void CIchthyosaur::BiteSound()
{
    EMIT_ICKY_SOUND( CHAN_WEAPON, pBiteSounds );
}

void CIchthyosaur::DeathSound()
{
    EMIT_ICKY_SOUND( CHAN_VOICE, pDieSounds );
}

void CIchthyosaur::PainSound()
{
    EMIT_ICKY_SOUND( CHAN_VOICE, pPainSounds );
}

enum
{
    TASK_ICHTHYOSAUR_CIRCLE_ENEMY = LAST_COMMON_TASK + 1,
    TASK_ICHTHYOSAUR_SWIM,
    TASK_ICHTHYOSAUR_FLOAT,
};

static Task_t tlSwimAround[] =
    {
        {TASK_SET_ACTIVITY, (float)ACT_WALK},
        {TASK_ICHTHYOSAUR_SWIM, 0.0},
};

static Schedule_t slSwimAround[] =
    {
        {tlSwimAround,
            std::size( tlSwimAround ),
            bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_SEE_ENEMY |
                bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND,
            bits_SOUND_PLAYER |
                bits_SOUND_COMBAT,
            "SwimAround"},
};

static Task_t tlSwimAgitated[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_RUN},
        {TASK_WAIT, (float)2.0},
};

static Schedule_t slSwimAgitated[] =
    {
        {tlSwimAgitated,
            std::size( tlSwimAgitated ),
            0,
            0,
            "SwimAgitated"},
};

static Task_t tlCircleEnemy[] =
    {
        {TASK_SET_ACTIVITY, (float)ACT_WALK},
        {TASK_ICHTHYOSAUR_CIRCLE_ENEMY, 0.0},
};

static Schedule_t slCircleEnemy[] =
    {
        {tlCircleEnemy,
            std::size( tlCircleEnemy ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK1,
            0,
            "CircleEnemy"},
};

Task_t tlTwitchDie[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SOUND_DIE, (float)0},
        {TASK_DIE, (float)0},
};

Schedule_t slTwitchDie[] =
    {
        {tlTwitchDie,
            std::size( tlTwitchDie ),
            0,
            0,
            "Die"},
};

Task_t tlFloat[] =
    {
        {TASK_ICHTHYOSAUR_FLOAT, (float)0},
};

Schedule_t slFloat[] =
    {
        {tlFloat,
            std::size( tlFloat ),
            0,
            0,
            "Float"},
};

BEGIN_CUSTOM_SCHEDULES( CIchthyosaur )
slSwimAround,
    slSwimAgitated,
    slCircleEnemy,
    slTwitchDie,
    slFloat
    END_CUSTOM_SCHEDULES();

bool CIchthyosaur::CheckMeleeAttack1( float flDot, float flDist )
{
    if( flDot >= 0.7 && m_flEnemyTouched > gpGlobals->time - 0.2 )
    {
        return true;
    }
    return false;
}

void CIchthyosaur::BiteTouch( CBaseEntity* pOther )
{
    // bite if we hit who we want to eat
    if( pOther == m_hEnemy )
    {
        m_flEnemyTouched = gpGlobals->time;
        m_bOnAttack = true;
    }
}

void CIchthyosaur::CombatUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !ShouldToggle( useType, m_bOnAttack ) )
        return;

    m_bOnAttack = !m_bOnAttack;
}

bool CIchthyosaur::CheckRangeAttack1( float flDot, float flDist )
{
    if( flDot > -0.7 && ( m_bOnAttack || ( flDist <= 192 && m_idealDist <= 192 ) ) )
    {
        return true;
    }

    return false;
}

void CIchthyosaur::SetYawSpeed()
{
    pev->yaw_speed = 100;
}

void CIchthyosaur::Killed( CBaseEntity* attacker, int iGib )
{
    // overrides CFlyingMonster.
    CBaseMonster::Killed( attacker, iGib );
    pev->velocity = Vector( 0, 0, 0 );
}

void CIchthyosaur::BecomeDead()
{
    pev->takedamage = DAMAGE_YES; // don't let autoaim aim at corpses.

    // give the corpse half of the monster's original maximum health.
    if( pev->health < 1 )
        pev->health = pev->max_health / 2;
    pev->max_health = 5; // max_health now becomes a counter for how many blood decals the corpse can place.
}

#define ICHTHYOSAUR_AE_SHAKE_RIGHT 1
#define ICHTHYOSAUR_AE_SHAKE_LEFT 2

void CIchthyosaur::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    bool bDidAttack = false;
    switch ( pEvent->event )
    {
    case ICHTHYOSAUR_AE_SHAKE_RIGHT:
    case ICHTHYOSAUR_AE_SHAKE_LEFT:
    {
        if( m_hEnemy != nullptr && FVisible( m_hEnemy ) )
        {
            CBaseEntity* pHurt = m_hEnemy;

            if( m_flEnemyTouched < gpGlobals->time - 0.2 && ( m_hEnemy->BodyTarget( pev->origin ) - pev->origin ).Length() > ( 32 + 16 + 32 ) )
                break;

            Vector vecShootDir = ShootAtEnemy( pev->origin );
            UTIL_MakeAimVectors( pev->angles );

            if( DotProduct( vecShootDir, gpGlobals->v_forward ) > 0.707 )
            {
                m_bOnAttack = true;
                pHurt->pev->punchangle.z = -18;
                pHurt->pev->punchangle.x = 5;
                pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 300;
                if( pHurt->IsPlayer() )
                {
                    pHurt->pev->angles.x += RANDOM_FLOAT( -35, 35 );
                    pHurt->pev->angles.y += RANDOM_FLOAT( -90, 90 );
                    pHurt->pev->angles.z = 0;
                    pHurt->pev->fixangle = FIXANGLE_ABSOLUTE;
                }
                pHurt->TakeDamage( this, this, g_cfg.GetValue<float>( "ichthyosaur_shake"sv, 50, this ), DMG_SLASH );
            }
        }
        BiteSound();

        bDidAttack = true;
    }
    break;
    default:
        CFlyingMonster::HandleAnimEvent( pEvent );
        break;
    }

    if( bDidAttack )
    {
        Vector vecSrc = pev->origin + gpGlobals->v_forward * 32;
        UTIL_Bubbles( vecSrc - Vector( 8, 8, 8 ), vecSrc + Vector( 8, 8, 8 ), 16 );
    }
}

SpawnAction CIchthyosaur::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "ichthyosaur_health"sv, 400, this );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -32, -32, -32 ), Vector( 32, 32, 32 ) );

    pev->solid = SOLID_BBOX;
    pev->movetype = MOVETYPE_FLY;
    m_bloodColor = BLOOD_COLOR_GREEN;
    pev->view_ofs = Vector( 0, 0, 16 );
    m_flFieldOfView = VIEW_FIELD_WIDE;
    m_MonsterState = MONSTERSTATE_NONE;
    SetBits( pev->flags, FL_SWIM );
    SetFlyingSpeed( ICHTHYOSAUR_SPEED );
    SetFlyingMomentum( 2.5 ); // Set momentum constant

    m_afCapability = bits_CAP_RANGE_ATTACK1 | bits_CAP_SWIM;

    // Ichthyosaurs have their own use functions so never allow follow.
    m_AllowFollow = false;

    MonsterInit();

    SetTouch( &CIchthyosaur::BiteTouch );
    SetUse( &CIchthyosaur::CombatUse );

    m_idealDist = 384;
    m_flMinSpeed = 80;
    m_flMaxSpeed = 300;
    m_flMaxDist = 384;

    Vector Forward;
    AngleVectors( pev->angles, &Forward, nullptr, nullptr );
    pev->velocity = m_flightSpeed * Forward.Normalize();
    m_SaveVelocity = pev->velocity;

    return SpawnAction::Spawn;
}

void CIchthyosaur::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PRECACHE_SOUND_ARRAY( pIdleSounds );
    PRECACHE_SOUND_ARRAY( pAlertSounds );
    PRECACHE_SOUND_ARRAY( pAttackSounds );
    PRECACHE_SOUND_ARRAY( pBiteSounds );
    PRECACHE_SOUND_ARRAY( pDieSounds );
    PRECACHE_SOUND_ARRAY( pPainSounds );
}

const Schedule_t* CIchthyosaur::GetSchedule()
{
    // AILogger->debug("GetSchedule()");
    switch ( m_MonsterState )
    {
    case MONSTERSTATE_IDLE:
        m_flightSpeed = 80;
        return GetScheduleOfType( SCHED_IDLE_WALK );

    case MONSTERSTATE_ALERT:
        m_flightSpeed = 150;
        return GetScheduleOfType( SCHED_IDLE_WALK );

    case MONSTERSTATE_COMBAT:
        m_flMaxSpeed = 400;
        // eat them
        if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
        {
            return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
        }
        // chase them down and eat them
        if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
        {
            return GetScheduleOfType( SCHED_CHASE_ENEMY );
        }
        if( HasConditions( bits_COND_HEAVY_DAMAGE ) )
        {
            m_bOnAttack = true;
        }
        if( pev->health < pev->max_health - 20 )
        {
            m_bOnAttack = true;
        }

        return GetScheduleOfType( SCHED_STANDOFF );
    }

    return CFlyingMonster::GetSchedule();
}

const Schedule_t* CIchthyosaur::GetScheduleOfType( int Type )
{
    // AILogger->debug("GetScheduleOfType({}) {}", Type, m_bOnAttack);
    switch ( Type )
    {
    case SCHED_IDLE_WALK:
        return slSwimAround;
    case SCHED_STANDOFF:
        return slCircleEnemy;
    case SCHED_FAIL:
        return slSwimAgitated;
    case SCHED_DIE:
        if( pev->deadflag == DEAD_DEAD )
        {
            // Already dead, immediately switch to float.
            return slFloat;
        }

        return slTwitchDie;
    case SCHED_CHASE_ENEMY:
        AttackSound();
    }

    return CBaseMonster::GetScheduleOfType( Type );
}

void CIchthyosaur::StartTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_ICHTHYOSAUR_CIRCLE_ENEMY:
        break;
    case TASK_ICHTHYOSAUR_SWIM:
        break;
    case TASK_SMALL_FLINCH:
        if( m_idealDist > 128 )
        {
            m_flMaxDist = 512;
            m_idealDist = 512;
        }
        else
        {
            m_bOnAttack = true;
        }
        CFlyingMonster::StartTask( pTask );
        break;

    case TASK_ICHTHYOSAUR_FLOAT:
    {
        const int sequenceIndex = LookupSequence( "bellyup" );

        // Don't restart the animation if we're restoring.
        if( pev->sequence != sequenceIndex )
        {
            pev->skin = EYE_BASE;
            SetSequenceByName( "bellyup" );
        }
        break;
    }

    default:
        CFlyingMonster::StartTask( pTask );
        break;
    }
}

void CIchthyosaur::RunTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_ICHTHYOSAUR_CIRCLE_ENEMY:
        if( m_hEnemy == nullptr )
        {
            TaskComplete();
        }
        else if( FVisible( m_hEnemy ) )
        {
            Vector vecFrom = m_hEnemy->EyePosition();

            Vector vecDelta = ( pev->origin - vecFrom ).Normalize();
            Vector vecSwim = CrossProduct( vecDelta, Vector( 0, 0, 1 ) ).Normalize();

            if( DotProduct( vecSwim, m_SaveVelocity ) < 0 )
                vecSwim = vecSwim * -1.0;

            Vector vecPos = vecFrom + vecDelta * m_idealDist + vecSwim * 32;

            // AILogger->debug("vecPos {:.0f}", vecPos);

            TraceResult tr;

            UTIL_TraceHull( vecFrom, vecPos, ignore_monsters, large_hull, m_hEnemy->edict(), &tr );

            if( tr.flFraction > 0.5 )
                vecPos = tr.vecEndPos;

            m_SaveVelocity = m_SaveVelocity * 0.8 + 0.2 * ( vecPos - pev->origin ).Normalize() * m_flightSpeed;

            // AILogger->debug("m_SaveVelocity {:.2f}", m_SaveVelocity);

            if( HasConditions( bits_COND_ENEMY_FACING_ME ) && m_hEnemy->FVisible( this ) )
            {
                m_flNextAlert -= 0.1;

                if( m_idealDist < m_flMaxDist )
                {
                    m_idealDist += 4;
                }
                if( m_flightSpeed > m_flMinSpeed )
                {
                    m_flightSpeed -= 2;
                }
                else if( m_flightSpeed < m_flMinSpeed )
                {
                    m_flightSpeed += 2;
                }
                if( m_flMinSpeed < m_flMaxSpeed )
                {
                    m_flMinSpeed += 0.5;
                }
            }
            else
            {
                m_flNextAlert += 0.1;

                if( m_idealDist > 128 )
                {
                    m_idealDist -= 4;
                }
                if( m_flightSpeed < m_flMaxSpeed )
                {
                    m_flightSpeed += 4;
                }
            }
            // AILogger->debug("{:.0f}", m_idealDist);
        }
        else
        {
            m_flNextAlert = gpGlobals->time + 0.2;
        }

        if( m_flNextAlert < gpGlobals->time )
        {
            // AILogger->debug("AlertSound()");
            AlertSound();
            m_flNextAlert = gpGlobals->time + RANDOM_FLOAT( 3, 5 );
        }

        break;
    case TASK_ICHTHYOSAUR_SWIM:
        if( m_fSequenceFinished )
        {
            TaskComplete();
        }
        break;
    case TASK_DIE:
        if( m_fSequenceFinished )
        {
            pev->deadflag = DEAD_DEAD;

            TaskComplete();
        }
        break;

    case TASK_ICHTHYOSAUR_FLOAT:
        pev->angles.x = UTIL_ApproachAngle( 0, pev->angles.x, 20 );
        pev->velocity = pev->velocity * 0.8;
        if( pev->waterlevel > WaterLevel::Feet && pev->velocity.z < 64 )
        {
            pev->velocity.z += 8;
        }
        else
        {
            pev->velocity.z -= 8;
        }
        // AILogger->debug("{}", pev->velocity.z);
        break;

    default:
        CFlyingMonster::RunTask( pTask );
        break;
    }
}

float CIchthyosaur::VectorToPitch( const Vector& vec )
{
    float pitch;
    if( vec.z == 0 && vec.x == 0 )
        pitch = 0;
    else
    {
        pitch = (int)( atan2( vec.z, sqrt( vec.x * vec.x + vec.y * vec.y ) ) * 180 / PI );
        if( pitch < 0 )
            pitch += 360;
    }
    return pitch;
}

void CIchthyosaur::Move( float flInterval )
{
    CFlyingMonster::Move( flInterval );
}

float CIchthyosaur::FlPitchDiff()
{
    float flPitchDiff;
    float flCurrentPitch;

    flCurrentPitch = UTIL_AngleMod( pev->angles.z );

    if( flCurrentPitch == pev->idealpitch )
    {
        return 0;
    }

    flPitchDiff = pev->idealpitch - flCurrentPitch;

    if( pev->idealpitch > flCurrentPitch )
    {
        if( flPitchDiff >= 180 )
            flPitchDiff = flPitchDiff - 360;
    }
    else
    {
        if( flPitchDiff <= -180 )
            flPitchDiff = flPitchDiff + 360;
    }
    return flPitchDiff;
}

float CIchthyosaur::ChangePitch( int speed )
{
    if( pev->movetype == MOVETYPE_FLY )
    {
        float diff = FlPitchDiff();
        float target = 0;
        if( m_IdealActivity != GetStoppedActivity() )
        {
            if( diff < -20 )
                target = 45;
            else if( diff > 20 )
                target = -45;
        }

        if( m_flLastPitchTime == 0 )
        {
            m_flLastPitchTime = gpGlobals->time - gpGlobals->frametime;
        }

        float delta = gpGlobals->time - m_flLastPitchTime;

        m_flLastPitchTime = gpGlobals->time;

        if( delta > 0.25 )
        {
            delta = 0.25;
        }

        pev->angles.x = UTIL_Approach( target, pev->angles.x, 220.0 * delta );
    }
    return 0;
}

float CIchthyosaur::ChangeYaw( int speed )
{
    if( pev->movetype == MOVETYPE_FLY )
    {
        float diff = FlYawDiff();
        float target = 0;

        if( m_IdealActivity != GetStoppedActivity() )
        {
            if( diff < -20 )
                target = 20;
            else if( diff > 20 )
                target = -20;
        }

        if( m_flLastZYawTime == 0 )
        {
            m_flLastZYawTime = gpGlobals->time - gpGlobals->frametime;
        }

        float delta = gpGlobals->time - m_flLastZYawTime;

        m_flLastZYawTime = gpGlobals->time;

        if( delta > 0.25 )
        {
            delta = 0.25;
        }

        pev->angles.z = UTIL_Approach( target, pev->angles.z, 220.0 * delta );
    }
    return CFlyingMonster::ChangeYaw( speed );
}

Activity CIchthyosaur::GetStoppedActivity()
{
    if( pev->movetype != MOVETYPE_FLY ) // UNDONE: Ground idle here, IDLE may be something else
        return ACT_IDLE;
    return ACT_WALK;
}

void CIchthyosaur::SetActivity( Activity NewActivity )
{
    const float frame = pev->frame;

    CFlyingMonster::SetActivity( NewActivity );

    // Restore belly up state.
    if( pev->deadflag == DEAD_DEAD )
    {
        SetSequenceByName( "bellyup" );
        pev->frame = frame;
    }
}

void CIchthyosaur::MoveExecute( CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval )
{
    m_SaveVelocity = vecDir * m_flightSpeed;
}

void CIchthyosaur::MonsterThink()
{
    CFlyingMonster::MonsterThink();

    if( pev->deadflag == DEAD_NO )
    {
        if( m_MonsterState != MONSTERSTATE_SCRIPT )
        {
            Swim();

            // blink the eye
            if( m_flBlink < gpGlobals->time )
            {
                pev->skin = EYE_CLOSED;
                if( m_flBlink + 0.2 < gpGlobals->time )
                {
                    m_flBlink = gpGlobals->time + RANDOM_FLOAT( 3, 4 );
                    if( m_bOnAttack )
                        pev->skin = EYE_MAD;
                    else
                        pev->skin = EYE_BASE;
                }
            }
        }
    }
}

void CIchthyosaur::Stop()
{
    if( !m_bOnAttack )
        m_flightSpeed = 80.0;
}

void CIchthyosaur::Swim()
{
    Vector start = pev->origin;

    Vector Angles;
    Vector Forward, Right, Up;

    if( FBitSet( pev->flags, FL_ONGROUND ) )
    {
        pev->angles.x = 0;
        pev->angles.y += RANDOM_FLOAT( -45, 45 );
        ClearBits( pev->flags, FL_ONGROUND );

        Angles = Vector( -pev->angles.x, pev->angles.y, pev->angles.z );
        AngleVectors( Angles, Forward, Right, Up );

        pev->velocity = Forward * 200 + Up * 200;

        return;
    }

    if( m_bOnAttack && m_flightSpeed < m_flMaxSpeed )
    {
        m_flightSpeed += 40;
    }
    if( m_flightSpeed < 180 )
    {
        if( m_IdealActivity == ACT_RUN )
            SetActivity( ACT_WALK );
        if( m_IdealActivity == ACT_WALK )
            pev->framerate = m_flightSpeed / 150.0;
        // AILogger->debug("walk {:.2f}", pev->framerate);
    }
    else
    {
        if( m_IdealActivity == ACT_WALK )
            SetActivity( ACT_RUN );
        if( m_IdealActivity == ACT_RUN )
            pev->framerate = m_flightSpeed / 150.0;
        // AILogger->debug("run  {:.2f}", pev->framerate);
    }

    /*
        if (!m_pBeam)
        {
            m_pBeam = CBeam::BeamCreate( "sprites/laserbeam.spr", 80 );
            m_pBeam->PointEntInit( pev->origin + m_SaveVelocity, entindex( ) );
            m_pBeam->SetEndAttachment( 1 );
            m_pBeam->SetColor( 255, 180, 96 );
            m_pBeam->SetBrightness( 192 );
        }
    */
#define PROBE_LENGTH 150
    Angles = UTIL_VecToAngles( m_SaveVelocity );
    Angles.x = -Angles.x;
    AngleVectors( Angles, Forward, Right, Up );

    Vector f, u, l, r, d;
    f = DoProbe( start + PROBE_LENGTH * Forward );
    r = DoProbe( start + PROBE_LENGTH / 3 * Forward + Right );
    l = DoProbe( start + PROBE_LENGTH / 3 * Forward - Right );
    u = DoProbe( start + PROBE_LENGTH / 3 * Forward + Up );
    d = DoProbe( start + PROBE_LENGTH / 3 * Forward - Up );

    Vector SteeringVector = f + r + l + u + d;
    m_SaveVelocity = ( m_SaveVelocity + SteeringVector / 2 ).Normalize();

    Angles = Vector( -pev->angles.x, pev->angles.y, pev->angles.z );
    AngleVectors( Angles, Forward, Right, Up );
    // AILogger->debug("{} : {}", Angles.x, Forward.z);

    float flDot = DotProduct( Forward, m_SaveVelocity );
    if( flDot > 0.5 )
        pev->velocity = m_SaveVelocity = m_SaveVelocity * m_flightSpeed;
    else if( flDot > 0 )
        pev->velocity = m_SaveVelocity = m_SaveVelocity * m_flightSpeed * ( flDot + 0.5 );
    else
        pev->velocity = m_SaveVelocity = m_SaveVelocity * 80;

    // AILogger->debug("{:.0f} {:.0f}", m_flightSpeed, pev->velocity.Length());


    // AILogger->debug("Steer {}", SteeringVector);

    /*
    m_pBeam->SetStartPos( pev->origin + pev->velocity );
    m_pBeam->RelinkBeam( );
*/

    // AILogger->debug("speed {}", m_flightSpeed);

    Angles = UTIL_VecToAngles( m_SaveVelocity );

    // Smooth Pitch
    //
    if( Angles.x > 180 )
        Angles.x = Angles.x - 360;
    pev->angles.x = UTIL_Approach( Angles.x, pev->angles.x, 50 * 0.1 );
    if( pev->angles.x < -80 )
        pev->angles.x = -80;
    if( pev->angles.x > 80 )
        pev->angles.x = 80;

    // Smooth Yaw and generate Roll
    //
    float turn = 360;
    // AILogger->debug("Y {:.0f} {:.0f}", Angles.y, pev->angles.y);

    if( fabs( Angles.y - pev->angles.y ) < fabs( turn ) )
    {
        turn = Angles.y - pev->angles.y;
    }
    if( fabs( Angles.y - pev->angles.y + 360 ) < fabs( turn ) )
    {
        turn = Angles.y - pev->angles.y + 360;
    }
    if( fabs( Angles.y - pev->angles.y - 360 ) < fabs( turn ) )
    {
        turn = Angles.y - pev->angles.y - 360;
    }

    float speed = m_flightSpeed * 0.1;

    // AILogger->debug("speed {:.0f} {%f}", turn, speed);
    if( fabs( turn ) > speed )
    {
        if( turn < 0.0 )
        {
            turn = -speed;
        }
        else
        {
            turn = speed;
        }
    }
    pev->angles.y += turn;
    pev->angles.z -= turn;
    pev->angles.y = fmod( ( pev->angles.y + 360.0 ), 360.0 );

    static float yaw_adj;

    yaw_adj = yaw_adj * 0.8 + turn;

    // AILogger->debug("yaw {} : {}", turn, yaw_adj);

    SetBoneController( 0, -yaw_adj / 4.0 );

    // Roll Smoothing
    //
    turn = 360;
    if( fabs( Angles.z - pev->angles.z ) < fabs( turn ) )
    {
        turn = Angles.z - pev->angles.z;
    }
    if( fabs( Angles.z - pev->angles.z + 360 ) < fabs( turn ) )
    {
        turn = Angles.z - pev->angles.z + 360;
    }
    if( fabs( Angles.z - pev->angles.z - 360 ) < fabs( turn ) )
    {
        turn = Angles.z - pev->angles.z - 360;
    }
    speed = m_flightSpeed / 2 * 0.1;
    if( fabs( turn ) < speed )
    {
        pev->angles.z += turn;
    }
    else
    {
        if( turn < 0.0 )
        {
            pev->angles.z -= speed;
        }
        else
        {
            pev->angles.z += speed;
        }
    }
    if( pev->angles.z < -20 )
        pev->angles.z = -20;
    if( pev->angles.z > 20 )
        pev->angles.z = 20;

    AngleVectors( Vector( -Angles.x, Angles.y, Angles.z ), Forward, Right, Up );

    // UTIL_MoveToOrigin ( edict(), pev->origin + Forward * speed, speed, MOVE_STRAFE );
}

Vector CIchthyosaur::DoProbe( const Vector& Probe )
{
    Vector WallNormal = Vector( 0, 0, -1 ); // WATER normal is Straight Down for fish.
    float frac;
    bool bBumpedSomething = ProbeZ( pev->origin, Probe, &frac );

    TraceResult tr;
    TRACE_MONSTER_HULL( edict(), pev->origin, Probe, dont_ignore_monsters, edict(), &tr );
    if( 0 != tr.fAllSolid || tr.flFraction < 0.99 )
    {
        if( tr.flFraction < 0.0 )
            tr.flFraction = 0.0;
        if( tr.flFraction > 1.0 )
            tr.flFraction = 1.0;
        if( tr.flFraction < frac )
        {
            frac = tr.flFraction;
            bBumpedSomething = true;
            WallNormal = tr.vecPlaneNormal;
        }
    }

    if( bBumpedSomething && ( m_hEnemy == nullptr || tr.pHit != m_hEnemy->edict() ) )
    {
        Vector ProbeDir = Probe - pev->origin;

        Vector NormalToProbeAndWallNormal = CrossProduct( ProbeDir, WallNormal );
        Vector SteeringVector = CrossProduct( NormalToProbeAndWallNormal, ProbeDir );

        float SteeringForce = m_flightSpeed * ( 1 - frac ) * ( DotProduct( WallNormal.Normalize(), m_SaveVelocity.Normalize() ) );
        if( SteeringForce < 0.0 )
        {
            SteeringForce = -SteeringForce;
        }
        SteeringVector = SteeringForce * SteeringVector.Normalize();

        return SteeringVector;
    }
    return Vector( 0, 0, 0 );
}
