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

#define CONTROLLER_AE_HEAD_OPEN 1
#define CONTROLLER_AE_BALL_SHOOT 2
#define CONTROLLER_AE_SMALL_SHOOT 3
#define CONTROLLER_AE_POWERUP_FULL 4
#define CONTROLLER_AE_POWERUP_HALF 5

#define CONTROLLER_FLINCH_DELAY 2 //!< at most one flinch every n secs

class CController : public CSquadMonster
{
    DECLARE_CLASS( CController, CSquadMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;

    bool HasAlienGibs() override { return true; }

    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;

    void RunAI() override;
    bool CheckRangeAttack1( float flDot, float flDist ) override; //!< shoot a bigass energy ball out of their head
    bool CheckRangeAttack2( float flDot, float flDist ) override; //!< head
    bool CheckMeleeAttack1( float flDot, float flDist ) override; //!< block, throw
    const Schedule_t* GetSchedule() override;
    const Schedule_t* GetScheduleOfType( int Type ) override;
    void StartTask( const Task_t* pTask ) override;
    void RunTask( const Task_t* pTask ) override;

    void Stop() override;
    void Move( float flInterval ) override;
    int CheckLocalMove( const Vector& vecStart, const Vector& vecEnd, CBaseEntity* pTarget, float* pflDist ) override;
    void MoveExecute( CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval ) override;
    void SetActivity( Activity NewActivity ) override;
    bool ShouldAdvanceRoute( float flWaypointDist ) override;
    int LookupFloat();

    float m_flNextFlinch;

    float m_flShootTime;
    float m_flShootEnd;

    void PainSound() override;
    void AlertSound() override;
    void IdleSound() override;
    void AttackSound();
    void DeathSound() override;

    static const char* pAttackSounds[];
    static const char* pIdleSounds[];
    static const char* pAlertSounds[];
    static const char* pPainSounds[];
    static const char* pDeathSounds[];

    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void Killed( CBaseEntity* attacker, int iGib ) override;
    void GibMonster() override;

    void UpdateOnRemove() override;

    void RemoveBalls();

    CSprite* m_pBall[2];   // hand balls
    int m_iBall[2];           // how bright it should be
    float m_iBallTime[2];  // when it should be that color
    int m_iBallCurrent[2]; // current brightness

    Vector m_vecEstVelocity;

    Vector m_velocity;
    bool m_fInCombat;
};

LINK_ENTITY_TO_CLASS( monster_alien_controller, CController );

BEGIN_DATAMAP( CController )
    DEFINE_ARRAY( m_pBall, FIELD_CLASSPTR, 2 ),
    DEFINE_ARRAY( m_iBall, FIELD_INTEGER, 2 ),
    DEFINE_ARRAY( m_iBallTime, FIELD_TIME, 2 ),
    DEFINE_ARRAY( m_iBallCurrent, FIELD_INTEGER, 2 ),
    DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
END_DATAMAP();

const char* CController::pAttackSounds[] =
    {
        "controller/con_attack1.wav",
        "controller/con_attack2.wav",
        "controller/con_attack3.wav",
};

const char* CController::pIdleSounds[] =
    {
        "controller/con_idle1.wav",
        "controller/con_idle2.wav",
        "controller/con_idle3.wav",
        "controller/con_idle4.wav",
        "controller/con_idle5.wav",
};

const char* CController::pAlertSounds[] =
    {
        "controller/con_alert1.wav",
        "controller/con_alert2.wav",
        "controller/con_alert3.wav",
};

const char* CController::pPainSounds[] =
    {
        "controller/con_pain1.wav",
        "controller/con_pain2.wav",
        "controller/con_pain3.wav",
};

const char* CController::pDeathSounds[] =
    {
        "controller/con_die1.wav",
        "controller/con_die2.wav",
};

void CController::OnCreate()
{
    CSquadMonster::OnCreate();

    pev->model = MAKE_STRING( "models/controller.mdl" );

    SetClassification( "alien_military" );
}

void CController::SetYawSpeed()
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

bool CController::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    // HACK HACK -- until we fix this.
    if( IsAlive() )
        PainSound();
    return CBaseMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CController::Killed( CBaseEntity* attacker, int iGib )
{
    // shut off balls
    /*
    m_iBall[0] = 0;
    m_iBallTime[0] = gpGlobals->time + 4.0;
    m_iBall[1] = 0;
    m_iBallTime[1] = gpGlobals->time + 4.0;
    */

    // fade balls
    if( m_pBall[0] )
    {
        m_pBall[0]->SUB_StartFadeOut();
        m_pBall[0] = nullptr;
    }
    if( m_pBall[1] )
    {
        m_pBall[1]->SUB_StartFadeOut();
        m_pBall[1] = nullptr;
    }

    CSquadMonster::Killed( attacker, iGib );
}

void CController::GibMonster()
{
    RemoveBalls();
    CSquadMonster::GibMonster();
}

void CController::UpdateOnRemove()
{
    RemoveBalls();
    CSquadMonster::UpdateOnRemove();
}

void CController::RemoveBalls()
{
    if( m_pBall[0] )
    {
        UTIL_Remove( m_pBall[0] );
        m_pBall[0] = nullptr;
    }
    if( m_pBall[1] )
    {
        UTIL_Remove( m_pBall[1] );
        m_pBall[1] = nullptr;
    }
}

void CController::PainSound()
{
    if( RANDOM_LONG( 0, 5 ) < 2 )
        EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pPainSounds );
}

void CController::AlertSound()
{
    EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAlertSounds );
}

void CController::IdleSound()
{
    EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pIdleSounds );
}

void CController::AttackSound()
{
    EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAttackSounds );
}

void CController::DeathSound()
{
    EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pDeathSounds );
}

void CController::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case CONTROLLER_AE_HEAD_OPEN:
    {
        Vector vecStart, angleGun;

        GetAttachment( 0, vecStart, angleGun );

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_ELIGHT );
        WRITE_SHORT( entindex() + 0x1000 ); // entity, attachment
        WRITE_COORD( vecStart.x );          // origin
        WRITE_COORD( vecStart.y );
        WRITE_COORD( vecStart.z );
        WRITE_COORD( 1 );      // radius
        WRITE_BYTE( 255 );  // R
        WRITE_BYTE( 192 );  // G
        WRITE_BYTE( 64 );      // B
        WRITE_BYTE( 20 );      // life * 10
        WRITE_COORD( -32 ); // decay
        MESSAGE_END();

        m_iBall[0] = 192;
        m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
        m_iBall[1] = 255;
        m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
    }
    break;

    case CONTROLLER_AE_BALL_SHOOT:
    {
        Vector vecStart, angleGun;

        GetAttachment( 0, vecStart, angleGun );

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_ELIGHT );
        WRITE_SHORT( entindex() + 0x1000 ); // entity, attachment
        WRITE_COORD( 0 );                      // origin
        WRITE_COORD( 0 );
        WRITE_COORD( 0 );
        WRITE_COORD( 32 ); // radius
        WRITE_BYTE( 255 ); // R
        WRITE_BYTE( 192 ); // G
        WRITE_BYTE( 64 );     // B
        WRITE_BYTE( 10 );     // life * 10
        WRITE_COORD( 32 ); // decay
        MESSAGE_END();

        CBaseMonster* pBall = (CBaseMonster*)Create( "controller_head_ball", vecStart, pev->angles, this );

        pBall->pev->velocity = Vector( 0, 0, 32 );
        pBall->m_hEnemy = m_hEnemy;

        m_iBall[0] = 0;
        m_iBall[1] = 0;
    }
    break;

    case CONTROLLER_AE_SMALL_SHOOT:
    {
        AttackSound();
        m_flShootTime = gpGlobals->time;
        m_flShootEnd = m_flShootTime + atoi( pEvent->options ) / 15.0;
    }
    break;
    case CONTROLLER_AE_POWERUP_FULL:
    {
        m_iBall[0] = 255;
        m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
        m_iBall[1] = 255;
        m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
    }
    break;
    case CONTROLLER_AE_POWERUP_HALF:
    {
        m_iBall[0] = 192;
        m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
        m_iBall[1] = 192;
        m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
    }
    break;
    default:
        CBaseMonster::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CController::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "controller_health"sv, 100, this );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_FLY;
    pev->flags |= FL_FLY;
    m_bloodColor = BLOOD_COLOR_GREEN;
    pev->view_ofs = Vector( 0, 0, -2 );  // position of the eyes relative to monster's origin.
    m_flFieldOfView = VIEW_FIELD_FULL; // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CController::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PRECACHE_SOUND_ARRAY( pAttackSounds );
    PRECACHE_SOUND_ARRAY( pIdleSounds );
    PRECACHE_SOUND_ARRAY( pAlertSounds );
    PRECACHE_SOUND_ARRAY( pPainSounds );
    PRECACHE_SOUND_ARRAY( pDeathSounds );

    PrecacheModel( "sprites/xspark4.spr" );

    UTIL_PrecacheOther( "controller_energy_ball" );
    UTIL_PrecacheOther( "controller_head_ball" );
}

Task_t tlControllerChaseEnemy[] =
    {
        {TASK_GET_PATH_TO_ENEMY, (float)128},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},

};

Schedule_t slControllerChaseEnemy[] =
    {
        {tlControllerChaseEnemy,
            std::size( tlControllerChaseEnemy ),
            bits_COND_NEW_ENEMY |
                bits_COND_TASK_FAILED,
            0,
            "ControllerChaseEnemy"},
};

Task_t tlControllerStrafe[] =
    {
        {TASK_WAIT, (float)0.2},
        {TASK_GET_PATH_TO_ENEMY, (float)128},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_WAIT, (float)1},
};

Schedule_t slControllerStrafe[] =
    {
        {tlControllerStrafe,
            std::size( tlControllerStrafe ),
            bits_COND_NEW_ENEMY,
            0,
            "ControllerStrafe"},
};

Task_t tlControllerTakeCover[] =
    {
        {TASK_WAIT, (float)0.2},
        {TASK_FIND_COVER_FROM_ENEMY, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_WAIT, (float)1},
};

Schedule_t slControllerTakeCover[] =
    {
        {tlControllerTakeCover,
            std::size( tlControllerTakeCover ),
            bits_COND_NEW_ENEMY,
            0,
            "ControllerTakeCover"},
};

Task_t tlControllerFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slControllerFail[] =
    {
        {tlControllerFail,
            std::size( tlControllerFail ),
            0,
            0,
            "ControllerFail"},
};

BEGIN_CUSTOM_SCHEDULES( CController )
slControllerChaseEnemy,
    slControllerStrafe,
    slControllerTakeCover,
    slControllerFail
    END_CUSTOM_SCHEDULES();

void CController::StartTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_RANGE_ATTACK1:
        CSquadMonster::StartTask( pTask );
        break;
    case TASK_GET_PATH_TO_ENEMY_LKP:
    {
        if( BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, pTask->flData, ( m_vecEnemyLKP - pev->origin ).Length() + 1024 ) )
        {
            TaskComplete();
        }
        else
        {
            // no way to get there =(
            AILogger->debug( "GetPathToEnemyLKP failed!!" );
            TaskFail();
        }
        break;
    }
    case TASK_GET_PATH_TO_ENEMY:
    {
        CBaseEntity* pEnemy = m_hEnemy;

        if( pEnemy == nullptr )
        {
            TaskFail();
            return;
        }

        if( BuildNearestRoute( pEnemy->pev->origin, pEnemy->pev->view_ofs, pTask->flData, ( pEnemy->pev->origin - pev->origin ).Length() + 1024 ) )
        {
            TaskComplete();
        }
        else
        {
            // no way to get there =(
            AILogger->debug( "GetPathToEnemy failed!!" );
            TaskFail();
        }
        break;
    }
    default:
        CSquadMonster::StartTask( pTask );
        break;
    }
}

Vector Intersect( Vector vecSrc, Vector vecDst, Vector vecMove, float flSpeed )
{
    Vector vecTo = vecDst - vecSrc;

    float a = DotProduct( vecMove, vecMove ) - flSpeed * flSpeed;
    float b = 0 * DotProduct( vecTo, vecMove ); // why does this work?
    float c = DotProduct( vecTo, vecTo );

    float t;
    if( a == 0 )
    {
        t = c / ( flSpeed * flSpeed );
    }
    else
    {
        t = b * b - 4 * a * c;
        t = sqrt( t ) / ( 2.0 * a );
        float t1 = -b + t;
        float t2 = -b - t;

        if( t1 < 0 || t2 < t1 )
            t = t2;
        else
            t = t1;
    }

    // AILogger->debug("Intersect {}", t);

    if( t < 0.1 )
        t = 0.1;
    if( t > 10.0 )
        t = 10.0;

    Vector vecHit = vecTo + vecMove * t;
    return vecHit.Normalize() * flSpeed;
}

int CController::LookupFloat()
{
    if( m_velocity.Length() < 32.0 )
    {
        return LookupSequence( "up" );
    }

    UTIL_MakeAimVectors( pev->angles );
    float x = DotProduct( gpGlobals->v_forward, m_velocity );
    float y = DotProduct( gpGlobals->v_right, m_velocity );
    float z = DotProduct( gpGlobals->v_up, m_velocity );

    if( fabs( x ) > fabs( y ) && fabs( x ) > fabs( z ) )
    {
        if( x > 0 )
            return LookupSequence( "forward" );
        else
            return LookupSequence( "backward" );
    }
    else if( fabs( y ) > fabs( z ) )
    {
        if( y > 0 )
            return LookupSequence( "right" );
        else
            return LookupSequence( "left" );
    }
    else
    {
        if( z > 0 )
            return LookupSequence( "up" );
        else
            return LookupSequence( "down" );
    }
}

void CController::RunTask( const Task_t* pTask )
{

    if( m_flShootEnd > gpGlobals->time )
    {
        Vector vecHand, vecAngle;

        GetAttachment( 2, vecHand, vecAngle );

        while( m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time )
        {
            Vector vecSrc = vecHand + pev->velocity * ( m_flShootTime - gpGlobals->time );
            Vector vecDir;

            if( m_hEnemy != nullptr )
            {
                if( HasConditions( bits_COND_SEE_ENEMY ) )
                {
                    m_vecEstVelocity = m_vecEstVelocity * 0.5 + m_hEnemy->pev->velocity * 0.5;
                }
                else
                {
                    m_vecEstVelocity = m_vecEstVelocity * 0.8;
                }
                vecDir = Intersect( vecSrc, m_hEnemy->BodyTarget( pev->origin ), m_vecEstVelocity, g_cfg.GetValue<float>( "controller_speedball"sv, 1000, this ) );
                float delta = 0.03490; // +-2 degree
                vecDir = vecDir + Vector( RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ) ) * g_cfg.GetValue<float>( "controller_speedball"sv, 1000, this );

                vecSrc = vecSrc + vecDir * ( gpGlobals->time - m_flShootTime );
                CBaseMonster* pBall = (CBaseMonster*)Create( "controller_energy_ball", vecSrc, pev->angles, this );
                pBall->pev->velocity = vecDir;
            }
            m_flShootTime += 0.2;
        }

        if( m_flShootTime > m_flShootEnd )
        {
            m_iBall[0] = 64;
            m_iBallTime[0] = m_flShootEnd;
            m_iBall[1] = 64;
            m_iBallTime[1] = m_flShootEnd;
            m_fInCombat = false;
        }
    }

    switch ( pTask->iTask )
    {
    case TASK_WAIT_FOR_MOVEMENT:
    case TASK_WAIT:
    case TASK_WAIT_FACE_ENEMY:
    case TASK_WAIT_PVS:
        if( m_hEnemy != 0 )
        {
            MakeIdealYaw( m_vecEnemyLKP );
            ChangeYaw( pev->yaw_speed );
        }

        if( m_fSequenceFinished )
        {
            m_fInCombat = false;
        }

        CSquadMonster::RunTask( pTask );

        if( !m_fInCombat )
        {
            if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
            {
                pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
                pev->frame = 0;
                ResetSequenceInfo();
                m_fInCombat = true;
            }
            else if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) )
            {
                pev->sequence = LookupActivity( ACT_RANGE_ATTACK2 );
                pev->frame = 0;
                ResetSequenceInfo();
                m_fInCombat = true;
            }
            else
            {
                int iFloat = LookupFloat();
                if( m_fSequenceFinished || iFloat != pev->sequence )
                {
                    pev->sequence = iFloat;
                    pev->frame = 0;
                    ResetSequenceInfo();
                }
            }
        }
        break;
    default:
        CSquadMonster::RunTask( pTask );
        break;
    }
}

const Schedule_t* CController::GetSchedule()
{
    switch ( m_MonsterState )
    {
    case MONSTERSTATE_IDLE:
        break;

    case MONSTERSTATE_ALERT:
        break;

    case MONSTERSTATE_COMBAT:
    {
        // dead enemy
        if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
        {
            // m_iFrustration++;
        }
        if( HasConditions( bits_COND_HEAVY_DAMAGE ) )
        {
            // m_iFrustration++;
        }
    }
    break;
    }

    return CSquadMonster::GetSchedule();
}

const Schedule_t* CController::GetScheduleOfType( int Type )
{
    // AILogger->debug( "{}", m_iFrustration);
    switch ( Type )
    {
    case SCHED_CHASE_ENEMY:
        return slControllerChaseEnemy;
    case SCHED_RANGE_ATTACK1:
        return slControllerStrafe;
    case SCHED_RANGE_ATTACK2:
    case SCHED_MELEE_ATTACK1:
    case SCHED_MELEE_ATTACK2:
    case SCHED_TAKE_COVER_FROM_ENEMY:
        return slControllerTakeCover;
    case SCHED_FAIL:
        return slControllerFail;
    }

    return CBaseMonster::GetScheduleOfType( Type );
}

bool CController::CheckRangeAttack1( float flDot, float flDist )
{
    if( flDot > 0.5 && flDist > 256 && flDist <= 2048 )
    {
        return true;
    }
    return false;
}

bool CController::CheckRangeAttack2( float flDot, float flDist )
{
    if( flDot > 0.5 && flDist > 64 && flDist <= 2048 )
    {
        return true;
    }
    return false;
}

bool CController::CheckMeleeAttack1( float flDot, float flDist )
{
    return false;
}

void CController::SetActivity( Activity NewActivity )
{
    CBaseMonster::SetActivity( NewActivity );

    switch ( m_Activity )
    {
    case ACT_WALK:
        m_flGroundSpeed = 100;
        break;
    default:
        m_flGroundSpeed = 100;
        break;
    }
}

void CController::RunAI()
{
    CBaseMonster::RunAI();
    Vector vecStart, angleGun;

    if( HasMemory( bits_MEMORY_KILLED ) )
        return;

    for( int i = 0; i < 2; i++ )
    {
        if( m_pBall[i] == nullptr )
        {
            m_pBall[i] = CSprite::SpriteCreate( "sprites/xspark4.spr", pev->origin, true );
            m_pBall[i]->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
            m_pBall[i]->SetAttachment( edict(), ( i + 3 ) );
            m_pBall[i]->SetScale( 1.0 );
        }

        float t = m_iBallTime[i] - gpGlobals->time;
        if( t > 0.1 )
            t = 0.1 / t;
        else
            t = 1.0;

        m_iBallCurrent[i] += ( m_iBall[i] - m_iBallCurrent[i] ) * t;

        m_pBall[i]->SetBrightness( m_iBallCurrent[i] );

        GetAttachment( i + 2, vecStart, angleGun );
        m_pBall[i]->SetOrigin( vecStart );

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_ELIGHT );
        WRITE_SHORT( entindex() + 0x1000 * ( i + 3 ) ); // entity, attachment
        WRITE_COORD( vecStart.x );                    // origin
        WRITE_COORD( vecStart.y );
        WRITE_COORD( vecStart.z );
        WRITE_COORD( m_iBallCurrent[i] / 8 ); // radius
        WRITE_BYTE( 255 );                    // R
        WRITE_BYTE( 192 );                    // G
        WRITE_BYTE( 64 );                        // B
        WRITE_BYTE( 5 );                        // life * 10
        WRITE_COORD( 0 );                        // decay
        MESSAGE_END();
    }
}

void CController::Stop()
{
    m_IdealActivity = GetStoppedActivity();
}

#define DIST_TO_CHECK 200
void CController::Move( float flInterval )
{
    float flWaypointDist;
    float flCheckDist;
    float flDist; // how far the lookahead check got before hitting an object.
    float flMoveDist;
    Vector vecDir;
    Vector vecApex;
    CBaseEntity* pTargetEnt;

    // Don't move if no valid route
    if( FRouteClear() )
    {
        AILogger->debug( "Tried to move with no route!" );
        TaskFail();
        return;
    }

    if( m_flMoveWaitFinished > gpGlobals->time )
        return;

        // Debug, test movement code
#if 0
//    if ( CVAR_GET_FLOAT("stopmove" ) != 0 )
    {
        if( m_movementGoal == MOVEGOAL_ENEMY )
            RouteSimplify( m_hEnemy );
        else
            RouteSimplify( m_hTargetEnt );
        FRefreshRoute();
        return;
    }
#else
// Debug, draw the route
//    DrawRoute(this, m_Route, m_iRouteIndex, 0, 0, 255);
#endif

    // if the monster is moving directly towards an entity (enemy for instance), we'll set this pointer
    // to that entity for the CheckLocalMove and Triangulate functions.
    pTargetEnt = nullptr;

    if( m_flGroundSpeed == 0 )
    {
        m_flGroundSpeed = 100;
        // TaskFail( );
        // return;
    }

    flMoveDist = m_flGroundSpeed * flInterval;

    do
    {
        // local move to waypoint.
        vecDir = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Normalize();
        flWaypointDist = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Length();

        // MakeIdealYaw ( m_Route[ m_iRouteIndex ].vecLocation );
        // ChangeYaw ( pev->yaw_speed );

        // if the waypoint is closer than CheckDist, CheckDist is the dist to waypoint
        if( flWaypointDist < DIST_TO_CHECK )
        {
            flCheckDist = flWaypointDist;
        }
        else
        {
            flCheckDist = DIST_TO_CHECK;
        }

        if( ( m_Route[m_iRouteIndex].iType & ( ~bits_MF_NOT_TO_MASK ) ) == bits_MF_TO_ENEMY )
        {
            // only on a PURE move to enemy ( i.e., ONLY MF_TO_ENEMY set, not MF_TO_ENEMY and DETOUR )
            pTargetEnt = m_hEnemy;
        }
        else if( ( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK ) == bits_MF_TO_TARGETENT )
        {
            pTargetEnt = m_hTargetEnt;
        }

        // !!!BUGBUG - CheckDist should be derived from ground speed.
        // If this fails, it should be because of some dynamic entity blocking this guy.
        // We've already checked this path, so we should wait and time out if the entity doesn't move
        flDist = 0;
        if( CheckLocalMove( pev->origin, pev->origin + vecDir * flCheckDist, pTargetEnt, &flDist ) != LOCALMOVE_VALID )
        {
            CBaseEntity* pBlocker;

            // Can't move, stop
            Stop();
            // Blocking entity is in global trace_ent
            pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
            if( pBlocker )
            {
                DispatchBlocked( edict(), pBlocker->edict() );
            }
            if( pBlocker && m_moveWaitTime > 0 && pBlocker->IsMoving() && !pBlocker->IsPlayer() && ( gpGlobals->time - m_flMoveWaitFinished ) > 3.0 )
            {
                // Can we still move toward our target?
                if( flDist < m_flGroundSpeed )
                {
                    // Wait for a second
                    m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
                    //                AILogger->debug("Move {}!!!", STRING(pBlocker->pev->classname));
                    return;
                }
            }
            else
            {
                // try to triangulate around whatever is in the way.
                if( FTriangulate( pev->origin, m_Route[m_iRouteIndex].vecLocation, flDist, pTargetEnt, &vecApex ) )
                {
                    InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
                    RouteSimplify( pTargetEnt );
                }
                else
                {
                    AILogger->debug( "Couldn't Triangulate" );
                    Stop();
                    if( m_moveWaitTime > 0 )
                    {
                        FRefreshRoute();
                        m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime * 0.5;
                    }
                    else
                    {
                        TaskFail();
                        AILogger->debug( "Failed to move!" );
                        // AILogger->debug("{}, {}, {}", pev->origin.z, (pev->origin + (vecDir * flCheckDist)).z, m_Route[m_iRouteIndex].vecLocation.z);
                    }
                    return;
                }
            }
        }

        // UNDONE: this is a hack to quit moving farther than it has looked ahead.
        if( flCheckDist < flMoveDist )
        {
            MoveExecute( pTargetEnt, vecDir, flCheckDist / m_flGroundSpeed );

            // AILogger->debug("{:.02f}", flInterval);
            AdvanceRoute( flWaypointDist );
            flMoveDist -= flCheckDist;
        }
        else
        {
            MoveExecute( pTargetEnt, vecDir, flMoveDist / m_flGroundSpeed );

            if( ShouldAdvanceRoute( flWaypointDist - flMoveDist ) )
            {
                AdvanceRoute( flWaypointDist );
            }
            flMoveDist = 0;
        }

        if( MovementIsComplete() )
        {
            Stop();
            RouteClear();
        }
    } while( flMoveDist > 0 && flCheckDist > 0 );

    // cut corner?
    if( flWaypointDist < 128 )
    {
        if( m_movementGoal == MOVEGOAL_ENEMY )
            RouteSimplify( m_hEnemy );
        else
            RouteSimplify( m_hTargetEnt );
        FRefreshRoute();

        if( m_flGroundSpeed > 100 )
            m_flGroundSpeed -= 40;
    }
    else
    {
        if( m_flGroundSpeed < 400 )
            m_flGroundSpeed += 10;
    }
}

bool CController::ShouldAdvanceRoute( float flWaypointDist )
{
    if( flWaypointDist <= 32 )
    {
        return true;
    }

    return false;
}

int CController::CheckLocalMove( const Vector& vecStart, const Vector& vecEnd, CBaseEntity* pTarget, float* pflDist )
{
    TraceResult tr;

    UTIL_TraceHull( vecStart + Vector( 0, 0, 32 ), vecEnd + Vector( 0, 0, 32 ), dont_ignore_monsters, large_hull, edict(), &tr );

    // AILogger->debug("{:.0f} : {:.0f}", vecStart, vecEnd);

    if( pflDist )
    {
        *pflDist = ( ( tr.vecEndPos - Vector( 0, 0, 32 ) ) - vecStart ).Length(); // get the distance.
    }

    // AILogger->debug("check {} {} {}", tr.fStartSolid, tr.fAllSolid, tr.flFraction);
    if( 0 != tr.fStartSolid || tr.flFraction < 1.0 )
    {
        if( pTarget && pTarget->edict() == gpGlobals->trace_ent )
            return LOCALMOVE_VALID;
        return LOCALMOVE_INVALID;
    }

    return LOCALMOVE_VALID;
}

void CController::MoveExecute( CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval )
{
    if( m_IdealActivity != m_movementActivity )
        m_IdealActivity = m_movementActivity;

    // AILogger->debug("move {:.4f} : {}", vecDir, flInterval);

    // float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
    // UTIL_MoveToOrigin ( edict(), m_Route[ m_iRouteIndex ].vecLocation, flTotal, MOVE_STRAFE );

    m_velocity = m_velocity * 0.8 + m_flGroundSpeed * vecDir * 0.2;

    UTIL_MoveToOrigin( edict(), pev->origin + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );
}

/**
 *    @brief Controller bouncy ball attack
 */
class CControllerHeadBall : public CBaseMonster
{
    DECLARE_CLASS( CControllerHeadBall, CBaseMonster );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;
    void HuntThink();
    void DieThink();
    void BounceTouch( CBaseEntity* pOther );
    void MovetoTarget( Vector vecTarget );
    void Crawl();

private:
    int m_iTrail;
    int m_flNextAttack;
    Vector m_vecIdeal;
    EHANDLE m_hOwner;
};

BEGIN_DATAMAP( CControllerHeadBall )
    DEFINE_FUNCTION( HuntThink ),
    DEFINE_FUNCTION( DieThink ),
    DEFINE_FUNCTION( BounceTouch ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( controller_head_ball, CControllerHeadBall );

SpawnAction CControllerHeadBall::Spawn()
{
    Precache();
    // motor
    pev->movetype = MOVETYPE_FLY;
    pev->solid = SOLID_BBOX;

    SetModel( "sprites/xspark4.spr" );
    pev->rendermode = kRenderTransAdd;
    pev->rendercolor.x = 255;
    pev->rendercolor.y = 255;
    pev->rendercolor.z = 255;
    pev->renderamt = 255;
    pev->scale = 2.0;

    SetSize( Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
    SetOrigin( pev->origin );

    SetThink( &CControllerHeadBall::HuntThink );
    SetTouch( &CControllerHeadBall::BounceTouch );

    m_vecIdeal = Vector( 0, 0, 0 );

    pev->nextthink = gpGlobals->time + 0.1;

    m_hOwner = Instance( pev->owner );
    pev->dmgtime = gpGlobals->time;

    return SpawnAction::Spawn;
}

void CControllerHeadBall::Precache()
{
    PrecacheModel( "sprites/xspark1.spr" );
    PrecacheSound( "debris/zap4.wav" );
    PrecacheSound( "weapons/electro4.wav" );
}

void CControllerHeadBall::HuntThink()
{
    pev->nextthink = gpGlobals->time + 0.1;

    pev->renderamt -= 5;

    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    WRITE_BYTE( TE_ELIGHT );
    WRITE_SHORT( entindex() );    // entity, attachment
    WRITE_COORD( pev->origin.x ); // origin
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z );
    WRITE_COORD( pev->renderamt / 16 ); // radius
    WRITE_BYTE( 255 );                  // R
    WRITE_BYTE( 255 );                  // G
    WRITE_BYTE( 255 );                  // B
    WRITE_BYTE( 2 );                      // life * 10
    WRITE_COORD( 0 );                      // decay
    MESSAGE_END();

    // check world boundaries
    if( gpGlobals->time - pev->dmgtime > 5 || pev->renderamt < 64 || m_hEnemy == nullptr || m_hOwner == nullptr || !IsInWorld() )
    {
        SetTouch( nullptr );
        UTIL_Remove( this );
        return;
    }

    MovetoTarget( m_hEnemy->Center() );

    if( ( m_hEnemy->Center() - pev->origin ).Length() < 64 )
    {
        TraceResult tr;

        UTIL_TraceLine( pev->origin, m_hEnemy->Center(), dont_ignore_monsters, edict(), &tr );

        CBaseEntity* pEntity = CBaseEntity::Instance( tr.pHit );
        if( pEntity != nullptr && 0 != pEntity->pev->takedamage )
        {
            ClearMultiDamage();
            pEntity->TraceAttack( m_hOwner, g_cfg.GetValue<float>( "controller_dmgzap"sv, 35, this ), pev->velocity, &tr, DMG_SHOCK );
            ApplyMultiDamage( this, m_hOwner );
        }

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_BEAMENTPOINT );
        WRITE_SHORT( entindex() );
        WRITE_COORD( tr.vecEndPos.x );
        WRITE_COORD( tr.vecEndPos.y );
        WRITE_COORD( tr.vecEndPos.z );
        WRITE_SHORT( g_sModelIndexLaser );
        WRITE_BYTE( 0 );     // frame start
        WRITE_BYTE( 10 );     // framerate
        WRITE_BYTE( 3 );     // life
        WRITE_BYTE( 20 );     // width
        WRITE_BYTE( 0 );     // noise
        WRITE_BYTE( 255 ); // r, g, b
        WRITE_BYTE( 255 ); // r, g, b
        WRITE_BYTE( 255 ); // r, g, b
        WRITE_BYTE( 255 ); // brightness
        WRITE_BYTE( 10 );     // speed
        MESSAGE_END();

        EmitAmbientSound( tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );

        m_flNextAttack = gpGlobals->time + 3.0;

        SetThink( &CControllerHeadBall::DieThink );
        pev->nextthink = gpGlobals->time + 0.3;
    }

    // Crawl( );
}

void CControllerHeadBall::DieThink()
{
    UTIL_Remove( this );
}

void CControllerHeadBall::MovetoTarget( Vector vecTarget )
{
    // accelerate
    float flSpeed = m_vecIdeal.Length();
    if( flSpeed == 0 )
    {
        m_vecIdeal = pev->velocity;
        flSpeed = m_vecIdeal.Length();
    }

    if( flSpeed > 400 )
    {
        m_vecIdeal = m_vecIdeal.Normalize() * 400;
    }
    m_vecIdeal = m_vecIdeal + ( vecTarget - pev->origin ).Normalize() * 100;
    pev->velocity = m_vecIdeal;
}

void CControllerHeadBall::Crawl()
{

    Vector vecAim = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize();
    Vector vecPnt = pev->origin + pev->velocity * 0.3 + vecAim * 64;

    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    WRITE_BYTE( TE_BEAMENTPOINT );
    WRITE_SHORT( entindex() );
    WRITE_COORD( vecPnt.x );
    WRITE_COORD( vecPnt.y );
    WRITE_COORD( vecPnt.z );
    WRITE_SHORT( g_sModelIndexLaser );
    WRITE_BYTE( 0 );     // frame start
    WRITE_BYTE( 10 );     // framerate
    WRITE_BYTE( 3 );     // life
    WRITE_BYTE( 20 );     // width
    WRITE_BYTE( 0 );     // noise
    WRITE_BYTE( 255 ); // r, g, b
    WRITE_BYTE( 255 ); // r, g, b
    WRITE_BYTE( 255 ); // r, g, b
    WRITE_BYTE( 255 ); // brightness
    WRITE_BYTE( 10 );     // speed
    MESSAGE_END();
}

void CControllerHeadBall::BounceTouch( CBaseEntity* pOther )
{
    Vector vecDir = m_vecIdeal.Normalize();

    TraceResult tr = UTIL_GetGlobalTrace();

    float n = -DotProduct( tr.vecPlaneNormal, vecDir );

    vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

    m_vecIdeal = vecDir * m_vecIdeal.Length();
}

class CControllerZapBall : public CBaseMonster
{
    DECLARE_CLASS( CControllerZapBall, CBaseMonster );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;
    void AnimateThink();
    void ExplodeTouch( CBaseEntity* pOther );

private:
    EHANDLE m_hOwner;
};

BEGIN_DATAMAP( CControllerZapBall )
    DEFINE_FUNCTION( AnimateThink ),
    DEFINE_FUNCTION( ExplodeTouch ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( controller_energy_ball, CControllerZapBall );

SpawnAction CControllerZapBall::Spawn()
{
    Precache();
    // motor
    pev->movetype = MOVETYPE_FLY;
    pev->solid = SOLID_BBOX;

    SetModel( "sprites/xspark4.spr" );
    pev->rendermode = kRenderTransAdd;
    pev->rendercolor.x = 255;
    pev->rendercolor.y = 255;
    pev->rendercolor.z = 255;
    pev->renderamt = 255;
    pev->scale = 0.5;

    SetSize( Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
    SetOrigin( pev->origin );

    SetThink( &CControllerZapBall::AnimateThink );
    SetTouch( &CControllerZapBall::ExplodeTouch );

    m_hOwner = Instance( pev->owner );
    pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
    pev->nextthink = gpGlobals->time + 0.1;

    return SpawnAction::Spawn;
}

void CControllerZapBall::Precache()
{
    PrecacheModel( "sprites/xspark4.spr" );
    // PrecacheSound("debris/zap4.wav");
    // PrecacheSound("weapons/electro4.wav");
}

void CControllerZapBall::AnimateThink()
{
    pev->nextthink = gpGlobals->time + 0.1;

    pev->frame = ( (int)pev->frame + 1 ) % 11;

    if( gpGlobals->time - pev->dmgtime > 5 || pev->velocity.Length() < 10 )
    {
        SetTouch( nullptr );
        UTIL_Remove( this );
    }
}

void CControllerZapBall::ExplodeTouch( CBaseEntity* pOther )
{
    if( 0 != pOther->pev->takedamage )
    {
        TraceResult tr = UTIL_GetGlobalTrace();

        CBaseEntity* owner = m_hOwner;
        if( !owner )
        {
            owner = this;
        }

        ClearMultiDamage();
        pOther->TraceAttack( owner, g_cfg.GetValue<float>( "controller_dmgball"sv, 5, this ), pev->velocity.Normalize(), &tr, DMG_ENERGYBEAM );
        ApplyMultiDamage( owner, owner );

        EmitAmbientSound( tr.vecEndPos, "weapons/electro4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 90, 99 ) );
    }

    UTIL_Remove( this );
}
