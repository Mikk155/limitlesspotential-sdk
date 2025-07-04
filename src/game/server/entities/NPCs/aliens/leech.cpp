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

// Animation events
#define LEECH_AE_ATTACK 1
#define LEECH_AE_FLOP 2

// Movement constants
#define LEECH_ACCELERATE 10
#define LEECH_CHECK_DIST 45
#define LEECH_SWIM_SPEED 50
#define LEECH_SWIM_ACCEL 80
#define LEECH_SWIM_DECEL 10
#define LEECH_TURN_RATE 90
#define LEECH_SIZEX 10
#define LEECH_FRAMETIME 0.1

#define DEBUG_BEAMS 0

/**
 *    @brief basic little swimming monster
 */
class CLeech : public CBaseMonster
{
    DECLARE_CLASS( CLeech, CBaseMonster );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;

    void SwimThink();
    void DeadThink();
    void Touch( CBaseEntity* pOther ) override
    {
        if( pOther->IsPlayer() )
        {
            // If the client is pushing me, give me some base velocity
            if( gpGlobals->trace_ent && gpGlobals->trace_ent == edict() )
            {
                pev->basevelocity = pOther->pev->velocity;
                pev->flags |= FL_BASEVELOCITY;
            }
        }
    }

    void SetObjectCollisionBox() override
    {
        pev->absmin = pev->origin + Vector( -8, -8, 0 );
        pev->absmax = pev->origin + Vector( 8, 8, 2 );
    }

    void AttackSound();
    void AlertSound() override;
    void UpdateMotion();

    /**
     *    @brief returns normalized distance to obstacle
     */
    float ObstacleDistance( CBaseEntity* pTarget );

    void MakeVectors();
    void RecalculateWaterlevel();
    void SwitchLeechState();

    // Base entity functions
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    int BloodColor() override { return DONT_BLEED; }
    void Killed( CBaseEntity* attacker, int iGib ) override;
    void Activate() override;
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    Relationship IRelationship( CBaseEntity* pTarget ) override;

    bool HasAlienGibs() override { return true; }

    static const char* pAttackSounds[];
    static const char* pAlertSounds[];

private:
    // UNDONE: Remove unused boid vars, do group behavior
    float m_flTurning;     // is this boid turning?
    bool m_fPathBlocked; // true if there is an obstacle ahead
    float m_flAccelerate;
    float m_obstacle;
    float m_top;
    float m_bottom;
    float m_height;
    float m_waterTime;
    float m_sideTime; // Timer to randomly check clearance on sides
    float m_zTime;
    float m_stateTime;
    float m_attackSoundTime;

#if DEBUG_BEAMS
    CBeam* m_pb;
    CBeam* m_pt;
#endif
};

LINK_ENTITY_TO_CLASS( monster_leech, CLeech );

BEGIN_DATAMAP( CLeech )
    DEFINE_FIELD( m_flTurning, FIELD_FLOAT ),
    DEFINE_FIELD( m_fPathBlocked, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flAccelerate, FIELD_FLOAT ),
    DEFINE_FIELD( m_obstacle, FIELD_FLOAT ),
    DEFINE_FIELD( m_top, FIELD_FLOAT ),
    DEFINE_FIELD( m_bottom, FIELD_FLOAT ),
    DEFINE_FIELD( m_height, FIELD_FLOAT ),
    DEFINE_FIELD( m_waterTime, FIELD_TIME ),
    DEFINE_FIELD( m_sideTime, FIELD_TIME ),
    DEFINE_FIELD( m_zTime, FIELD_TIME ),
    DEFINE_FIELD( m_stateTime, FIELD_TIME ),
    DEFINE_FIELD( m_attackSoundTime, FIELD_TIME ),
    DEFINE_FUNCTION( SwimThink ),
    DEFINE_FUNCTION( DeadThink ),
END_DATAMAP();

const char* CLeech::pAttackSounds[] =
    {
        "leech/leech_bite1.wav",
        "leech/leech_bite2.wav",
        "leech/leech_bite3.wav",
};

const char* CLeech::pAlertSounds[] =
    {
        "leech/leech_alert1.wav",
        "leech/leech_alert2.wav",
};

void CLeech::OnCreate()
{
    CBaseMonster::OnCreate();

    pev->model = MAKE_STRING( "models/leech.mdl" );

    SetClassification( "insect" );

    // Just for fun
    // pev->model = MAKE_STRING("models/icky.mdl");
}

SpawnAction CLeech::Spawn()
{
    Precache();
    SetModel( STRING( pev->model ) );

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "leech_health"sv, 2, this );

    // Don't push the minz down too much or the water check will fail because this entity is really point-sized
    SetSize( Vector( -1, -1, 0 ), Vector( 1, 1, 2 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_FLY;
    SetBits( pev->flags, FL_SWIM );

    m_flFieldOfView = -0.5; // 180 degree FOV
    m_flDistLook = 750;
    m_AllowFollow = false;
    MonsterInit();
    SetThink( &CLeech::SwimThink );
    SetUse( nullptr );
    SetTouch( nullptr );
    pev->view_ofs = g_vecZero;

    m_flTurning = 0;
    m_fPathBlocked = false;
    SetActivity( ACT_SWIM );
    SetState( MONSTERSTATE_IDLE );
    m_stateTime = gpGlobals->time + RANDOM_FLOAT( 1, 5 );

    return SpawnAction::Spawn;
}

void CLeech::Activate()
{
    RecalculateWaterlevel();
}

void CLeech::RecalculateWaterlevel()
{
    // Calculate boundaries
    Vector vecTest = pev->origin - Vector( 0, 0, 400 );

    TraceResult tr;

    UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );
    if( tr.flFraction != 1.0 )
        m_bottom = tr.vecEndPos.z + 1;
    else
        m_bottom = vecTest.z;

    m_top = UTIL_WaterLevel( pev->origin, pev->origin.z, pev->origin.z + 400 ) - 1;

    // Chop off 20% of the outside range
    float newBottom = m_bottom * 0.8 + m_top * 0.2;
    m_top = m_bottom * 0.2 + m_top * 0.8;
    m_bottom = newBottom;
    m_height = RANDOM_FLOAT( m_bottom, m_top );
    m_waterTime = gpGlobals->time + RANDOM_FLOAT( 5, 7 );
}

void CLeech::SwitchLeechState()
{
    m_stateTime = gpGlobals->time + RANDOM_FLOAT( 3, 6 );
    if( m_MonsterState == MONSTERSTATE_COMBAT )
    {
        m_hEnemy = nullptr;
        SetState( MONSTERSTATE_IDLE );
        // We may be up against the player, so redo the side checks
        m_sideTime = 0;
    }
    else
    {
        Look( m_flDistLook );
        CBaseEntity* pEnemy = BestVisibleEnemy();
        if( pEnemy && pEnemy->pev->waterlevel != WaterLevel::Dry )
        {
            m_hEnemy = pEnemy;
            SetState( MONSTERSTATE_COMBAT );
            m_stateTime = gpGlobals->time + RANDOM_FLOAT( 18, 25 );
            AlertSound();
        }
    }
}

Relationship CLeech::IRelationship( CBaseEntity* pTarget )
{
    if( pTarget->IsPlayer() )
        return Relationship::Dislike;
    return CBaseMonster::IRelationship( pTarget );
}

void CLeech::AttackSound()
{
    if( gpGlobals->time > m_attackSoundTime )
    {
        EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_NORM );
        m_attackSoundTime = gpGlobals->time + 0.5;
    }
}

void CLeech::AlertSound()
{
    EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0, ATTN_NORM * 0.5 );
}

void CLeech::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PRECACHE_SOUND_ARRAY( pAttackSounds );
    PRECACHE_SOUND_ARRAY( pAlertSounds );
}

bool CLeech::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    pev->velocity = g_vecZero;

    // Nudge the leech away from the damage
    if( inflictor )
    {
        pev->velocity = ( pev->origin - inflictor->pev->origin ).Normalize() * 25;
    }

    return CBaseMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CLeech::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case LEECH_AE_ATTACK:
        AttackSound();
        CBaseEntity* pEnemy;

        pEnemy = m_hEnemy;
        if( pEnemy != nullptr )
        {
            Vector dir, face;

            AngleVectors( pev->angles, &face, nullptr, nullptr );
            face.z = 0;
            dir = ( pEnemy->pev->origin - pev->origin );
            dir.z = 0;
            dir = dir.Normalize();
            face = face.Normalize();


            if( DotProduct( dir, face ) > 0.9 ) // Only take damage if the leech is facing the prey
                pEnemy->TakeDamage( this, this, g_cfg.GetValue<float>( "leech_dmg_bite"sv, 2, this ), DMG_SLASH );
        }
        m_stateTime -= 2;
        break;

    case LEECH_AE_FLOP:
        // Play flop sound
        break;

    default:
        CBaseMonster::HandleAnimEvent( pEvent );
        break;
    }
}

void CLeech::MakeVectors()
{
    Vector tmp = pev->angles;
    tmp.x = -tmp.x;
    UTIL_MakeVectors( tmp );
}

float CLeech::ObstacleDistance( CBaseEntity* pTarget )
{
    TraceResult tr;
    Vector vecTest;

    // use VELOCITY, not angles, not all boids point the direction they are flying
    // Vector vecDir = UTIL_VecToAngles( pev->velocity );
    MakeVectors();

    // check for obstacle ahead
    vecTest = pev->origin + gpGlobals->v_forward * LEECH_CHECK_DIST;
    UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );

    if( 0 != tr.fStartSolid )
    {
        pev->speed = -LEECH_SWIM_SPEED * 0.5;
        //        AILogger->debug("Stuck from ({}) to ({})", pev->oldorigin, pev->origin);
        //        SetOrigin(pev->oldorigin);
    }

    if( tr.flFraction != 1.0 )
    {
        if( ( pTarget == nullptr || tr.pHit != pTarget->edict() ) )
        {
            return tr.flFraction;
        }
        else
        {
            if( fabs( m_height - pev->origin.z ) > 10 )
                return tr.flFraction;
        }
    }

    if( m_sideTime < gpGlobals->time )
    {
        // extra wide checks
        vecTest = pev->origin + gpGlobals->v_right * LEECH_SIZEX * 2 + gpGlobals->v_forward * LEECH_CHECK_DIST;
        UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );
        if( tr.flFraction != 1.0 )
            return tr.flFraction;

        vecTest = pev->origin - gpGlobals->v_right * LEECH_SIZEX * 2 + gpGlobals->v_forward * LEECH_CHECK_DIST;
        UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );
        if( tr.flFraction != 1.0 )
            return tr.flFraction;

        // Didn't hit either side, so stop testing for another 0.5 - 1 seconds
        m_sideTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 1 );
    }
    return 1.0;
}

void CLeech::DeadThink()
{
    if( m_fSequenceFinished )
    {
        if( m_Activity == ACT_DIEFORWARD )
        {
            SetThink( nullptr );
            StopAnimation();
            return;
        }
        else if( ( pev->flags & FL_ONGROUND ) != 0 )
        {
            pev->solid = SOLID_NOT;
            SetActivity( ACT_DIEFORWARD );
        }
    }
    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.1;

    // Apply damage velocity, but keep out of the walls
    if( pev->velocity.x != 0 || pev->velocity.y != 0 )
    {
        TraceResult tr;

        // Look 0.5 seconds ahead
        UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 0.5, missile, edict(), &tr );
        if( tr.flFraction != 1.0 )
        {
            pev->velocity.x = 0;
            pev->velocity.y = 0;
        }
    }
}

void CLeech::UpdateMotion()
{
    float flapspeed = ( pev->speed - m_flAccelerate ) / LEECH_ACCELERATE;
    m_flAccelerate = m_flAccelerate * 0.8 + pev->speed * 0.2;

    if( flapspeed < 0 )
        flapspeed = -flapspeed;
    flapspeed += 1.0;
    if( flapspeed < 0.5 )
        flapspeed = 0.5;
    if( flapspeed > 1.9 )
        flapspeed = 1.9;

    pev->framerate = flapspeed;

    if( !m_fPathBlocked )
        pev->avelocity.y = pev->ideal_yaw;
    else
        pev->avelocity.y = pev->ideal_yaw * m_obstacle;

    if( pev->avelocity.y > 150 )
        m_IdealActivity = ACT_TURN_LEFT;
    else if( pev->avelocity.y < -150 )
        m_IdealActivity = ACT_TURN_RIGHT;
    else
        m_IdealActivity = ACT_SWIM;

    // lean
    float targetPitch, delta;
    delta = m_height - pev->origin.z;

    if( delta < -10 )
        targetPitch = -30;
    else if( delta > 10 )
        targetPitch = 30;
    else
        targetPitch = 0;

    pev->angles.x = UTIL_Approach( targetPitch, pev->angles.x, 60 * LEECH_FRAMETIME );

    // bank
    pev->avelocity.z = -( pev->angles.z + ( pev->avelocity.y * 0.25 ) );

    if( m_MonsterState == MONSTERSTATE_COMBAT && HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
        m_IdealActivity = ACT_MELEE_ATTACK1;

    // Out of water check
    if( WaterLevel::Dry == pev->waterlevel )
    {
        pev->movetype = MOVETYPE_TOSS;
        m_IdealActivity = ACT_TWITCH;
        pev->velocity = g_vecZero;

        // Animation will intersect the floor if either of these is non-zero
        pev->angles.z = 0;
        pev->angles.x = 0;

        if( pev->framerate < 1.0 )
            pev->framerate = 1.0;
    }
    else if( pev->movetype == MOVETYPE_TOSS )
    {
        pev->movetype = MOVETYPE_FLY;
        pev->flags &= ~FL_ONGROUND;
        RecalculateWaterlevel();
        m_waterTime = gpGlobals->time + 2; // Recalc again soon, water may be rising
    }

    if( m_Activity != m_IdealActivity )
    {
        SetActivity( m_IdealActivity );
    }
    float flInterval = StudioFrameAdvance();
    DispatchAnimEvents( flInterval );

#if DEBUG_BEAMS
    if( !m_pb )
        m_pb = CBeam::BeamCreate( "sprites/laserbeam.spr", 5 );
    if( !m_pt )
        m_pt = CBeam::BeamCreate( "sprites/laserbeam.spr", 5 );
    m_pb->PointsInit( pev->origin, pev->origin + gpGlobals->v_forward * LEECH_CHECK_DIST );
    m_pt->PointsInit( pev->origin, pev->origin - gpGlobals->v_right * ( pev->avelocity.y * 0.25 ) );
    if( m_fPathBlocked )
    {
        float color = m_obstacle * 30;
        if( m_obstacle == 1.0 )
            color = 0;
        if( color > 255 )
            color = 255;
        m_pb->SetColor( 255, (int)color, (int)color );
    }
    else
        m_pb->SetColor( 255, 255, 0 );
    m_pt->SetColor( 0, 0, 255 );
#endif
}

void CLeech::SwimThink()
{
    TraceResult tr;
    float flLeftSide;
    float flRightSide;
    float targetSpeed;
    float targetYaw = 0;
    CBaseEntity* pTarget;

    if( !UTIL_FindClientInPVS( this ) )
    {
        pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 1, 1.5 );
        pev->velocity = g_vecZero;
        return;
    }
    else
        pev->nextthink = gpGlobals->time + 0.1;

    UpdateShockEffect();

    targetSpeed = LEECH_SWIM_SPEED;

    if( m_waterTime < gpGlobals->time )
        RecalculateWaterlevel();

    if( m_stateTime < gpGlobals->time )
        SwitchLeechState();

    ClearConditions( bits_COND_CAN_MELEE_ATTACK1 );
    switch ( m_MonsterState )
    {
    case MONSTERSTATE_COMBAT:
        pTarget = m_hEnemy;
        if( !pTarget )
            SwitchLeechState();
        else
        {
            // Chase the enemy's eyes
            m_height = pTarget->pev->origin.z + pTarget->pev->view_ofs.z - 5;
            // Clip to viable water area
            if( m_height < m_bottom )
                m_height = m_bottom;
            else if( m_height > m_top )
                m_height = m_top;
            Vector location = pTarget->pev->origin - pev->origin;
            location.z += ( pTarget->pev->view_ofs.z );
            if( location.Length() < 40 )
                SetConditions( bits_COND_CAN_MELEE_ATTACK1 );
            // Turn towards target ent
            targetYaw = VectorToYaw( location );

            targetYaw = UTIL_AngleDiff( targetYaw, UTIL_AngleMod( pev->angles.y ) );

            if( targetYaw < ( -LEECH_TURN_RATE * 0.75 ) )
                targetYaw = ( -LEECH_TURN_RATE * 0.75 );
            else if( targetYaw > ( LEECH_TURN_RATE * 0.75 ) )
                targetYaw = ( LEECH_TURN_RATE * 0.75 );
            else
                targetSpeed *= 2;
        }

        break;

    default:
        if( m_zTime < gpGlobals->time )
        {
            float newHeight = RANDOM_FLOAT( m_bottom, m_top );
            m_height = 0.5 * m_height + 0.5 * newHeight;
            m_zTime = gpGlobals->time + RANDOM_FLOAT( 1, 4 );
        }
        if( RANDOM_LONG( 0, 100 ) < 10 )
            targetYaw = RANDOM_LONG( -30, 30 );
        pTarget = nullptr;
        // oldorigin test
        if( ( pev->origin - pev->oldorigin ).Length() < 1 )
        {
            // If leech didn't move, there must be something blocking it, so try to turn
            m_sideTime = 0;
        }

        break;
    }

    m_obstacle = ObstacleDistance( pTarget );
    pev->oldorigin = pev->origin;
    if( m_obstacle < 0.1 )
        m_obstacle = 0.1;

    // is the way ahead clear?
    if( m_obstacle == 1.0 )
    {
        // if the leech is turning, stop the trend.
        if( m_flTurning != 0 )
        {
            m_flTurning = 0;
        }

        m_fPathBlocked = false;
        pev->speed = UTIL_Approach( targetSpeed, pev->speed, LEECH_SWIM_ACCEL * LEECH_FRAMETIME );
        pev->velocity = gpGlobals->v_forward * pev->speed;
    }
    else
    {
        m_obstacle = 1.0 / m_obstacle;
        // IF we get this far in the function, the leader's path is blocked!
        m_fPathBlocked = true;

        if( m_flTurning == 0 ) // something in the way and leech is not already turning to avoid
        {
            Vector vecTest;
            // measure clearance on left and right to pick the best dir to turn
            vecTest = pev->origin + ( gpGlobals->v_right * LEECH_SIZEX ) + ( gpGlobals->v_forward * LEECH_CHECK_DIST );
            UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );
            flRightSide = tr.flFraction;

            vecTest = pev->origin + ( gpGlobals->v_right * -LEECH_SIZEX ) + ( gpGlobals->v_forward * LEECH_CHECK_DIST );
            UTIL_TraceLine( pev->origin, vecTest, missile, edict(), &tr );
            flLeftSide = tr.flFraction;

            // turn left, right or random depending on clearance ratio
            float delta = ( flRightSide - flLeftSide );
            if( delta > 0.1 || ( delta > -0.1 && RANDOM_LONG( 0, 100 ) < 50 ) )
                m_flTurning = -LEECH_TURN_RATE;
            else
                m_flTurning = LEECH_TURN_RATE;
        }
        pev->speed = UTIL_Approach( -( LEECH_SWIM_SPEED * 0.5 ), pev->speed, LEECH_SWIM_DECEL * LEECH_FRAMETIME * m_obstacle );
        pev->velocity = gpGlobals->v_forward * pev->speed;
    }
    pev->ideal_yaw = m_flTurning + targetYaw;
    UpdateMotion();
}

void CLeech::Killed( CBaseEntity* attacker, int iGib )
{
    // AILogger->debug("Leech: killed");
    //  tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
    MaybeNotifyOwnerOfDeath();

    // When we hit the ground, play the "death_end" activity
    if( WaterLevel::Dry != pev->waterlevel )
    {
        pev->angles.z = 0;
        pev->angles.x = 0;
        pev->origin.z += 1;
        pev->avelocity = g_vecZero;
        if( RANDOM_LONG( 0, 99 ) < 70 )
            pev->avelocity.y = RANDOM_LONG( -720, 720 );

        pev->gravity = 0.02;
        ClearBits( pev->flags, FL_ONGROUND );
        SetActivity( ACT_DIESIMPLE );
    }
    else
        SetActivity( ACT_DIEFORWARD );

    ClearShockEffect();

    pev->movetype = MOVETYPE_TOSS;
    pev->takedamage = DAMAGE_NO;
    SetThink( &CLeech::DeadThink );
}
