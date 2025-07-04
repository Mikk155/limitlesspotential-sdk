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
#include "plane.h"
#include "defaultai.h"
#include "squadmonster.h"
#include "talkmonster.h"
#include "COFSquadTalkMonster.h"
#include "customentity.h"
#include "hgrunt_ally_base.h"

BEGIN_DATAMAP( CBaseHGruntAlly )
    DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
    DEFINE_FIELD( m_flNextPainTime, FIELD_TIME ),
    //    DEFINE_FIELD(m_flLastEnemySightTime, FIELD_TIME), // don't save, go to zero
    DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
    DEFINE_FIELD( m_fThrowGrenade, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fStanding, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fFirstEncounter, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_cClipSize, FIELD_INTEGER ),
    DEFINE_FIELD( m_iSentence, FIELD_INTEGER ),
    DEFINE_FIELD( m_iGruntHead, FIELD_INTEGER ),
    DEFINE_FIELD( m_iGruntTorso, FIELD_INTEGER ),
END_DATAMAP();

void CBaseHGruntAlly::OnCreate()
{
    COFSquadTalkMonster::OnCreate();

    m_iszUse = MAKE_STRING( "FG_OK" );
    m_iszUnUse = MAKE_STRING( "FG_WAIT" );

    SetClassification( "human_military_ally" );

    // get voice pitch
    m_voicePitch = 100;
}

void CBaseHGruntAlly::SpeakSentence()
{
    if( m_iSentence == HGRUNT_SENT_NONE )
    {
        // no sentence cued up.
        return;
    }

    if( FOkToSpeak() )
    {
        sentences::g_Sentences.PlayRndSz( this, pGruntSentences[m_iSentence], HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
        JustSpoke();
    }
}

void CBaseHGruntAlly::GibMonster()
{
    if( m_hWaitMedic )
    {
        COFSquadTalkMonster* pMedic = m_hWaitMedic;

        if( pMedic->pev->deadflag != DEAD_NO )
            m_hWaitMedic = nullptr;
        else
            pMedic->HealMe( nullptr );
    }

    DropWeapon( true );

    COFSquadTalkMonster::GibMonster();
}

int CBaseHGruntAlly::ISoundMask()
{
    return bits_SOUND_WORLD |
           bits_SOUND_COMBAT |
           bits_SOUND_PLAYER |
           bits_SOUND_DANGER |
           bits_SOUND_CARCASS |
           bits_SOUND_MEAT |
           bits_SOUND_GARBAGE;
}

bool CBaseHGruntAlly::FOkToSpeak()
{
    // if someone else is talking, don't speak
    if( gpGlobals->time <= COFSquadTalkMonster::g_talkWaitTime )
        return false;

    if( ( pev->spawnflags & SF_MONSTER_GAG ) != 0 )
    {
        if( m_MonsterState != MONSTERSTATE_COMBAT )
        {
            // no talking outside of combat if gagged.
            return false;
        }
    }

    // if player is not in pvs, don't speak
    //    if (!UTIL_FindClientInPVS(this))
    //        return false;

    return true;
}

void CBaseHGruntAlly::JustSpoke()
{
    COFSquadTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 2.0 );
    m_iSentence = HGRUNT_SENT_NONE;
}

void CBaseHGruntAlly::PrescheduleThink()
{
    if( InSquad() && m_hEnemy != nullptr )
    {
        if( HasConditions( bits_COND_SEE_ENEMY ) )
        {
            // update the squad's last enemy sighting time.
            MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
        }
        else
        {
            if( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
            {
                // been a while since we've seen the enemy
                MySquadLeader()->m_fEnemyEluded = true;
            }
        }
    }
}

bool CBaseHGruntAlly::FCanCheckAttacks()
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

bool CBaseHGruntAlly::CheckMeleeAttack1( float flDot, float flDist )
{
    CBaseMonster* pEnemy = nullptr;

    if( m_hEnemy != nullptr )
    {
        pEnemy = m_hEnemy->MyMonsterPointer();
    }

    if( !pEnemy )
    {
        return false;
    }

    if( flDist <= 64 && flDot >= 0.7 && !pEnemy->IsBioWeapon() )
    {
        return true;
    }
    return false;
}

bool CBaseHGruntAlly::CheckRangeAttack1( float flDot, float flDist )
{
    if( CanRangeAttack() )
    {
        const auto maxDistance = GetMaximumRangeAttackDistance();

        // Friendly fire is allowed
        if (!HasConditions(bits_COND_ENEMY_OCCLUDED) && flDist <= maxDistance && flDot >= 0.5 /*&& NoFriendlyFire()*/)
        {
            TraceResult tr;

            CBaseEntity* pEnemy = m_hEnemy;

            // if (!pEnemy->IsPlayer() && flDist <= 64)
            //{
            //    // kick nonclients, but don't shoot at them.
            //    return false;
            // }

            // TODO: kinda odd that this doesn't use GetGunPosition like the original
            Vector vecSrc = pev->origin + Vector( 0, 0, 55 );

            // Fire at last known position, adjusting for target origin being offset from entity origin
            const auto targetOrigin = pEnemy->BodyTarget( vecSrc );

            const auto targetPosition = targetOrigin - pEnemy->pev->origin + m_vecEnemyLKP;

            // verify that a bullet fired from the gun will hit the enemy before the world.
            UTIL_TraceLine( vecSrc, targetPosition, dont_ignore_monsters, edict(), &tr );

            m_lastAttackCheck = tr.flFraction == 1.0 ? true : tr.pHit && GET_PRIVATE( tr.pHit ) == pEnemy;

            return m_lastAttackCheck;
        }
    }

    return false;
}

bool CBaseHGruntAlly::CheckRangeAttack2( float flDot, float flDist )
{
    if( !CanUseThrownGrenades() && !CanUseGrenadeLauncher() )
    {
        return false;
    }

    // if the grunt isn't moving, it's ok to check.
    if( m_flGroundSpeed != 0 )
    {
        m_fThrowGrenade = false;
        return m_fThrowGrenade;
    }

    // assume things haven't changed too much since last time
    if( gpGlobals->time < m_flNextGrenadeCheck )
    {
        return m_fThrowGrenade;
    }

    if( !FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) && m_hEnemy->pev->waterlevel == WaterLevel::Dry && m_vecEnemyLKP.z > pev->absmax.z )
    {
        //!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to
        // be grenaded.
        // don't throw grenades at anything that isn't on the ground!
        m_fThrowGrenade = false;
        return m_fThrowGrenade;
    }

    Vector vecTarget;

    if( CanUseThrownGrenades() )
    {
        // find feet
        if( RANDOM_LONG( 0, 1 ) )
        {
            // magically know where they are
            vecTarget = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
        }
        else
        {
            // toss it to where you last saw them
            vecTarget = m_vecEnemyLKP;
        }
        // vecTarget = m_vecEnemyLKP + (m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin);
        // estimate position
        // vecTarget = vecTarget + m_hEnemy->pev->velocity * 2;
    }
    else
    {
        // find target
        // vecTarget = m_hEnemy->BodyTarget( pev->origin );
        vecTarget = m_vecEnemyLKP + ( m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin );
        // estimate position
        if( HasConditions( bits_COND_SEE_ENEMY ) )
            vecTarget = vecTarget + ( ( vecTarget - pev->origin ).Length() / g_cfg.GetValue<float>( "hgrunt_ally_gspeed"sv, 600, this ) ) * m_hEnemy->pev->velocity;
    }

    // are any of my squad members near the intended grenade impact area?
    if( InSquad() )
    {
        if( SquadMemberInRange( vecTarget, 256 ) )
        {
            // crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
            m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
            m_fThrowGrenade = false;
        }
    }

    if( ( vecTarget - pev->origin ).Length2D() <= 256 )
    {
        // crap, I don't want to blow myself up
        m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
        m_fThrowGrenade = false;
        return m_fThrowGrenade;
    }


    if( CanUseThrownGrenades() )
    {
        Vector vecToss = VecCheckToss( this, GetGunPosition(), vecTarget, 0.5 );

        if( vecToss != g_vecZero )
        {
            m_vecTossVelocity = vecToss;

            // throw a hand grenade
            m_fThrowGrenade = true;
            // don't check again for a while.
            m_flNextGrenadeCheck = gpGlobals->time; // 1/3 second.
        }
        else
        {
            // don't throw
            m_fThrowGrenade = false;
            // don't check again for a while.
            m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
        }
    }
    else
    {
        Vector vecToss = VecCheckThrow( this, GetGunPosition(), vecTarget, g_cfg.GetValue<float>( "hgrunt_ally_gspeed"sv, 600, this ), 0.5 );

        if( vecToss != g_vecZero )
        {
            m_vecTossVelocity = vecToss;

            // throw a hand grenade
            m_fThrowGrenade = true;
            // don't check again for a while.
            m_flNextGrenadeCheck = gpGlobals->time + 0.3; // 1/3 second.
        }
        else
        {
            // don't throw
            m_fThrowGrenade = false;
            // don't check again for a while.
            m_flNextGrenadeCheck = gpGlobals->time + 1; // one full second.
        }
    }

    return m_fThrowGrenade;
}

void CBaseHGruntAlly::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    // check for helmet shot
    if( ptr->iHitgroup == 11 )
    {
        // make sure we're wearing one
        // TODO: disabled for ally
        if (/*GetBodygroup( HGruntAllyBodygroup::Head ) == HGruntAllyHead::GasMask &&*/ (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB)) != 0)
        {
            // absorb damage
            flDamage -= 20;
            if( flDamage <= 0 )
            {
                UTIL_Ricochet( ptr->vecEndPos, 1.0 );
                flDamage = 0.01;
            }
        }
        // it's head shot anyways
        ptr->iHitgroup = HITGROUP_HEAD;
    }
    // PCV absorbs some damage types
    else if( ( ptr->iHitgroup == HITGROUP_CHEST || ptr->iHitgroup == HITGROUP_STOMACH ) && ( bitsDamageType & ( DMG_BLAST | DMG_BULLET | DMG_SLASH ) ) != 0 )
    {
        flDamage *= 0.5;
    }

    COFSquadTalkMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
}

bool CBaseHGruntAlly::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    // make sure friends talk about it if player hurts talkmonsters...
    bool ret = COFSquadTalkMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );

    if( !IsAlive() || pev->deadflag != DEAD_NO )
        return ret;

    Forget( bits_MEMORY_INCOVER );

    if( m_MonsterState != MONSTERSTATE_PRONE && ( attacker->pev->flags & FL_CLIENT ) != 0 )
    {
        // This is a heurstic to determine if the player intended to harm me
        // If I have an enemy, we can't establish intent (may just be crossfire)
        if( m_hEnemy == nullptr )
        {
            // If the player was facing directly at me, or I'm already suspicious, get mad
            if( gpGlobals->time - m_flLastHitByPlayer < 4.0 && m_iPlayerHits > 2 && ( ( m_afMemory & bits_MEMORY_SUSPICIOUS ) != 0 || IsFacing( attacker, pev->origin ) ) )
            {
                // Alright, now I'm pissed!
                PlaySentence( "FG_MAD", 4, VOL_NORM, ATTN_NORM );

                Remember( bits_MEMORY_PROVOKED );
                StopFollowing( true );
                AILogger->debug( "HGrunt Ally is now MAD!" );
            }
            else
            {
                // Hey, be careful with that
                PlaySentence( "FG_SHOT", 4, VOL_NORM, ATTN_NORM );
                Remember( bits_MEMORY_SUSPICIOUS );

                if( 4.0 > gpGlobals->time - m_flLastHitByPlayer )
                    ++m_iPlayerHits;
                else
                    m_iPlayerHits = 0;

                m_flLastHitByPlayer = gpGlobals->time;

                AILogger->debug( "HGrunt Ally is now SUSPICIOUS!" );
            }
        }
        else if( !m_hEnemy->IsPlayer() )
        {
            PlaySentence( "FG_SHOT", 4, VOL_NORM, ATTN_NORM );
        }
    }

    return ret;
}

void CBaseHGruntAlly::SetYawSpeed()
{
    int ys;

    switch ( m_Activity )
    {
    case ACT_IDLE:
        ys = 150;
        break;
    case ACT_RUN:
        ys = 150;
        break;
    case ACT_WALK:
        ys = 180;
        break;
    case ACT_RANGE_ATTACK1:
        ys = 120;
        break;
    case ACT_RANGE_ATTACK2:
        ys = 120;
        break;
    case ACT_MELEE_ATTACK1:
        ys = 120;
        break;
    case ACT_MELEE_ATTACK2:
        ys = 120;
        break;
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        ys = 180;
        break;
    case ACT_GLIDE:
    case ACT_FLY:
        ys = 30;
        break;
    default:
        ys = 90;
        break;
    }

    pev->yaw_speed = ys;
}

void CBaseHGruntAlly::IdleSound()
{
    if( FOkToSpeak() && ( 0 != g_fGruntAllyQuestion || RANDOM_LONG( 0, 1 ) ) )
    {
        if( 0 == g_fGruntAllyQuestion )
        {
            // ask question or make statement
            switch ( RANDOM_LONG( 0, 2 ) )
            {
            case 0: // check in
                sentences::g_Sentences.PlayRndSz( this, "FG_CHECK", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                g_fGruntAllyQuestion = 1;
                break;
            case 1: // question
                sentences::g_Sentences.PlayRndSz( this, "FG_QUEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                g_fGruntAllyQuestion = 2;
                break;
            case 2: // statement
                sentences::g_Sentences.PlayRndSz( this, "FG_IDLE", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            }
        }
        else
        {
            switch ( g_fGruntAllyQuestion )
            {
            case 1: // check in
                sentences::g_Sentences.PlayRndSz( this, "FG_CLEAR", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            case 2: // question
                sentences::g_Sentences.PlayRndSz( this, "FG_ANSWER", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            }
            g_fGruntAllyQuestion = 0;
        }
        JustSpoke();
    }
}

void CBaseHGruntAlly::CheckAmmo()
{
    if( m_cAmmoLoaded <= 0 )
    {
        SetConditions( bits_COND_NO_AMMO_LOADED );
    }
}

CBaseEntity* CBaseHGruntAlly::Kick()
{
    TraceResult tr;

    UTIL_MakeVectors( pev->angles );
    Vector vecStart = pev->origin;
    vecStart.z += pev->size.z * 0.5;
    Vector vecEnd = vecStart + ( gpGlobals->v_forward * 70 );

    UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, edict(), &tr );

    if( tr.pHit )
    {
        CBaseEntity* pEntity = CBaseEntity::Instance( tr.pHit );
        return pEntity;
    }

    return nullptr;
}

Vector CBaseHGruntAlly::GetGunPosition()
{
    if( m_fStanding )
    {
        return pev->origin + Vector( 0, 0, 60 );
    }
    else
    {
        return pev->origin + Vector( 0, 0, 48 );
    }
}

void CBaseHGruntAlly::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case HGRUNT_AE_DROP_GUN:
        DropWeapon( false );
        break;

    case HGRUNT_AE_GREN_TOSS:
    {
        if( CanUseThrownGrenades() )
        {
            UTIL_MakeVectors( pev->angles );
            // CGrenade::ShootTimed(this, pev->origin + gpGlobals->v_forward * 34 + Vector (0, 0, 32), m_vecTossVelocity, 3.5);
            CGrenade::ShootTimed( this, GetGunPosition(), m_vecTossVelocity, 3.5 );

            m_fThrowGrenade = false;
            m_flNextGrenadeCheck = gpGlobals->time + 6; // wait six seconds before even looking again to see if a grenade can be thrown.
                                                        // !!!LATER - when in a group, only try to throw grenade if ordered.
        }
    }
    break;

    case HGRUNT_AE_GREN_LAUNCH:
    {
        if( CanUseGrenadeLauncher() )
        {
            EmitSound( CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM );
            CGrenade::ShootContact( this, GetGunPosition(), m_vecTossVelocity );
            m_fThrowGrenade = false;
            if( g_cfg.GetSkillLevel() == SkillLevel::Hard )
                m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 ); // wait a random amount of time before shooting again
            else
                m_flNextGrenadeCheck = gpGlobals->time + 6; // wait six seconds before even looking again to see if a grenade can be thrown.
        }
    }
    break;

    case HGRUNT_AE_GREN_DROP:
    {
        if( CanUseThrownGrenades() )
        {
            UTIL_MakeVectors( pev->angles );
            CGrenade::ShootTimed( this, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
        }
    }
    break;

    case HGRUNT_AE_KICK:
    {
        CBaseEntity* pHurt = Kick();

        if( pHurt )
        {
            // SOUND HERE!
            UTIL_MakeVectors( pev->angles );
            pHurt->pev->punchangle.x = 15;
            pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
            pHurt->TakeDamage( this, this, g_cfg.GetValue<float>( "hgrunt_ally_kick"sv, 5, this ), DMG_CLUB );
        }
    }
    break;

    case HGRUNT_AE_CAUGHT_ENEMY:
    {
        if( FOkToSpeak() )
        {
            sentences::g_Sentences.PlayRndSz( this, "FG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
            JustSpoke();
        }
        break;
    }

    default:
        COFSquadTalkMonster::HandleAnimEvent( pEvent );
        break;
    }
}

void CBaseHGruntAlly::SpawnCore()
{
    Precache();

    SetModel( STRING( pev->model ) );
    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_RED;
    pev->effects = 0;
    m_flFieldOfView = 0.2; // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;
    m_flNextGrenadeCheck = gpGlobals->time + 1;
    m_flNextPainTime = gpGlobals->time;
    m_iSentence = HGRUNT_SENT_NONE;

    m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP | bits_CAP_HEAR;

    m_fEnemyEluded = false;
    m_fFirstEncounter = true; // this is true when the grunt spawns, because he hasn't encountered an enemy yet.

    m_HackedGunPos = Vector( 0, 0, 55 );

    pev->body = 0;
    pev->skin = 0;

    COFSquadTalkMonster::g_talkWaitTime = 0;

    m_flMedicWaitTime = gpGlobals->time;

    MonsterInit();
}

void CBaseHGruntAlly::Precache()
{
    TalkInit();

    PrecacheModel( STRING( pev->model ) );

    PrecacheSound( "hgrunt/gr_mgun1.wav" );
    PrecacheSound( "hgrunt/gr_mgun2.wav" );

    PrecacheSound( "fgrunt/death1.wav" );
    PrecacheSound( "fgrunt/death2.wav" );
    PrecacheSound( "fgrunt/death3.wav" );
    PrecacheSound( "fgrunt/death4.wav" );
    PrecacheSound( "fgrunt/death5.wav" );
    PrecacheSound( "fgrunt/death6.wav" );

    PrecacheSound( "fgrunt/pain1.wav" );
    PrecacheSound( "fgrunt/pain2.wav" );
    PrecacheSound( "fgrunt/pain3.wav" );
    PrecacheSound( "fgrunt/pain4.wav" );
    PrecacheSound( "fgrunt/pain5.wav" );
    PrecacheSound( "fgrunt/pain6.wav" );

    PrecacheSound( "hgrunt/gr_reload1.wav" );

    PrecacheSound( "weapons/glauncher.wav" );

    PrecacheSound( "weapons/sbarrel1.wav" );

    PrecacheSound( "fgrunt/medic.wav" );

    PrecacheSound( "zombie/claw_miss2.wav" ); // because we use the basemonster SWIPE animation event

    COFSquadTalkMonster::Precache();
}

void CBaseHGruntAlly::StartTask( const Task_t* pTask )
{
    m_iTaskStatus = TASKSTATUS_RUNNING;

    switch ( pTask->iTask )
    {
    case TASK_GRUNT_CHECK_FIRE:
        if( !NoFriendlyFire() )
        {
            SetConditions( bits_COND_GRUNT_NOFIRE );
        }
        TaskComplete();
        break;

    case TASK_GRUNT_SPEAK_SENTENCE:
        SpeakSentence();
        TaskComplete();
        break;

    case TASK_WALK_PATH:
    case TASK_RUN_PATH:
        // grunt no longer assumes he is covered if he moves
        Forget( bits_MEMORY_INCOVER );
        COFSquadTalkMonster::StartTask( pTask );
        break;

    case TASK_RELOAD:
        m_IdealActivity = ACT_RELOAD;
        break;

    case TASK_GRUNT_FACE_TOSS_DIR:
        break;

    case TASK_FACE_IDEAL:
    case TASK_FACE_ENEMY:
        COFSquadTalkMonster::StartTask( pTask );
        if( pev->movetype == MOVETYPE_FLY )
        {
            m_IdealActivity = ACT_GLIDE;
        }
        break;

    default:
        COFSquadTalkMonster::StartTask( pTask );
        break;
    }
}

void CBaseHGruntAlly::RunTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_GRUNT_FACE_TOSS_DIR:
    {
        // project a point along the toss vector and turn to face that point.
        MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
        ChangeYaw( pev->yaw_speed );

        if( FacingIdeal() )
        {
            m_iTaskStatus = TASKSTATUS_COMPLETE;
        }
        break;
    }
    default:
    {
        COFSquadTalkMonster::RunTask( pTask );
        break;
    }
    }
}

void CBaseHGruntAlly::PainSound()
{
    if( gpGlobals->time > m_flNextPainTime )
    {
#if 0
        if( RANDOM_LONG( 0, 99 ) < 5 )
        {
            // pain sentences are rare
            if( FOkToSpeak() )
            {
                sentences::g_Sentences.PlayRndSz( this, "FG_PAIN", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM );
                JustSpoke();
                return;
            }
        }
#endif
        switch ( RANDOM_LONG( 0, 7 ) )
        {
        case 0:
            EmitSound( CHAN_VOICE, "fgrunt/pain3.wav", 1, ATTN_NORM );
            break;
        case 1:
            EmitSound( CHAN_VOICE, "fgrunt/pain4.wav", 1, ATTN_NORM );
            break;
        case 2:
            EmitSound( CHAN_VOICE, "fgrunt/pain5.wav", 1, ATTN_NORM );
            break;
        case 3:
            EmitSound( CHAN_VOICE, "fgrunt/pain1.wav", 1, ATTN_NORM );
            break;
        case 4:
            EmitSound( CHAN_VOICE, "fgrunt/pain2.wav", 1, ATTN_NORM );
            break;
        case 5:
            EmitSound( CHAN_VOICE, "fgrunt/pain6.wav", 1, ATTN_NORM );
            break;
        }

        m_flNextPainTime = gpGlobals->time + 1;
    }
}

void CBaseHGruntAlly::DeathSound()
{
    switch ( RANDOM_LONG( 0, 5 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "fgrunt/death1.wav", 1, ATTN_IDLE );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "fgrunt/death2.wav", 1, ATTN_IDLE );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "fgrunt/death3.wav", 1, ATTN_IDLE );
        break;
    case 3:
        EmitSound( CHAN_VOICE, "fgrunt/death4.wav", 1, ATTN_IDLE );
        break;
    case 4:
        EmitSound( CHAN_VOICE, "fgrunt/death5.wav", 1, ATTN_IDLE );
        break;
    case 5:
        EmitSound( CHAN_VOICE, "fgrunt/death6.wav", 1, ATTN_IDLE );
        break;
    }
}

Task_t tlGruntAllyFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slGruntAllyFail[] =
    {
        {tlGruntAllyFail,
            std::size( tlGruntAllyFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2 |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK2,
            0,
            "Grunt Fail"},
};

Task_t tlGruntAllyCombatFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slGruntAllyCombatFail[] =
    {
        {tlGruntAllyCombatFail,
            std::size( tlGruntAllyCombatFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2,
            0,
            "Grunt Combat Fail"},
};

Task_t tlGruntAllyVictoryDance[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_WAIT, (float)1.5},
        {TASK_GET_PATH_TO_ENEMY_CORPSE, (float)0},
        {TASK_WALK_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_VICTORY_DANCE},
};

Schedule_t slGruntAllyVictoryDance[] =
    {
        {tlGruntAllyVictoryDance,
            std::size( tlGruntAllyVictoryDance ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "GruntVictoryDance"},
};

Task_t tlGruntAllyEstablishLineOfFire[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_GRUNT_ELOF_FAIL},
        {TASK_GET_PATH_TO_ENEMY, (float)0},
        {TASK_GRUNT_SPEAK_SENTENCE, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
};

/**
 *    @brief move to a position that allows the grunt to attack.
 */
Schedule_t slGruntAllyEstablishLineOfFire[] =
    {
        {tlGruntAllyEstablishLineOfFire,
            std::size( tlGruntAllyEstablishLineOfFire ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2 |
                bits_COND_CAN_MELEE_ATTACK2 |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "GruntEstablishLineOfFire"},
};

Task_t tlGruntAllyFoundEnemy[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_SIGNAL1},
};

/**
 *    @brief grunt established sight with an enemy that was hiding from the squad.
 */
Schedule_t slGruntAllyFoundEnemy[] =
    {
        {tlGruntAllyFoundEnemy,
            std::size( tlGruntAllyFoundEnemy ),
            bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "GruntFoundEnemy"},
};

Task_t tlGruntAllyCombatFace1[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_WAIT, (float)1.5},
        {TASK_SET_SCHEDULE, (float)SCHED_GRUNT_SWEEP},
};

Schedule_t slGruntAllyCombatFace[] =
    {
        {tlGruntAllyCombatFace1,
            std::size( tlGruntAllyCombatFace1 ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2,
            0,
            "Combat Face"},
};

Task_t tlGruntAllySignalSuppress[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_SIGNAL2},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

/**
 *    @brief don't stop shooting until the clip is empty or grunt gets hurt.
 */
Schedule_t slGruntAllySignalSuppress[] =
    {
        {tlGruntAllySignalSuppress,
            std::size( tlGruntAllySignalSuppress ),
            bits_COND_ENEMY_DEAD |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_NO_AMMO_LOADED,

            bits_SOUND_DANGER,
            "SignalSuppress"},
};

Task_t tlGruntAllySuppress[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slGruntAllySuppress[] =
    {
        {tlGruntAllySuppress,
            std::size( tlGruntAllySuppress ),
            bits_COND_ENEMY_DEAD |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_NO_AMMO_LOADED,

            bits_SOUND_DANGER,
            "Suppress"},
};

Task_t tlGruntAllyWaitInCover[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)1},
};

/**
 *    @brief we don't allow danger or the ability to attack to break a grunt's run to cover schedule,
 *    but when a grunt is in cover, we do want them to attack if they can.
 */
Schedule_t slGruntAllyWaitInCover[] =
    {
        {tlGruntAllyWaitInCover,
            std::size( tlGruntAllyWaitInCover ),
            bits_COND_NEW_ENEMY |
                bits_COND_HEAR_SOUND |
                bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2 |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK2,

            bits_SOUND_DANGER,
            "GruntWaitInCover"},
};

//=========================================================
// run to cover.
// !!!BUGBUG - set a decent fail schedule here.
//=========================================================
Task_t tlGruntAllyTakeCover1[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_GRUNT_TAKECOVER_FAILED},
        {TASK_WAIT, (float)0.2},
        {TASK_FIND_COVER_FROM_ENEMY, (float)0},
        {TASK_GRUNT_SPEAK_SENTENCE, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
        {TASK_SET_SCHEDULE, (float)SCHED_GRUNT_WAIT_FACE_ENEMY},
};

Schedule_t slGruntAllyTakeCover[] =
    {
        {tlGruntAllyTakeCover1,
            std::size( tlGruntAllyTakeCover1 ),
            0,
            0,
            "TakeCover"},
};

Task_t tlGruntAllyGrenadeCover1[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FIND_COVER_FROM_ENEMY, (float)99},
        {TASK_FIND_FAR_NODE_COVER_FROM_ENEMY, (float)384},
        {TASK_PLAY_SEQUENCE, (float)ACT_SPECIAL_ATTACK1},
        {TASK_CLEAR_MOVE_WAIT, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_SET_SCHEDULE, (float)SCHED_GRUNT_WAIT_FACE_ENEMY},
};

/**
 *    @brief drop grenade then run to cover.
 */
Schedule_t slGruntAllyGrenadeCover[] =
    {
        {tlGruntAllyGrenadeCover1,
            std::size( tlGruntAllyGrenadeCover1 ),
            0,
            0,
            "GrenadeCover"},
};

Task_t tlGruntAllyTossGrenadeCover1[] =
    {
        {TASK_FACE_ENEMY, (float)0},
        {TASK_RANGE_ATTACK2, (float)0},
        {TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ENEMY},
};

/**
 *    @brief drop grenade then run to cover.
 */
Schedule_t slGruntAllyTossGrenadeCover[] =
    {
        {tlGruntAllyTossGrenadeCover1,
            std::size( tlGruntAllyTossGrenadeCover1 ),
            0,
            0,
            "TossGrenadeCover"},
};

Task_t tlGruntAllyTakeCoverFromBestSound[] =
    {
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_COWER}, // duck and cover if cannot move from explosion
        {TASK_STOP_MOVING, (float)0},
        {TASK_FIND_COVER_FROM_BEST_SOUND, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
        {TASK_TURN_LEFT, (float)179},
};

/**
 *    @brief hide from the loudest sound source (to run from grenade)
 */
Schedule_t slGruntAllyTakeCoverFromBestSound[] =
    {
        {tlGruntAllyTakeCoverFromBestSound,
            std::size( tlGruntAllyTakeCoverFromBestSound ),
            0,
            0,
            "GruntTakeCoverFromBestSound"},
};

Task_t tlGruntAllyHideReload[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_FAIL_SCHEDULE, (float)SCHED_RELOAD},
        {TASK_FIND_COVER_FROM_ENEMY, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_REMEMBER, (float)bits_MEMORY_INCOVER},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_RELOAD},
};

Schedule_t slGruntAllyHideReload[] =
    {
        {tlGruntAllyHideReload,
            std::size( tlGruntAllyHideReload ),
            bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "GruntHideReload"}};

Task_t tlGruntAllySweep[] =
    {
        {TASK_TURN_LEFT, (float)179},
        {TASK_WAIT, (float)1},
        {TASK_TURN_LEFT, (float)179},
        {TASK_WAIT, (float)1},
};

/**
 *    @brief Do a turning sweep of the area
 */
Schedule_t slGruntAllySweep[] =
    {
        {tlGruntAllySweep,
            std::size( tlGruntAllySweep ),

            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2 |
                bits_COND_HEAR_SOUND,

            bits_SOUND_WORLD | // sound flags
                bits_SOUND_DANGER |
                bits_SOUND_PLAYER,

            "Grunt Sweep"},
};

Task_t tlGruntAllyRangeAttack1A[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_CROUCH},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

/**
 *    @brief Overridden because base class stops attacking when the enemy is occluded.
 *    grunt's grenade toss requires the enemy be occluded.
 */
Schedule_t slGruntAllyRangeAttack1A[] =
    {
        {tlGruntAllyRangeAttack1A,
            std::size( tlGruntAllyRangeAttack1A ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_ENEMY_OCCLUDED |
                bits_COND_HEAR_SOUND |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_NO_AMMO_LOADED,

            bits_SOUND_DANGER,
            "Range Attack1A"},
};

Task_t tlGruntAllyRangeAttack1B[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_IDLE_ANGRY},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_GRUNT_CHECK_FIRE, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

/**
 *    @brief Overridden because base class stops attacking when the enemy is occluded.
 *    grunt's grenade toss requires the enemy be occluded.
 */
Schedule_t slGruntAllyRangeAttack1B[] =
    {
        {tlGruntAllyRangeAttack1B,
            std::size( tlGruntAllyRangeAttack1B ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_ENEMY_OCCLUDED |
                bits_COND_NO_AMMO_LOADED |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "Range Attack1B"},
};

Task_t tlGruntAllyRangeAttack2[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_GRUNT_FACE_TOSS_DIR, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_RANGE_ATTACK2},
        {TASK_SET_SCHEDULE, (float)SCHED_GRUNT_WAIT_FACE_ENEMY}, // don't run immediately after throwing grenade.
};

/**
 *    @brief Overridden because base class stops attacking when the enemy is occluded.
 *    grunt's grenade toss requires the enemy be occluded.
 */
Schedule_t slGruntAllyRangeAttack2[] =
    {
        {tlGruntAllyRangeAttack2,
            std::size( tlGruntAllyRangeAttack2 ),
            0,
            0,
            "RangeAttack2"},
};

Task_t tlGruntAllyRepel[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_GLIDE},
};

Schedule_t slGruntAllyRepel[] =
    {
        {tlGruntAllyRepel,
            std::size( tlGruntAllyRepel ),
            bits_COND_SEE_ENEMY |
                bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER |
                bits_SOUND_COMBAT |
                bits_SOUND_PLAYER,
            "Repel"},
};

Task_t tlGruntAllyRepelAttack[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_FLY},
};

Schedule_t slGruntAllyRepelAttack[] =
    {
        {tlGruntAllyRepelAttack,
            std::size( tlGruntAllyRepelAttack ),
            bits_COND_ENEMY_OCCLUDED,
            0,
            "Repel Attack"},
};

Task_t tlGruntAllyRepelLand[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_LAND},
        {TASK_GET_PATH_TO_LASTPOSITION, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_CLEAR_LASTPOSITION, (float)0},
};

Schedule_t slGruntAllyRepelLand[] =
    {
        {tlGruntAllyRepelLand,
            std::size( tlGruntAllyRepelLand ),
            bits_COND_SEE_ENEMY |
                bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER |
                bits_SOUND_COMBAT |
                bits_SOUND_PLAYER,
            "Repel Land"},
};

Task_t tlGruntAllyFollow[] =
    {
        {TASK_MOVE_TO_TARGET_RANGE, (float)128}, // Move within 128 of target ent (client)
        {TASK_SET_SCHEDULE, (float)SCHED_TARGET_FACE},
};

Schedule_t slGruntAllyFollow[] =
    {
        {tlGruntAllyFollow,
            std::size( tlGruntAllyFollow ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_PROVOKED,
            bits_SOUND_DANGER,
            "Follow"},
};

Task_t tlGruntAllyFaceTarget[] =
    {
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_FACE_TARGET, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_SET_SCHEDULE, (float)SCHED_TARGET_CHASE},
};

Schedule_t slGruntAllyFaceTarget[] =
    {
        {tlGruntAllyFaceTarget,
            std::size( tlGruntAllyFaceTarget ),
            bits_COND_CLIENT_PUSH |
                bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_PROVOKED,
            bits_SOUND_DANGER,
            "FaceTarget"},
};

Task_t tlGruntAllyIdleStand[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},            // repick IDLESTAND every two seconds.
        {TASK_TLK_HEADRESET, (float)0}, // reset head position
};

Schedule_t slGruntAllyIdleStand[] =
    {
        {tlGruntAllyIdleStand,
            std::size( tlGruntAllyIdleStand ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_SMELL |
                bits_COND_PROVOKED,

            bits_SOUND_COMBAT | // sound flags - change these, and you'll break the talking code.
                                // bits_SOUND_PLAYER        |
                                // bits_SOUND_WORLD        |

                bits_SOUND_DANGER |
                bits_SOUND_MEAT | // scents
                bits_SOUND_CARCASS |
                bits_SOUND_GARBAGE,
            "IdleStand"},
};

BEGIN_CUSTOM_SCHEDULES( CBaseHGruntAlly )
slGruntAllyFollow,
    slGruntAllyFaceTarget,
    slGruntAllyIdleStand,
    slGruntAllyFail,
    slGruntAllyCombatFail,
    slGruntAllyVictoryDance,
    slGruntAllyEstablishLineOfFire,
    slGruntAllyFoundEnemy,
    slGruntAllyCombatFace,
    slGruntAllySignalSuppress,
    slGruntAllySuppress,
    slGruntAllyWaitInCover,
    slGruntAllyTakeCover,
    slGruntAllyGrenadeCover,
    slGruntAllyTossGrenadeCover,
    slGruntAllyTakeCoverFromBestSound,
    slGruntAllyHideReload,
    slGruntAllySweep,
    slGruntAllyRangeAttack1A,
    slGruntAllyRangeAttack1B,
    slGruntAllyRangeAttack2,
    slGruntAllyRepel,
    slGruntAllyRepelAttack,
    slGruntAllyRepelLand
    END_CUSTOM_SCHEDULES();

std::tuple<int, Activity> CBaseHGruntAlly::GetSequenceForActivity( Activity NewActivity )
{
    int iSequence;

    switch ( NewActivity )
    {
    case ACT_RANGE_ATTACK2:
        // grunt is going to a secondary long range attack. This may be a thrown
        // grenade or fired grenade, we must determine which and pick proper sequence
        if( CanUseThrownGrenades() )
        {
            // get toss anim
            iSequence = LookupSequence( "throwgrenade" );
        }
        else
        {
            // TODO: maybe check if we can actually use the grenade launcher before doing this
            //  get launch anim
            iSequence = LookupSequence( "launchgrenade" );
        }
        break;
    case ACT_RUN:
        if( pev->health <= HGRUNT_LIMP_HEALTH )
        {
            // limp!
            iSequence = LookupActivity( ACT_RUN_HURT );
        }
        else
        {
            iSequence = LookupActivity( NewActivity );
        }
        break;
    case ACT_WALK:
        if( pev->health <= HGRUNT_LIMP_HEALTH )
        {
            // limp!
            iSequence = LookupActivity( ACT_WALK_HURT );
        }
        else
        {
            iSequence = LookupActivity( NewActivity );
        }
        break;
    case ACT_IDLE:
        if( m_MonsterState == MONSTERSTATE_COMBAT )
        {
            NewActivity = ACT_IDLE_ANGRY;
        }
        iSequence = LookupActivity( NewActivity );
        break;
    default:
        iSequence = LookupActivity( NewActivity );
        break;
    }

    return {iSequence, NewActivity};
}

void CBaseHGruntAlly::SetActivity( Activity NewActivity )
{
    const auto [iSequence, activity] = GetSequenceForActivity( NewActivity );

    m_Activity = activity; // Go ahead and set this so it doesn't keep trying when the anim is not present

    // Set to the desired anim, or default anim if the desired is not present
    if( iSequence > ACTIVITY_NOT_AVAILABLE )
    {
        if( pev->sequence != iSequence || !m_fSequenceLoops )
        {
            pev->frame = 0;
        }

        pev->sequence = iSequence; // Set to the reset anim (if it's there)
        ResetSequenceInfo();
        SetYawSpeed();
    }
    else
    {
        // Not available try to get default anim
        AILogger->debug( "{} has no sequence for act:{}", STRING( pev->classname ), activity );
        pev->sequence = 0; // Set to the reset anim (if it's there)
    }
}

const Schedule_t* CBaseHGruntAlly::GetSchedule()
{
    // clear old sentence
    m_iSentence = HGRUNT_SENT_NONE;

    // flying? If PRONE, barnacle has me. IF not, it's assumed I am rapelling.
    if( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
    {
        if( ( pev->flags & FL_ONGROUND ) != 0 )
        {
            // just landed
            pev->movetype = MOVETYPE_STEP;
            return GetScheduleOfType( SCHED_GRUNT_REPEL_LAND );
        }
        else
        {
            // repel down a rope,
            if( m_MonsterState == MONSTERSTATE_COMBAT )
                return GetScheduleOfType( SCHED_GRUNT_REPEL_ATTACK );
            else
                return GetScheduleOfType( SCHED_GRUNT_REPEL );
        }
    }

    if( auto healSchedule = GetHealSchedule(); healSchedule )
    {
        return healSchedule;
    }

    // grunts place HIGH priority on running away from danger sounds.
    if( HasConditions( bits_COND_HEAR_SOUND ) )
    {
        CSound* pSound;
        pSound = PBestSound();

        ASSERT( pSound != nullptr );
        if( pSound )
        {
            if( ( pSound->m_iType & bits_SOUND_DANGER ) != 0 )
            {
                // dangerous sound nearby!

                //!!!KELLY - currently, this is the grunt's signal that a grenade has landed nearby,
                // and the grunt should find cover from the blast
                // good place for "SHIT!" or some other colorful verbal indicator of dismay.
                // It's not safe to play a verbal order here "Scatter", etc cause
                // this may only affect a single individual in a squad.

                if( FOkToSpeak() )
                {
                    sentences::g_Sentences.PlayRndSz( this, "FG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
            }
            /*
            if (!HasConditions( bits_COND_SEE_ENEMY ) && ( pSound->m_iType & (bits_SOUND_PLAYER | bits_SOUND_COMBAT) ))
            {
                MakeIdealYaw( pSound->m_vecOrigin );
            }
            */
        }
    }
    switch ( m_MonsterState )
    {
    case MONSTERSTATE_COMBAT:
    {
        // dead enemy
        if( HasConditions( bits_COND_ENEMY_DEAD ) )
        {
            if( FOkToSpeak() )
            {
                PlaySentence( "FG_KILL", 4, VOL_NORM, ATTN_NORM );
            }

            // call base class, all code to handle dead enemies is centralized there.
            return COFSquadTalkMonster::GetSchedule();
        }

        if( m_hWaitMedic )
        {
            COFSquadTalkMonster* pMedic = m_hWaitMedic;

            if( pMedic->pev->deadflag != DEAD_NO )
                m_hWaitMedic = nullptr;
            else
                pMedic->HealMe( nullptr );

            m_flMedicWaitTime = gpGlobals->time + 5.0;
        }

        if( auto torchSchedule = GetTorchSchedule(); torchSchedule )
        {
            return torchSchedule;
        }

        // new enemy
        // Do not fire until fired upon
        if( HasAllConditions( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE ) )
        {
            if( InSquad() )
            {
                MySquadLeader()->m_fEnemyEluded = false;

                if( !IsLeader() )
                {
                    return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
                }
                else
                {
                    //!!!KELLY - the leader of a squad of grunts has just seen the player or a
                    // monster and has made it the squad's enemy. You
                    // can check pev->flags for FL_CLIENT to determine whether this is the player
                    // or a monster. He's going to immediately start
                    // firing, though. If you'd like, we can make an alternate "first sight"
                    // schedule where the leader plays a handsign anim
                    // that gives us enough time to hear a short sentence or spoken command
                    // before he starts pluggin away.
                    if( FOkToSpeak() ) // && RANDOM_LONG(0,1))
                    {
                        if( m_hEnemy )
                        {
                            if( m_hEnemy->IsPlayer() )
                            {
                                // player
                                sentences::g_Sentences.PlayRndSz( this, "FG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                            }
                            else if( auto monster = m_hEnemy->MyMonsterPointer(); monster && monster->HasAlienGibs() )
                            {
                                // monster
                                sentences::g_Sentences.PlayRndSz( this, "FG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                            }
                        }

                        JustSpoke();
                    }

                    if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
                    {
                        return GetScheduleOfType( SCHED_GRUNT_SUPPRESS );
                    }
                    else
                    {
                        return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
                    }
                }
            }

            return GetScheduleOfType( SCHED_SMALL_FLINCH );
        }

        else if( HasConditions( bits_COND_HEAVY_DAMAGE ) )
            return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
        // no ammo
        // Only if the grunt has a weapon
        else if( CanTakeCoverAndReload() && HasConditions( bits_COND_NO_AMMO_LOADED ) )
        {
            //!!!KELLY - this individual just realized he's out of bullet ammo.
            // He's going to try to find cover to run to and reload, but rarely, if
            // none is available, he'll drop and reload in the open here.
            return GetScheduleOfType( SCHED_GRUNT_COVER_AND_RELOAD );
        }

        // damaged just a little
        else if( HasConditions( bits_COND_LIGHT_DAMAGE ) )
        {
            // if hurt:
            // 90% chance of taking cover
            // 10% chance of flinch.
            int iPercent = RANDOM_LONG( 0, 99 );

            if( iPercent <= 90 && m_hEnemy != nullptr )
            {
                // only try to take cover if we actually have an enemy!

                //!!!KELLY - this grunt was hit and is going to run to cover.
                if( FOkToSpeak() ) // && RANDOM_LONG(0,1))
                {
                    // sentences::g_Sentences.PlayRndSz(this, "FG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
                    m_iSentence = HGRUNT_SENT_COVER;
                    // JustSpoke();
                }
                return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
            }
            else
            {
                return GetScheduleOfType( SCHED_SMALL_FLINCH );
            }
        }
        // can kick
        else if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
        {
            return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
        }
        // can grenade launch

        else if( CanUseGrenadeLauncher() && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
        {
            // shoot a grenade if you can
            return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
        }
        // can shoot
        else if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
        {
            if( InSquad() )
            {
                // if the enemy has eluded the squad and a squad member has just located the enemy
                // and the enemy does not see the squad member, issue a call to the squad to waste a
                // little time and give the player a chance to turn.
                if( MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
                {
                    MySquadLeader()->m_fEnemyEluded = false;
                    return GetScheduleOfType( SCHED_GRUNT_FOUND_ENEMY );
                }
            }

            if( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
            {
                // try to take an available ENGAGE slot
                return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
            }
            else if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
            {
                // throw a grenade if can and no engage slots are available
                return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
            }
            else
            {
                // hide!
                return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
            }
        }
        // can't see enemy
        else if( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
        {
            if( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
            {
                //!!!KELLY - this grunt is about to throw or fire a grenade at the player. Great place for "fire in the hole"  "frag out" etc
                if( FOkToSpeak() )
                {
                    sentences::g_Sentences.PlayRndSz( this, "FG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
            }
            else if( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
            {
                //!!!KELLY - grunt cannot see the enemy and has just decided to
                // charge the enemy's position.
                if( FOkToSpeak() ) // && RANDOM_LONG(0,1))
                {
                    // sentences::g_Sentences.PlayRndSz(this, "FG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
                    m_iSentence = HGRUNT_SENT_CHARGE;
                    // JustSpoke();
                }

                return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
            }
            else
            {
                //!!!KELLY - grunt is going to stay put for a couple seconds to see if
                // the enemy wanders back out into the open, or approaches the
                // grunt's covered position. Good place for a taunt, I guess?
                if( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
                {
                    sentences::g_Sentences.PlayRndSz( this, "FG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return GetScheduleOfType( SCHED_STANDOFF );
            }
        }

        // Only if not following a player
        if( !m_hTargetEnt || !m_hTargetEnt->IsPlayer() )
        {
            if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
            {
                return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
            }
        }

        // Don't fall through to idle schedules
        break;
    }

    case MONSTERSTATE_ALERT:
    case MONSTERSTATE_IDLE:
        if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
        {
            // flinch if hurt
            return GetScheduleOfType( SCHED_SMALL_FLINCH );
        }

        // if we're not waiting on a medic and we're hurt, call out for a medic
        if( !m_hWaitMedic && gpGlobals->time > m_flMedicWaitTime && pev->health <= 20.0 )
        {
            auto pMedic = MySquadMedic();

            if( !pMedic )
            {
                pMedic = FindSquadMedic( 1024 );
            }

            if( pMedic )
            {
                if( pMedic->pev->deadflag == DEAD_NO )
                {
                    AILogger->debug( "Injured Grunt found Medic" );

                    if( pMedic->HealMe( this ) )
                    {
                        AILogger->debug( "Injured Grunt called for Medic" );

                        EmitSound( CHAN_VOICE, "fgrunt/medic.wav", VOL_NORM, ATTN_NORM );

                        JustSpoke();
                        m_flMedicWaitTime = gpGlobals->time + 5.0;
                    }
                }
            }
        }

        if( m_hEnemy == nullptr && IsFollowing() )
        {
            if( !m_hTargetEnt->IsAlive() )
            {
                // UNDONE: Comment about the recently dead player here?
                StopFollowing( false );
                break;
            }
            else
            {
                if( HasConditions( bits_COND_CLIENT_PUSH ) )
                {
                    return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
                }
                return GetScheduleOfType( SCHED_TARGET_FACE );
            }
        }

        if( HasConditions( bits_COND_CLIENT_PUSH ) )
        {
            return GetScheduleOfType( SCHED_MOVE_AWAY );
        }

        // try to say something about smells
        TrySmellTalk();
        break;
    }

    // no special cases here, call the base class
    return COFSquadTalkMonster::GetSchedule();
}

const Schedule_t* CBaseHGruntAlly::GetScheduleOfType( int Type )
{
    switch ( Type )
    {
    case SCHED_TAKE_COVER_FROM_ENEMY:
    {
        if( InSquad() )
        {
            if( g_cfg.GetSkillLevel() == SkillLevel::Hard && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
            {
                if( FOkToSpeak() )
                {
                    sentences::g_Sentences.PlayRndSz( this, "FG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return slGruntAllyTossGrenadeCover;
            }
            else
            {
                return &slGruntAllyTakeCover[0];
            }
        }
        else
        {
            // if ( RANDOM_LONG(0,1) )
            //{
            return &slGruntAllyTakeCover[0];
            //}
            // else
            //{
            //    return &slGruntAllyGrenadeCover[ 0 ];
            //}
        }
    }
    case SCHED_TAKE_COVER_FROM_BEST_SOUND:
    {
        return &slGruntAllyTakeCoverFromBestSound[0];
    }
    case SCHED_GRUNT_TAKECOVER_FAILED:
    {
        if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
        {
            return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
        }

        return GetScheduleOfType( SCHED_FAIL );
    }
    break;
    case SCHED_GRUNT_ELOF_FAIL:
    {
        // human grunt is unable to move to a position that allows him to attack the enemy.
        return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
    }
    break;
    case SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE:
    {
        return &slGruntAllyEstablishLineOfFire[0];
    }
    break;
    case SCHED_RANGE_ATTACK1:
    {
        switch ( GetPreferredCombatPosture() )
        {
        case PostureType::Random:
            // randomly stand or crouch
            if( RANDOM_LONG( 0, 9 ) == 0 )
                m_fStanding = RANDOM_LONG( 0, 1 );
            break;

        case PostureType::Standing:
            m_fStanding = true;
            break;

        case PostureType::Crouching:
            m_fStanding = false;
            break;
        }

        if( m_fStanding )
            return &slGruntAllyRangeAttack1B[0];
        else
            return &slGruntAllyRangeAttack1A[0];
    }
    case SCHED_RANGE_ATTACK2:
    {
        return &slGruntAllyRangeAttack2[0];
    }
    case SCHED_COMBAT_FACE:
    {
        return &slGruntAllyCombatFace[0];
    }
    case SCHED_GRUNT_WAIT_FACE_ENEMY:
    {
        return &slGruntAllyWaitInCover[0];
    }
    case SCHED_GRUNT_SWEEP:
    {
        return &slGruntAllySweep[0];
    }
    case SCHED_GRUNT_COVER_AND_RELOAD:
    {
        return &slGruntAllyHideReload[0];
    }
    case SCHED_GRUNT_FOUND_ENEMY:
    {
        return &slGruntAllyFoundEnemy[0];
    }
    case SCHED_VICTORY_DANCE:
    {
        if( InSquad() )
        {
            if( !IsLeader() )
            {
                return &slGruntAllyFail[0];
            }
        }

        return &slGruntAllyVictoryDance[0];
    }

    case SCHED_GRUNT_SUPPRESS:
    {
        if( m_hEnemy->IsPlayer() && m_fFirstEncounter )
        {
            m_fFirstEncounter = false; // after first encounter, leader won't issue handsigns anymore when he has a new enemy
            return &slGruntAllySignalSuppress[0];
        }
        else
        {
            return &slGruntAllySuppress[0];
        }
    }
    case SCHED_FAIL:
    {
        if( m_hEnemy != nullptr )
        {
            // grunt has an enemy, so pick a different default fail schedule most likely to help recover.
            return &slGruntAllyCombatFail[0];
        }

        return &slGruntAllyFail[0];
    }
    case SCHED_GRUNT_REPEL:
    {
        if( pev->velocity.z > -128 )
            pev->velocity.z -= 32;
        return &slGruntAllyRepel[0];
    }
    case SCHED_GRUNT_REPEL_ATTACK:
    {
        if( pev->velocity.z > -128 )
            pev->velocity.z -= 32;
        return &slGruntAllyRepelAttack[0];
    }
    case SCHED_GRUNT_REPEL_LAND:
    {
        return &slGruntAllyRepelLand[0];
    }

    case SCHED_TARGET_CHASE:
        return slGruntAllyFollow;

    case SCHED_TARGET_FACE:
    {
        auto pSchedule = COFSquadTalkMonster::GetScheduleOfType( SCHED_TARGET_FACE );

        if( pSchedule == slIdleStand )
            return slGruntAllyFaceTarget;
        return pSchedule;
    }

    case SCHED_IDLE_STAND:
    {
        auto pSchedule = COFSquadTalkMonster::GetScheduleOfType( SCHED_IDLE_STAND );

        if( pSchedule == slIdleStand )
            return slGruntAllyIdleStand;
        return pSchedule;
    }

    case SCHED_CANT_FOLLOW:
        return &slGruntAllyFail[0];

    default:
    {
        return COFSquadTalkMonster::GetScheduleOfType( Type );
    }
    }
}

int CBaseHGruntAlly::ObjectCaps()
{
    return FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE;
}

void CBaseHGruntAlly::TalkInit()
{
    COFSquadTalkMonster::TalkInit();

    m_szGrp[TLK_ANSWER] = "FG_ANSWER";
    m_szGrp[TLK_QUESTION] = "FG_QUESTION";
    m_szGrp[TLK_IDLE] = "FG_IDLE";
    m_szGrp[TLK_STARE] = "FG_STARE";
    m_szGrp[TLK_STOP] = "FG_STOP";

    m_szGrp[TLK_NOSHOOT] = "FG_SCARED";
    m_szGrp[TLK_HELLO] = "FG_HELLO";

    m_szGrp[TLK_PLHURT1] = "!FG_CUREA";
    m_szGrp[TLK_PLHURT2] = "!FG_CUREB";
    m_szGrp[TLK_PLHURT3] = "!FG_CUREC";

    m_szGrp[TLK_PHELLO] = nullptr;          //"BA_PHELLO";        // UNDONE
    m_szGrp[TLK_PIDLE] = nullptr;          //"BA_PIDLE";            // UNDONE
    m_szGrp[TLK_PQUESTION] = "FG_PQUEST"; // UNDONE

    m_szGrp[TLK_SMELL] = "FG_SMELL";

    m_szGrp[TLK_WOUND] = "FG_WOUND";
    m_szGrp[TLK_MORTAL] = "FG_MORTAL";
}

void CBaseHGruntAlly::AlertSound()
{
    if( m_hEnemy && FOkToSpeak() )
    {
        PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_NORM );
    }
}

void CBaseHGruntAlly::DeclineFollowing()
{
    PlaySentence( "FG_POK", 2, VOL_NORM, ATTN_NORM );
}

bool CBaseHGruntAlly::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "head" ) )
    {
        m_iGruntHead = atoi( pkvd->szValue );
        return true;
    }

    return COFSquadTalkMonster::KeyValue( pkvd );
}

void CBaseHGruntAlly::Killed( CBaseEntity* attacker, int iGib )
{
    if( m_MonsterState != MONSTERSTATE_DEAD )
    {
        if( HasMemory( bits_MEMORY_SUSPICIOUS ) || IsFacing( attacker, pev->origin ) )
        {
            Remember( bits_MEMORY_PROVOKED );

            StopFollowing( true );
        }
    }

    if( m_hWaitMedic )
    {
        COFSquadTalkMonster* medic = m_hWaitMedic;
        if( DEAD_NO != medic->pev->deadflag )
            m_hWaitMedic = nullptr;
        else
            medic->HealMe( nullptr );
    }

    COFSquadTalkMonster::Killed( attacker, iGib );
}

BEGIN_DATAMAP( CBaseHGruntAllyRepel )
    DEFINE_FUNCTION( RepelUse ),
    DEFINE_FIELD( m_iGruntHead, FIELD_INTEGER ),
    DEFINE_FIELD( m_iszUse, FIELD_STRING ),
    DEFINE_FIELD( m_iszUnUse, FIELD_STRING ),
END_DATAMAP();

bool CBaseHGruntAllyRepel::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "head" ) )
    {
        m_iGruntHead = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "UseSentence" ) )
    {
        m_iszUse = ALLOC_STRING( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "UnUseSentence" ) )
    {
        m_iszUnUse = ALLOC_STRING( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

SpawnAction CBaseHGruntAllyRepel::Spawn()
{
    Precache();
    pev->solid = SOLID_NOT;

    SetUse( &CBaseHGruntAllyRepel::RepelUse );

    return SpawnAction::Spawn;
}

void CBaseHGruntAllyRepel::Precache()
{
    UTIL_PrecacheOther( GetMonsterClassname() );
    m_iSpriteTexture = PrecacheModel( "sprites/rope.spr" );
}

void CBaseHGruntAllyRepel::RepelUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    TraceResult tr;
    UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0 ), dont_ignore_monsters, edict(), &tr );
    /*
    if ( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP)
        return nullptr;
    */

    CBaseEntity* pEntity = Create( GetMonsterClassname(), pev->origin, pev->angles );
    auto pGrunt = static_cast<CBaseHGruntAlly*>( pEntity->MySquadTalkMonsterPointer() );

    if( pGrunt )
    {
        pGrunt->pev->weapons = pev->weapons;
        pGrunt->pev->netname = pev->netname;

        // Carry over these spawn flags
        pGrunt->pev->spawnflags |= pev->spawnflags & ( SF_MONSTER_WAIT_TILL_SEEN | SF_MONSTER_GAG | SF_MONSTER_HITMONSTERCLIP | SF_MONSTER_PRISONER | SF_SQUADMONSTER_LEADER | SF_MONSTER_PREDISASTER );

        pGrunt->m_iGruntHead = m_iGruntHead;

        if( !FStringNull( m_iszUse ) )
        {
            pGrunt->m_iszUse = m_iszUse;
        }

        if( !FStringNull( m_iszUnUse ) )
        {
            pGrunt->m_iszUnUse = m_iszUnUse;
        }

        // Run logic to set up body groups (Spawn was already called once by Create above)
        pGrunt->Spawn();

        pGrunt->pev->movetype = MOVETYPE_FLY;
        pGrunt->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
        pGrunt->SetActivity( ACT_GLIDE );
        // UNDONE: position?
        pGrunt->m_vecLastPosition = tr.vecEndPos;

        CBeam* pBeam = CBeam::BeamCreate( "sprites/rope.spr", 10 );
        pBeam->PointEntInit( pev->origin + Vector( 0, 0, 112 ), pGrunt->entindex() );
        pBeam->SetFlags( BEAM_FSOLID );
        pBeam->SetColor( 255, 255, 255 );
        pBeam->SetThink( &CBeam::SUB_Remove );
        pBeam->pev->nextthink = gpGlobals->time + -4096.0 * tr.flFraction / pGrunt->pev->velocity.z + 0.5;

        UTIL_Remove( this );
    }
}
