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

LINK_ENTITY_TO_CLASS( monster_scientist, CScientist );

BEGIN_DATAMAP( CScientist )
    DEFINE_FIELD( m_painTime, FIELD_TIME ),
    DEFINE_FIELD( m_healTime, FIELD_TIME ),
    DEFINE_FIELD( m_fearTime, FIELD_TIME ),
END_DATAMAP();

Task_t tlScientistFollow[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_CANT_FOLLOW}, // If you fail, bail out of follow
        {TASK_MOVE_TO_TARGET_RANGE, (float)128},            // Move within 128 of target ent (client)
                                                            //    { TASK_SET_SCHEDULE,        (float)SCHED_TARGET_FACE },
};

Schedule_t slScientistFollow[] =
    {
        {tlScientistFollow,
            std::size( tlScientistFollow ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,
            bits_SOUND_COMBAT |
                bits_SOUND_DANGER,
            "ScientistFollow"},
};

Task_t tlFollowScared[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE}, // If you fail, follow normally
        {TASK_MOVE_TO_TARGET_RANGE_SCARED, (float)128},         // Move within 128 of target ent (client)
                                                             //    { TASK_SET_SCHEDULE,        (float)SCHED_TARGET_FACE_SCARED },
};

Schedule_t slFollowScared[] =
    {
        {tlFollowScared,
            std::size( tlFollowScared ),
            bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            bits_SOUND_DANGER,
            "FollowScared"},
};

Task_t tlFaceTargetScared[] =
    {
        {TASK_FACE_TARGET, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_CROUCHIDLE},
        {TASK_SET_SCHEDULE, (float)SCHED_TARGET_CHASE_SCARED},
};

Schedule_t slFaceTargetScared[] =
    {
        {tlFaceTargetScared,
            std::size( tlFaceTargetScared ),
            bits_COND_HEAR_SOUND |
                bits_COND_NEW_ENEMY,
            bits_SOUND_DANGER,
            "FaceTargetScared"},
};

Task_t tlStopFollowing[] =
    {
        {TASK_CANT_FOLLOW, (float)0},
};

Schedule_t slStopFollowing[] =
    {
        {tlStopFollowing,
            std::size( tlStopFollowing ),
            0,
            0,
            "StopFollowing"},
};

Task_t tlHeal[] =
    {
        {TASK_MOVE_TO_TARGET_RANGE, (float)50},                 // Move within 60 of target ent (client)
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE}, // If you fail, catch up with that guy! (change this to put syringe away and then chase)
        {TASK_FACE_IDEAL, (float)0},
        {TASK_SAY_HEAL, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_TARGET, (float)ACT_ARM},     // Whip out the needle
        {TASK_HEAL, (float)0},                                 // Put it in the player
        {TASK_PLAY_SEQUENCE_FACE_TARGET, (float)ACT_DISARM}, // Put away the needle
};

Schedule_t slHeal[] =
    {
        {tlHeal,
            std::size( tlHeal ),
            0, // Don't interrupt or he'll end up running around with a needle all the time
            0,
            "Heal"},
};

Task_t tlScientistFaceTarget[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_TARGET, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_SET_SCHEDULE, (float)SCHED_TARGET_CHASE},
};

Schedule_t slScientistFaceTarget[] =
    {
        {tlScientistFaceTarget,
            std::size( tlScientistFaceTarget ),
            bits_COND_CLIENT_PUSH |
                bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND,
            bits_SOUND_COMBAT |
                bits_SOUND_DANGER,
            "ScientistFaceTarget"},
};

Task_t tlSciPanic[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_SCREAM, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_EXCITED}, // This is really fear-stricken excitement
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slSciPanic[] =
    {
        {tlSciPanic,
            std::size( tlSciPanic ),
            0,
            0,
            "SciPanic"},
};

Task_t tlIdleSciStand[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},            // repick IDLESTAND every two seconds.
        {TASK_TLK_HEADRESET, (float)0}, // reset head position
};

Schedule_t slIdleSciStand[] =
    {
        {tlIdleSciStand,
            std::size( tlIdleSciStand ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_SMELL |
                bits_COND_CLIENT_PUSH |
                bits_COND_PROVOKED,

            bits_SOUND_COMBAT | // sound flags
                                // bits_SOUND_PLAYER        |
                                // bits_SOUND_WORLD        |
                bits_SOUND_DANGER |
                bits_SOUND_MEAT | // scents
                bits_SOUND_CARCASS |
                bits_SOUND_GARBAGE,
            "IdleSciStand"

        },
};

Task_t tlScientistCover[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
        {TASK_STOP_MOVING, (float)0},
        {TASK_FIND_COVER_FROM_ENEMY, (float)0},
        {TASK_RUN_PATH_SCARED, (float)0},
        {TASK_TURN_LEFT, (float)179},
        {TASK_SET_SCHEDULE, (float)SCHED_HIDE},
};

Schedule_t slScientistCover[] =
    {
        {tlScientistCover,
            std::size( tlScientistCover ),
            bits_COND_NEW_ENEMY,
            0,
            "ScientistCover"},
};

Task_t tlScientistHide[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_CROUCH},
        {TASK_SET_ACTIVITY, (float)ACT_CROUCHIDLE}, // FIXME: This looks lame
        {TASK_WAIT_RANDOM, (float)10.0},
};

Schedule_t slScientistHide[] =
    {
        {tlScientistHide,
            std::size( tlScientistHide ),
            bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND |
                bits_COND_SEE_ENEMY |
                bits_COND_SEE_HATE |
                bits_COND_SEE_FEAR |
                bits_COND_SEE_DISLIKE,
            bits_SOUND_DANGER,
            "ScientistHide"},
};

Task_t tlScientistStartle[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_PANIC}, // If you fail, just panic!
        {TASK_RANDOM_SCREAM, (float)0.3},              // Scream 30% of the time
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
        {TASK_RANDOM_SCREAM, (float)0.1}, // Scream again 10% of the time
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCHIDLE},
        {TASK_WAIT_RANDOM, (float)1.0},
};

Schedule_t slScientistStartle[] =
    {
        {tlScientistStartle,
            std::size( tlScientistStartle ),
            bits_COND_NEW_ENEMY |
                bits_COND_SEE_ENEMY |
                bits_COND_SEE_HATE |
                bits_COND_SEE_FEAR |
                bits_COND_SEE_DISLIKE,
            0,
            "ScientistStartle"},
};

// Marphy Fact Files Fix - Restore fear display animation
Task_t tlFear[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_SAY_FEAR, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_FEAR_DISPLAY},
};

Schedule_t slFear[] =
    {
        {tlFear,
            std::size( tlFear ),
            bits_COND_NEW_ENEMY,
            0,
            "Fear"},
};

BEGIN_CUSTOM_SCHEDULES( CScientist )
slScientistFollow,
    slScientistFaceTarget,
    slIdleSciStand,
    slFear,
    slScientistCover,
    slScientistHide,
    slScientistStartle,
    slHeal,
    slStopFollowing,
    slSciPanic,
    slFollowScared,
    slFaceTargetScared
    END_CUSTOM_SCHEDULES();

void CScientist::OnCreate()
{
    CTalkMonster::OnCreate();

    pev->model = MAKE_STRING( "models/scientist.mdl" );

    m_iszUse = MAKE_STRING( "SC_OK" );
    m_iszUnUse = MAKE_STRING( "SC_WAIT" );

    SetClassification( "human_passive" );
}

void CScientist::DeclineFollowing()
{
    Talk( 10 );
    m_hTalkTarget = m_hEnemy;
    PlaySentence( "SC_POK", 2, VOL_NORM, ATTN_NORM );
}

void CScientist::Scream()
{
    // Marphy Fact Files Fix - This speech check always fails during combat, so removing
    // if ( FOkToSpeak() )
    //{
    Talk( 10 );
    m_hTalkTarget = m_hEnemy;
    PlaySentence( "SC_SCREAM", RANDOM_FLOAT( 3, 6 ), VOL_NORM, ATTN_NORM );
    //}
}

Activity CScientist::GetStoppedActivity()
{
    if( m_hEnemy != nullptr )
        return ACT_EXCITED;
    return CTalkMonster::GetStoppedActivity();
}

void CScientist::StartTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_SAY_HEAL:
        //        if ( FOkToSpeak() )
        Talk( 2 );
        m_hTalkTarget = m_hTargetEnt;
        PlaySentence( "SC_HEAL", 2, VOL_NORM, ATTN_IDLE );

        TaskComplete();
        break;

    case TASK_SCREAM:
        Scream();
        TaskComplete();
        break;

    case TASK_RANDOM_SCREAM:
        if( RANDOM_FLOAT( 0, 1 ) < pTask->flData )
            Scream();
        TaskComplete();
        break;

    case TASK_SAY_FEAR:
        // Marphy Fact FIles Fix - This speech check always fails during combat, so removing
        // if ( FOkToSpeak() )
        if( m_hEnemy )
        {
            Talk( 2 );
            m_hTalkTarget = m_hEnemy;
            if( m_hEnemy->IsPlayer() )
                PlaySentence( "SC_PLFEAR", 5, VOL_NORM, ATTN_NORM );
            else
                PlaySentence( "SC_FEAR", 5, VOL_NORM, ATTN_NORM );
        }
        TaskComplete();
        break;

    case TASK_HEAL:
        m_IdealActivity = ACT_MELEE_ATTACK1;
        break;

    case TASK_RUN_PATH_SCARED:
        m_movementActivity = ACT_RUN_SCARED;
        break;

    case TASK_MOVE_TO_TARGET_RANGE_SCARED:
    {
        if( ( m_hTargetEnt->pev->origin - pev->origin ).Length() < 1 )
            TaskComplete();
        else
        {
            m_vecMoveGoal = m_hTargetEnt->pev->origin;
            if( !MoveToTarget( ACT_WALK_SCARED, 0.5 ) )
                TaskFail();
        }
    }
    break;

    default:
        CTalkMonster::StartTask( pTask );
        break;
    }
}

void CScientist::RunTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_RUN_PATH_SCARED:
        if( MovementIsComplete() )
            TaskComplete();

        // Marphy Fact Files Fix - Reducing scream (which didn't work before) chance significantly
        // if ( RANDOM_LONG(0,31) < 8 )
        if( RANDOM_LONG( 0, 63 ) < 1 )
            Scream();
        break;

    case TASK_MOVE_TO_TARGET_RANGE_SCARED:
    {
        // Marphy Fact Files Fix - Removing redundant scream
        // if ( RANDOM_LONG(0,63)< 8 )
        // Scream();

        if( m_hEnemy == nullptr )
        {
            TaskFail();
        }
        else
        {
            float distance;

            distance = ( m_vecMoveGoal - pev->origin ).Length2D();
            // Re-evaluate when you think your finished, or the target has moved too far
            if( ( distance < pTask->flData ) || ( m_vecMoveGoal - m_hTargetEnt->pev->origin ).Length() > pTask->flData * 0.5 )
            {
                m_vecMoveGoal = m_hTargetEnt->pev->origin;
                distance = ( m_vecMoveGoal - pev->origin ).Length2D();
                FRefreshRoute();
            }

            // Set the appropriate activity based on an overlapping range
            // overlap the range to prevent oscillation
            if( distance < pTask->flData )
            {
                TaskComplete();
                RouteClear(); // Stop moving
            }
            else if( distance < 190 && m_movementActivity != ACT_WALK_SCARED )
                m_movementActivity = ACT_WALK_SCARED;
            else if( distance >= 270 && m_movementActivity != ACT_RUN_SCARED )
                m_movementActivity = ACT_RUN_SCARED;
        }
    }
    break;

    case TASK_HEAL:
        if( m_fSequenceFinished )
        {
            TaskComplete();
        }
        else
        {
            if( TargetDistance() > 90 )
                TaskComplete();
            pev->ideal_yaw = VectorToYaw( m_hTargetEnt->pev->origin - pev->origin );
            ChangeYaw( pev->yaw_speed );
        }
        break;
    default:
        CTalkMonster::RunTask( pTask );
        break;
    }
}

void CScientist::SetYawSpeed()
{
    int ys;

    ys = 90;

    switch ( m_Activity )
    {
    case ACT_IDLE:
        ys = 120;
        break;
    case ACT_WALK:
        ys = 180;
        break;
    case ACT_RUN:
        ys = 150;
        break;
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        ys = 120;
        break;
    }

    pev->yaw_speed = ys;
}

void CScientist::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case SCIENTIST_AE_HEAL: // Heal my target (if within range)
        Heal();
        break;
    case SCIENTIST_AE_NEEDLEON:
    {
        SetBodygroup( ScientistBodygroup::Needle, ScientistNeedle::Drawn );
    }
    break;
    case SCIENTIST_AE_NEEDLEOFF:
    {
        SetBodygroup( ScientistBodygroup::Needle, ScientistNeedle::Blank );
    }
    break;

    default:
        CTalkMonster::HandleAnimEvent( pEvent );
    }
}

bool CScientist::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "item" ) )
    {
        m_Item = static_cast<ScientistItem>( atoi( pkvd->szValue ) );
        return true;
    }

    return CTalkMonster::KeyValue( pkvd );
}

SpawnAction CScientist::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "scientist_health"sv, 20, this );

    SetModel( STRING( pev->model ) );

    // -1 chooses a random head
    if( pev->body == -1 )
    {
        // Erase previous value because SetBodygroup won't.
        pev->body = 0;
        // pick a head, any head
        SetBodygroup( ScientistBodygroup::Head, RANDOM_LONG( 0, GetBodygroupSubmodelCount( ScientistBodygroup::Head ) - 1 ) );
    }

    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_RED;
    pev->view_ofs = Vector( 0, 0, 50 );  // position of the eyes relative to monster's origin.
    m_flFieldOfView = VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so scientists will notice player and say hello
    m_MonsterState = MONSTERSTATE_NONE;

    //    m_flDistTooFar        = 256.0;

    m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;

    // White hands
    pev->skin = 0;

    // Luther is black, make his hands black
    if( GetBodygroup( ScientistBodygroup::Head ) == HEAD_LUTHER )
        pev->skin = 1;

    // get voice for head
    switch ( GetBodygroup( ScientistBodygroup::Head ) % GetBodygroupSubmodelCount( ScientistBodygroup::Head ) )
    {
    default:
    case HEAD_GLASSES:
        m_voicePitch = 105;
        break; // glasses
    case HEAD_EINSTEIN:
        m_voicePitch = 100;
        break; // einstein
    case HEAD_LUTHER:
        m_voicePitch = 95;
        break; // luther
    case HEAD_SLICK:
        m_voicePitch = 100;
        break; // slick
    }

    SetBodygroup( ScientistBodygroup::Item, static_cast<int>( m_Item ) );

    MonsterInit();

    return SpawnAction::Spawn;
}

void CScientist::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "scientist/sci_pain1.wav" );
    PrecacheSound( "scientist/sci_pain2.wav" );
    PrecacheSound( "scientist/sci_pain3.wav" );
    PrecacheSound( "scientist/sci_pain4.wav" );
    PrecacheSound( "scientist/sci_pain5.wav" );

    // every new scientist must call this, otherwise
    // when a level is loaded, nobody will talk (time is reset to 0)
    TalkInit();

    CTalkMonster::Precache();
}

void CScientist::TalkInit()
{
    CTalkMonster::TalkInit();

    // scientists speach group names (group names are in sentences.txt)

    m_szGrp[TLK_ANSWER] = "SC_ANSWER";
    m_szGrp[TLK_QUESTION] = "SC_QUESTION";
    m_szGrp[TLK_IDLE] = "SC_IDLE";
    m_szGrp[TLK_STARE] = "SC_STARE";
    m_szGrp[TLK_STOP] = "SC_STOP";
    m_szGrp[TLK_NOSHOOT] = "SC_SCARED";
    m_szGrp[TLK_HELLO] = "SC_HELLO";

    m_szGrp[TLK_PLHURT1] = "!SC_CUREA";
    m_szGrp[TLK_PLHURT2] = "!SC_CUREB";
    m_szGrp[TLK_PLHURT3] = "!SC_CUREC";

    m_szGrp[TLK_PHELLO] = "SC_PHELLO";
    m_szGrp[TLK_PIDLE] = "SC_PIDLE";
    m_szGrp[TLK_PQUESTION] = "SC_PQUEST";
    m_szGrp[TLK_SMELL] = "SC_SMELL";

    m_szGrp[TLK_WOUND] = "SC_WOUND";
    m_szGrp[TLK_MORTAL] = "SC_MORTAL";
}

bool CScientist::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{

    if( inflictor && ( inflictor->pev->flags & FL_CLIENT ) != 0 )
    {
        Remember( bits_MEMORY_PROVOKED );
        StopFollowing( true );
    }

    // make sure friends talk about it if player hurts scientist...
    return CTalkMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

// Marphy Fact Files Fix - Restore scientist's sense of smell
int CScientist::ISoundMask()
{
    return bits_SOUND_WORLD |
           bits_SOUND_COMBAT |
           bits_SOUND_CARCASS |
           bits_SOUND_MEAT |
           bits_SOUND_GARBAGE |
           bits_SOUND_DANGER |
           bits_SOUND_PLAYER;
}

void CScientist::PainSound()
{
    if( gpGlobals->time < m_painTime )
        return;

    m_painTime = gpGlobals->time + RANDOM_FLOAT( 0.5, 0.75 );

    switch ( RANDOM_LONG( 0, 4 ) )
    {
    case 0:
        EmitSoundDyn( CHAN_VOICE, "scientist/sci_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
        break;
    case 1:
        EmitSoundDyn( CHAN_VOICE, "scientist/sci_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
        break;
    case 2:
        EmitSoundDyn( CHAN_VOICE, "scientist/sci_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
        break;
    case 3:
        EmitSoundDyn( CHAN_VOICE, "scientist/sci_pain4.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
        break;
    case 4:
        EmitSoundDyn( CHAN_VOICE, "scientist/sci_pain5.wav", 1, ATTN_NORM, 0, GetVoicePitch() );
        break;
    }
}

void CScientist::DeathSound()
{
    PainSound();
}

void CScientist::SetActivity( Activity newActivity )
{
    int iSequence;

    iSequence = LookupActivity( newActivity );

    // Set to the desired anim, or default anim if the desired is not present
    if( iSequence == ACTIVITY_NOT_AVAILABLE )
        newActivity = ACT_IDLE;
    CTalkMonster::SetActivity( newActivity );
}

const Schedule_t* CScientist::GetScheduleOfType( int Type )
{
    const Schedule_t* psched;

    switch ( Type )
    {
        // Hook these to make a looping schedule
    case SCHED_TARGET_FACE:
        // call base class default so that scientist will talk
        // when 'used'
        psched = CTalkMonster::GetScheduleOfType( Type );

        if( psched == slIdleStand )
            return slScientistFaceTarget; // override this for different target face behavior
        else
            return psched;

    case SCHED_TARGET_CHASE:
        return slScientistFollow;

    case SCHED_CANT_FOLLOW:
        return slStopFollowing;

    case SCHED_PANIC:
        return slSciPanic;

    case SCHED_TARGET_CHASE_SCARED:
        return slFollowScared;

    case SCHED_TARGET_FACE_SCARED:
        return slFaceTargetScared;

    case SCHED_IDLE_STAND:
        // call base class default so that scientist will talk
        // when standing during idle
        psched = CTalkMonster::GetScheduleOfType( Type );

        if( psched == slIdleStand )
            return slIdleSciStand;
        else
            return psched;

    case SCHED_HIDE:
        return slScientistHide;

    case SCHED_STARTLE:
        return slScientistStartle;

    case SCHED_FEAR:
        return slFear;
    }

    return CTalkMonster::GetScheduleOfType( Type );
}

const Schedule_t* CScientist::GetSchedule()
{
    // so we don't keep calling through the EHANDLE stuff
    CBaseEntity* pEnemy = m_hEnemy;

    if( HasConditions( bits_COND_HEAR_SOUND ) )
    {
        CSound* pSound;
        pSound = PBestSound();

        ASSERT( pSound != nullptr );
        if( pSound && ( pSound->m_iType & bits_SOUND_DANGER ) != 0 )
            return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
    }

    switch ( m_MonsterState )
    {
    case MONSTERSTATE_ALERT:
    case MONSTERSTATE_IDLE:
        if( pEnemy )
        {
            if( HasConditions( bits_COND_SEE_ENEMY ) )
                m_fearTime = gpGlobals->time;
            else if( DisregardEnemy( pEnemy ) ) // After 15 seconds of being hidden, return to alert
            {
                m_hEnemy = nullptr;
                pEnemy = nullptr;

                // Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
                m_fearTime = gpGlobals->time;
            }
        }

        if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
        {
            // flinch if hurt
            return GetScheduleOfType( SCHED_SMALL_FLINCH );
        }

        // Cower when you hear something scary
        if( HasConditions( bits_COND_HEAR_SOUND ) )
        {
            CSound* pSound;
            pSound = PBestSound();

            ASSERT( pSound != nullptr );
            if( pSound )
            {
                if( ( pSound->m_iType & ( bits_SOUND_DANGER | bits_SOUND_COMBAT ) ) != 0 )
                {
                    if( gpGlobals->time - m_fearTime > 3 ) // Only cower every 3 seconds or so
                    {
                        m_fearTime = gpGlobals->time;             // Update last fear
                        return GetScheduleOfType( SCHED_STARTLE ); // This will just duck for a second
                    }
                }
            }
        }

        // Behavior for following the player
        if( IsFollowing() )
        {
            if( !m_hTargetEnt->IsAlive() )
            {
                // UNDONE: Comment about the recently dead player here?
                StopFollowing( false );
                break;
            }

            Relationship relationship = Relationship::None;

            // Nothing scary, just me and the player
            if( pEnemy != nullptr )
                relationship = IRelationship( pEnemy );

            // UNDONE: Model fear properly, fix R_FR and add multiple levels of fear
            if( relationship != Relationship::Dislike && relationship != Relationship::Hate )
            {
                // If I'm already close enough to my target
                if( TargetDistance() <= 128 )
                {
                    if( CanHeal() ) // Heal opportunistically
                        return slHeal;
                    if( HasConditions( bits_COND_CLIENT_PUSH ) ) // Player wants me to move
                        return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
                }
                return GetScheduleOfType( SCHED_TARGET_FACE ); // Just face and follow.
            }
            else // UNDONE: When afraid, scientist won't move out of your way.  Keep This?  If not, write move away scared
            {
                if( HasConditions( bits_COND_NEW_ENEMY ) )                // I just saw something new and scary, react
                    return GetScheduleOfType( SCHED_FEAR );            // React to something scary
                return GetScheduleOfType( SCHED_TARGET_FACE_SCARED ); // face and follow, but I'm scared!
            }
        }

        if( HasConditions( bits_COND_CLIENT_PUSH ) ) // Player wants me to move
            return GetScheduleOfType( SCHED_MOVE_AWAY );

        // try to say something about smells
        TrySmellTalk();
        break;
    case MONSTERSTATE_COMBAT:
        if( HasConditions( bits_COND_NEW_ENEMY ) )
            return slFear; // Point and scream!

        if( HasConditions( bits_COND_SEE_ENEMY ) )
        {
            // Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
            m_fearTime = gpGlobals->time;
            return slScientistCover; // Take Cover
        }

        if( HasConditions( bits_COND_HEAR_SOUND ) )
            return slTakeCoverFromBestSound; // Cower and panic from the scary sound!

        // Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
        if( pEnemy )
        {
            if( HasConditions( bits_COND_SEE_ENEMY ) )
                m_fearTime = gpGlobals->time;
            else if( DisregardEnemy( pEnemy ) ) // After 15 seconds of being hidden, return to alert
            {
                m_hEnemy = NULL;
                pEnemy = NULL;

                m_fearTime = gpGlobals->time;

                if( IsFollowing() )
                {
                    return slScientistStartle;
                }

                return slScientistHide; // Hide after disregard
            }
        }

        return slScientistCover; // Run & Cower
        break;
    }

    return CTalkMonster::GetSchedule();
}

MONSTERSTATE CScientist::GetIdealState()
{
    switch ( m_MonsterState )
    {
    case MONSTERSTATE_ALERT:
    case MONSTERSTATE_IDLE:
        if( HasConditions( bits_COND_NEW_ENEMY ) )
        {
            if( IsFollowing() )
            {
                Relationship relationship = IRelationship( m_hEnemy );
                if( relationship != Relationship::Fear ||
                    ( relationship != Relationship::Hate &&
                        !HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) ) )
                {
                    // Don't go to combat if you're following the player
                    m_IdealMonsterState = MONSTERSTATE_ALERT;
                    return m_IdealMonsterState;
                }
                StopFollowing( true );
            }
        }
        else if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
        {
            // Stop following if you take damage
            if( IsFollowing() )
                StopFollowing( true );
        }
        break;

    case MONSTERSTATE_COMBAT:
    {
        CBaseEntity* pEnemy = m_hEnemy;
        if( pEnemy != nullptr )
        {
            if( DisregardEnemy( pEnemy ) ) // After 15 seconds of being hidden, return to alert
            {
                // Strip enemy when going to alert
                m_IdealMonsterState = MONSTERSTATE_ALERT;
                m_hEnemy = nullptr;

                // Marphy Fact Files Fix - Fix scientists not disregarding enemy after hiding
                m_fearTime = gpGlobals->time;
                return m_IdealMonsterState;
            }
            // Follow if only scared a little
            if( m_hTargetEnt != nullptr )
            {
                m_IdealMonsterState = MONSTERSTATE_ALERT;
                return m_IdealMonsterState;
            }

            if( HasConditions( bits_COND_SEE_ENEMY ) )
            {
                m_fearTime = gpGlobals->time;
                m_IdealMonsterState = MONSTERSTATE_COMBAT;
                return m_IdealMonsterState;
            }
        }
    }
    break;
    }

    return CTalkMonster::GetIdealState();
}

bool CScientist::CanHeal()
{
    if( ( m_healTime > gpGlobals->time ) || ( m_hTargetEnt == nullptr ) || ( m_hTargetEnt->pev->health > ( m_hTargetEnt->pev->max_health * 0.5 ) ) )
        return false;

    return true;
}

void CScientist::Heal()
{
    if( !CanHeal() )
        return;

    Vector target = m_hTargetEnt->pev->origin - pev->origin;
    if( target.Length() > 100 )
        return;

    m_hTargetEnt->GiveHealth( g_cfg.GetValue<float>( "scientist_heal"sv, 25, this ), DMG_GENERIC );
    // Don't heal again for 1 minute
    m_healTime = gpGlobals->time + 60;
}

class CDeadScientist : public CBaseMonster
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;

    bool HasHumanGibs() override { return true; }

    bool KeyValue( KeyValueData* pkvd ) override;
    int m_iPose; // which sequence to display
    static const char* m_szPoses[7];
};

LINK_ENTITY_TO_CLASS( monster_scientist_dead, CDeadScientist );

const char* CDeadScientist::m_szPoses[] = {"lying_on_back", "lying_on_stomach", "dead_sitting", "dead_hang", "dead_table1", "dead_table2", "dead_table3"};

void CDeadScientist::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8; // g_cfg.GetValue<float>( "scientist_health"sv, 20, this );
    pev->model = MAKE_STRING( "models/scientist.mdl" );

    SetClassification( "human_passive" );
}

bool CDeadScientist::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

SpawnAction CDeadScientist::Spawn()
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
    if( GetBodygroup( ScientistBodygroup::Head ) == HEAD_LUTHER )
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

    return SpawnAction::Spawn;
}

LINK_ENTITY_TO_CLASS( monster_sitting_scientist, CSittingScientist );

BEGIN_DATAMAP( CSittingScientist )
// Don't need to save/restore m_baseSequence (recalced)
    DEFINE_FIELD( m_headTurn, FIELD_INTEGER ),
    DEFINE_FIELD( m_flResponseDelay, FIELD_FLOAT ),
    DEFINE_FUNCTION( SittingThink ),
END_DATAMAP();

// animation sequence aliases
enum SITTING_ANIM
{
    SITTING_ANIM_sitlookleft,
    SITTING_ANIM_sitlookright,
    SITTING_ANIM_sitscared,
    SITTING_ANIM_sitting2,
    SITTING_ANIM_sitting3
};

void CSittingScientist::OnCreate()
{
    CScientist::OnCreate();

    pev->health = 50;
    pev->model = MAKE_STRING( "models/scientist.mdl" );

    SetClassification( "human_passive" );
}

SpawnAction CSittingScientist::Spawn()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );
    Precache();
    InitBoneControllers();

    SetSize( Vector( -14, -14, 0 ), Vector( 14, 14, 36 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    pev->effects = 0;

    m_bloodColor = BLOOD_COLOR_RED;
    m_flFieldOfView = VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )

    m_afCapability = bits_CAP_HEAR | bits_CAP_TURN_HEAD;

    SetBits( pev->spawnflags, SF_MONSTER_PREDISASTER ); // predisaster only!

    if( pev->body == -1 )
    {
        pev->body = 0;
        // -1 chooses a random head
        // pick a head, any head
        SetBodygroup( ScientistBodygroup::Head, RANDOM_LONG( 0, GetBodygroupSubmodelCount( ScientistBodygroup::Head ) - 1 ) );
    }
    // Luther is black, make his hands black
    if( GetBodygroup( ScientistBodygroup::Head ) == HEAD_LUTHER )
        pev->skin = 1;

    m_baseSequence = LookupSequence( "sitlookleft" );
    pev->sequence = m_baseSequence + RANDOM_LONG( 0, 4 );
    ResetSequenceInfo();

    SetThink( &CSittingScientist::SittingThink );
    pev->nextthink = gpGlobals->time + 0.1;

    DROP_TO_FLOOR( edict() );

    return SpawnAction::Spawn;
}

void CSittingScientist::Precache()
{
    m_baseSequence = LookupSequence( "sitlookleft" );
    TalkInit();
}

void CSittingScientist::SittingThink()
{
    CBaseEntity* pent;

    StudioFrameAdvance();

    // try to greet player
    if( FIdleHello() )
    {
        pent = FindNearestFriend( true );
        if( pent )
        {
            float yaw = VecToYaw( pent->pev->origin - pev->origin ) - pev->angles.y;

            if( yaw > 180 )
                yaw -= 360;
            if( yaw < -180 )
                yaw += 360;

            if( yaw > 0 )
                pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
            else
                pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

            ResetSequenceInfo();
            pev->frame = 0;
            SetBoneController( 0, 0 );
        }
    }
    else if( m_fSequenceFinished )
    {
        int i = RANDOM_LONG( 0, 99 );
        m_headTurn = 0;

        if( 0 != m_flResponseDelay && gpGlobals->time > m_flResponseDelay )
        {
            // respond to question
            IdleRespond();
            pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
            m_flResponseDelay = 0;
        }
        else if( i < 30 )
        {
            pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;

            // turn towards player or nearest friend and speak

            if( !FBitSet( m_bitsSaid, bit_saidHelloPlayer ) )
                pent = FindNearestFriend( true );
            else
                pent = FindNearestFriend( false );

            if( !FIdleSpeak() || !pent )
            {
                m_headTurn = RANDOM_LONG( 0, 8 ) * 10 - 40;
                pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
            }
            else
            {
                // only turn head if we spoke
                float yaw = VecToYaw( pent->pev->origin - pev->origin ) - pev->angles.y;

                if( yaw > 180 )
                    yaw -= 360;
                if( yaw < -180 )
                    yaw += 360;

                if( yaw > 0 )
                    pev->sequence = m_baseSequence + SITTING_ANIM_sitlookleft;
                else
                    pev->sequence = m_baseSequence + SITTING_ANIM_sitlookright;

                // AILogger->debug("sitting speak");
            }
        }
        else if( i < 60 )
        {
            pev->sequence = m_baseSequence + SITTING_ANIM_sitting3;
            m_headTurn = RANDOM_LONG( 0, 8 ) * 10 - 40;
            if( RANDOM_LONG( 0, 99 ) < 5 )
            {
                // AILogger->debug("sitting speak2");
                FIdleSpeak();
            }
        }
        else if( i < 80 )
        {
            pev->sequence = m_baseSequence + SITTING_ANIM_sitting2;
        }
        else if( i < 100 )
        {
            pev->sequence = m_baseSequence + SITTING_ANIM_sitscared;
        }

        ResetSequenceInfo();
        pev->frame = 0;
        SetBoneController( 0, m_headTurn );
    }
    pev->nextthink = gpGlobals->time + 0.1;
}

void CSittingScientist::SetAnswerQuestion( CTalkMonster* pSpeaker )
{
    m_flResponseDelay = gpGlobals->time + RANDOM_FLOAT( 3, 4 );
    m_hTalkTarget = (CBaseMonster*)pSpeaker;
}

bool CSittingScientist::FIdleSpeak()
{
    // try to start a conversation, or make statement
    int pitch;

    if( !FOkToSpeak() )
        return false;

    // set global min delay for next conversation
    CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 4.8, 5.2 );

    pitch = GetVoicePitch();

    // if there is a friend nearby to speak to, play sentence, set friend's response time, return

    // try to talk to any standing or sitting scientists nearby
    CBaseEntity* pentFriend = FindNearestFriend( false );

    if( pentFriend && RANDOM_LONG( 0, 1 ) )
    {
        CTalkMonster* pTalkMonster = pentFriend->MyTalkMonsterPointer();
        pTalkMonster->SetAnswerQuestion( this );

        IdleHeadTurn( pentFriend->pev->origin );
        sentences::g_Sentences.PlayRndSz( this, m_szGrp[TLK_PQUESTION], 1.0, ATTN_IDLE, 0, pitch );
        // set global min delay for next conversation
        CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 4.8, 5.2 );
        return true;
    }

    // otherwise, play an idle statement
    if( RANDOM_LONG( 0, 1 ) )
    {
        sentences::g_Sentences.PlayRndSz( this, m_szGrp[TLK_PIDLE], 1.0, ATTN_IDLE, 0, pitch );
        // set global min delay for next conversation
        CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 4.8, 5.2 );
        return true;
    }

    // never spoke
    CTalkMonster::g_talkWaitTime = 0;
    return false;
}
