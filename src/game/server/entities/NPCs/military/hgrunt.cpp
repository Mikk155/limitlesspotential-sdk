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
#include "squadmonster.h"
#include "talkmonster.h"
#include "customentity.h"
#include "hgrunt.h"

int g_fGruntQuestion; //!< true if an idle grunt asked a question. Cleared when someone answers.

LINK_ENTITY_TO_CLASS( monster_human_grunt, CHGrunt );

BEGIN_DATAMAP( CHGrunt )
    DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
    DEFINE_FIELD( m_flNextPainTime, FIELD_TIME ),
    //    DEFINE_FIELD(m_flLastEnemySightTime, FIELD_TIME), // don't save, go to zero
    DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
    DEFINE_FIELD( m_fThrowGrenade, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fStanding, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fFirstEncounter, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_cClipSize, FIELD_INTEGER ),
    DEFINE_FIELD( m_voicePitch, FIELD_INTEGER ),
    //  DEFINE_FIELD(m_iBrassShell, FIELD_INTEGER),
    //  DEFINE_FIELD(m_iShotgunShell, FIELD_INTEGER),
    DEFINE_FIELD( m_iSentence, FIELD_INTEGER ),
END_DATAMAP();

const char* CHGrunt::pGruntSentences[] =
    {
        "HG_GREN",      //!< grenade scared grunt
        "HG_ALERT",      //!< sees player
        "HG_MONSTER", //!< sees monster
        "HG_COVER",      //!< running to cover
        "HG_THROW",      //!< about to throw grenade
        "HG_CHARGE",  //!< running out to get the enemy
        "HG_TAUNT",      //!< say rude things
};

void CHGrunt::OnCreate()
{
    CSquadMonster::OnCreate();

    pev->model = MAKE_STRING( "models/hgrunt.mdl" );

    SetClassification( "human_military" );
}

int& CHGrunt::GetGruntQuestion()
{
    return g_fGruntQuestion;
}

void CHGrunt::SpeakSentence()
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

Relationship CHGrunt::IRelationship( CBaseEntity* pTarget )
{
    if( pTarget->ClassnameIs( "monster_alien_grunt" ) || ( pTarget->ClassnameIs( "monster_gargantua" ) ) )
    {
        return Relationship::Nemesis;
    }

    return CSquadMonster::IRelationship( pTarget );
}

void CHGrunt::GibMonster()
{
    Vector vecGunPos;
    Vector vecGunAngles;

    if( GetBodygroup( HGruntBodyGroup::Weapons ) != HGruntWeapon::Blank )
    { // throw a gun if the grunt has one
        GetAttachment( 0, vecGunPos, vecGunAngles );

        CBaseEntity* pGun;
        if( FBitSet( pev->weapons, HGRUNT_SHOTGUN ) )
        {
            pGun = DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
        }
        else
        {
            pGun = DropItem( "weapon_9mmar", vecGunPos, vecGunAngles );
        }
        if( pGun )
        {
            pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
            pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
        }

        if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
        {
            pGun = DropItem( "ammo_argrenades", vecGunPos, vecGunAngles );
            if( pGun )
            {
                pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
                pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
            }
        }
    }

    CBaseMonster::GibMonster();
}

int CHGrunt::ISoundMask()
{
    return bits_SOUND_WORLD |
           bits_SOUND_COMBAT |
           bits_SOUND_PLAYER |
           bits_SOUND_DANGER;
}

bool CHGrunt::FOkToSpeak()
{
    // if someone else is talking, don't speak
    if( gpGlobals->time <= CTalkMonster::g_talkWaitTime )
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

void CHGrunt::JustSpoke()
{
    CTalkMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 2.0 );
    m_iSentence = HGRUNT_SENT_NONE;
}

void CHGrunt::PrescheduleThink()
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

bool CHGrunt::FCanCheckAttacks()
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

bool CHGrunt::CheckMeleeAttack1( float flDot, float flDist )
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

bool CHGrunt::CheckRangeAttack1( float flDot, float flDist )
{
    if( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 2048 && flDot >= 0.5 && NoFriendlyFire() )
    {
        TraceResult tr;

        if( !m_hEnemy->IsPlayer() && flDist <= 64 )
        {
            // kick nonclients, but don't shoot at them.
            return false;
        }

        Vector vecSrc = GetGunPosition();

        // verify that a bullet fired from the gun will hit the enemy before the world.
        UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget( vecSrc ), ignore_monsters, ignore_glass, edict(), &tr );

        if( tr.flFraction == 1.0 )
        {
            return true;
        }
    }

    return false;
}

bool CHGrunt::CheckRangeAttack2Core( float flDot, float flDist, float grenadeSpeed )
{
    if( !FBitSet( pev->weapons, ( HGRUNT_HANDGRENADE | HGRUNT_GRENADELAUNCHER ) ) )
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

    if( FBitSet( pev->weapons, HGRUNT_HANDGRENADE ) )
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
            vecTarget = vecTarget + ( ( vecTarget - pev->origin ).Length() / grenadeSpeed ) * m_hEnemy->pev->velocity;
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


    if( FBitSet( pev->weapons, HGRUNT_HANDGRENADE ) )
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
        Vector vecToss = VecCheckThrow( this, GetGunPosition(), vecTarget, grenadeSpeed, 0.5 );

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

bool CHGrunt::CheckRangeAttack2( float flDot, float flDist )
{
    return CheckRangeAttack2Core( flDot, flDist, g_cfg.GetValue<float>( "hgrunt_gspeed"sv, 800, this ) );
}

void CHGrunt::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    // check for helmet shot
    if( ptr->iHitgroup == 11 )
    {
        // make sure we're wearing one
        if( GetBodygroup( HGruntBodyGroup::Head ) == HGruntHead::Grunt && ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB ) ) != 0 )
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
    CSquadMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
}

bool CHGrunt::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    Forget( bits_MEMORY_INCOVER );

    return CSquadMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CHGrunt::SetYawSpeed()
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

void CHGrunt::IdleSound()
{
    int& question = GetGruntQuestion();

    if( FOkToSpeak() && ( 0 != question || RANDOM_LONG( 0, 1 ) ) )
    {
        if( 0 == question )
        {
            // ask question or make statement
            switch ( RANDOM_LONG( 0, 2 ) )
            {
            case 0: // check in
                sentences::g_Sentences.PlayRndSz( this, "HG_CHECK", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                question = 1;
                break;
            case 1: // question
                sentences::g_Sentences.PlayRndSz( this, "HG_QUEST", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                question = 2;
                break;
            case 2: // statement
                sentences::g_Sentences.PlayRndSz( this, "HG_IDLE", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            }
        }
        else
        {
            switch ( question )
            {
            case 1: // check in
                sentences::g_Sentences.PlayRndSz( this, "HG_CLEAR", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            case 2: // question
                sentences::g_Sentences.PlayRndSz( this, "HG_ANSWER", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
                break;
            }
            question = 0;
        }
        JustSpoke();
    }
}

void CHGrunt::CheckAmmo()
{
    if( m_cAmmoLoaded <= 0 )
    {
        SetConditions( bits_COND_NO_AMMO_LOADED );
    }
}

CBaseEntity* CHGrunt::Kick()
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

Vector CHGrunt::GetGunPosition()
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

void CHGrunt::Shoot( bool firstShotInBurst )
{
    if( m_hEnemy )
    {
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            Vector vecShootOrigin = GetGunPosition();
            Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

            UTIL_MakeVectors( pev->angles );

            Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
            EjectBrass( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
            FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_MP5 ); // shoot +-5 degrees

            pev->effects |= EF_MUZZLEFLASH;

            m_cAmmoLoaded--; // take away a bullet!

            Vector angDir = UTIL_VecToAngles( vecShootDir );
            SetBlending( 0, angDir.x );
        }
        else
        {
            // Check this so shotgunners don't shoot bursts if the animation happens to have the events
            if( firstShotInBurst )
            {
                Vector vecShootOrigin = GetGunPosition();
                Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

                UTIL_MakeVectors( pev->angles );

                Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
                EjectBrass( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iShotgunShell, TE_BOUNCE_SHOTSHELL );
                FireBullets( g_cfg.GetValue<float>( "hgrunt_pellets"sv, 6, this ), vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0 ); // shoot +-7.5 degrees

                pev->effects |= EF_MUZZLEFLASH;

                m_cAmmoLoaded--; // take away a bullet!

                Vector angDir = UTIL_VecToAngles( vecShootDir );
                SetBlending( 0, angDir.x );
            }
        }
    }

    if( firstShotInBurst )
    {
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            // the first round of the three round burst plays the sound and puts a sound in the world sound list.
            if( RANDOM_LONG( 0, 1 ) )
            {
                EmitSound( CHAN_WEAPON, "hgrunt/gr_mgun1.wav", 1, ATTN_NORM );
            }
            else
            {
                EmitSound( CHAN_WEAPON, "hgrunt/gr_mgun2.wav", 1, ATTN_NORM );
            }
        }
        else
        {
            EmitSound( CHAN_WEAPON, "weapons/sbarrel1.wav", 1, ATTN_NORM );
        }

        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
    }
}

void CHGrunt::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case HGRUNT_AE_DROP_GUN:
    {
        if( GetBodygroup( HGruntBodyGroup::Weapons ) != HGruntWeapon::Blank )
        {
            Vector vecGunPos;
            Vector vecGunAngles;

            GetAttachment( 0, vecGunPos, vecGunAngles );

            // switch to body group with no gun.
            SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::Blank );

            // now spawn a gun.
            if( FBitSet( pev->weapons, HGRUNT_SHOTGUN ) )
            {
                DropItem( "weapon_shotgun", vecGunPos, vecGunAngles );
            }
            else
            {
                DropItem( "weapon_9mmar", vecGunPos, vecGunAngles );
            }
            if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
            {
                DropItem( "ammo_argrenades", BodyTarget( pev->origin ), vecGunAngles );
            }
        }
    }
    break;

    case HGRUNT_AE_RELOAD:
        EmitSound( CHAN_WEAPON, "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
        m_cAmmoLoaded = m_cClipSize;
        ClearConditions( bits_COND_NO_AMMO_LOADED );
        break;

    case HGRUNT_AE_GREN_TOSS:
    {
        UTIL_MakeVectors( pev->angles );
        // CGrenade::ShootTimed(this, pev->origin + gpGlobals->v_forward * 34 + Vector (0, 0, 32), m_vecTossVelocity, 3.5);
        CGrenade::ShootTimed( this, GetGunPosition(), m_vecTossVelocity, 3.5 );

        m_fThrowGrenade = false;
        m_flNextGrenadeCheck = gpGlobals->time + 6; // wait six seconds before even looking again to see if a grenade can be thrown.
                                                    // !!!LATER - when in a group, only try to throw grenade if ordered.
    }
    break;

    case HGRUNT_AE_GREN_LAUNCH:
    {
        EmitSound( CHAN_WEAPON, "weapons/glauncher.wav", 0.8, ATTN_NORM );
        CGrenade::ShootContact( this, GetGunPosition(), m_vecTossVelocity );
        m_fThrowGrenade = false;
        if( g_cfg.GetSkillLevel() == SkillLevel::Hard )
            m_flNextGrenadeCheck = gpGlobals->time + RANDOM_FLOAT( 2, 5 ); // wait a random amount of time before shooting again
        else
            m_flNextGrenadeCheck = gpGlobals->time + 6; // wait six seconds before even looking again to see if a grenade can be thrown.
    }
    break;

    case HGRUNT_AE_GREN_DROP:
    {
        UTIL_MakeVectors( pev->angles );
        CGrenade::ShootTimed( this, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6, g_vecZero, 3 );
    }
    break;

    case HGRUNT_AE_BURST1:
    case HGRUNT_AE_BURST2:
    case HGRUNT_AE_BURST3:
        Shoot( pEvent->event == HGRUNT_AE_BURST1 );
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
            pHurt->TakeDamage( this, this, g_cfg.GetValue<float>( "hgrunt_kick"sv, 10, this ), DMG_CLUB );
        }
    }
    break;

    case HGRUNT_AE_CAUGHT_ENEMY:
    {
        if( FOkToSpeak() )
        {
            sentences::g_Sentences.PlayRndSz( this, "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
            JustSpoke();
        }
    }
    break;

    default:
        CSquadMonster::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CHGrunt::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "hgrunt_health"sv, 80, this );

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

    m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

    m_fEnemyEluded = false;
    m_fFirstEncounter = true; // this is true when the grunt spawns, because he hasn't encountered an enemy yet.

    m_HackedGunPos = Vector( 0, 0, 55 );

    if( pev->weapons == 0 )
    {
        // initialize to original values
        pev->weapons = HGRUNT_9MMAR | HGRUNT_HANDGRENADE;
        // pev->weapons = HGRUNT_SHOTGUN;
        // pev->weapons = HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER;
    }

    if( FBitSet( pev->weapons, HGRUNT_SHOTGUN ) )
    {
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::Shotgun );
        m_cClipSize = 8;
    }
    else
    {
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::MP5 );
        m_cClipSize = GRUNT_CLIP_SIZE;
    }
    m_cAmmoLoaded = m_cClipSize;

    if( RANDOM_LONG( 0, 99 ) < 80 )
        pev->skin = 0; // light skin
    else
        pev->skin = 1; // dark skin

    if( FBitSet( pev->weapons, HGRUNT_SHOTGUN ) )
    {
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::Shotgun );
    }
    else if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
    {
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::M203 );
        pev->skin = 1; // alway dark skin
    }

    CTalkMonster::g_talkWaitTime = 0;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CHGrunt::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PrecacheSound( "hgrunt/gr_mgun1.wav" );
    PrecacheSound( "hgrunt/gr_mgun2.wav" );

    PrecacheSound( "hgrunt/gr_die1.wav" );
    PrecacheSound( "hgrunt/gr_die2.wav" );
    PrecacheSound( "hgrunt/gr_die3.wav" );

    PrecacheSound( "hgrunt/gr_pain1.wav" );
    PrecacheSound( "hgrunt/gr_pain2.wav" );
    PrecacheSound( "hgrunt/gr_pain3.wav" );
    PrecacheSound( "hgrunt/gr_pain4.wav" );
    PrecacheSound( "hgrunt/gr_pain5.wav" );

    PrecacheSound( "hgrunt/gr_reload1.wav" );

    PrecacheSound( "weapons/glauncher.wav" );

    PrecacheSound( "weapons/sbarrel1.wav" );

    PrecacheSound( "zombie/claw_miss2.wav" ); // because we use the basemonster SWIPE animation event

    // get voice pitch
    if( RANDOM_LONG( 0, 1 ) )
        m_voicePitch = 109 + RANDOM_LONG( 0, 7 );
    else
        m_voicePitch = 100;

    m_iBrassShell = PrecacheModel( "models/shell.mdl" ); // brass shell
    m_iShotgunShell = PrecacheModel( "models/shotgunshell.mdl" );
}

void CHGrunt::StartTask( const Task_t* pTask )
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
        CSquadMonster::StartTask( pTask );
        break;

    case TASK_RELOAD:
        m_IdealActivity = ACT_RELOAD;
        break;

    case TASK_GRUNT_FACE_TOSS_DIR:
        break;

    case TASK_FACE_IDEAL:
    case TASK_FACE_ENEMY:
        CSquadMonster::StartTask( pTask );
        if( pev->movetype == MOVETYPE_FLY )
        {
            m_IdealActivity = ACT_GLIDE;
        }
        break;

    default:
        CSquadMonster::StartTask( pTask );
        break;
    }
}

void CHGrunt::RunTask( const Task_t* pTask )
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
        CSquadMonster::RunTask( pTask );
        break;
    }
    }
}

void CHGrunt::PainSound()
{
    if( gpGlobals->time > m_flNextPainTime )
    {
#if 0
        if( RANDOM_LONG( 0, 99 ) < 5 )
        {
            // pain sentences are rare
            if( FOkToSpeak() )
            {
                sentences::g_Sentences.PlayRndSz( this, "HG_PAIN", HGRUNT_SENTENCE_VOLUME, ATTN_NORM, 0, PITCH_NORM );
                JustSpoke();
                return;
            }
        }
#endif
        switch ( RANDOM_LONG( 0, 6 ) )
        {
        case 0:
            EmitSound( CHAN_VOICE, "hgrunt/gr_pain3.wav", 1, ATTN_NORM );
            break;
        case 1:
            EmitSound( CHAN_VOICE, "hgrunt/gr_pain4.wav", 1, ATTN_NORM );
            break;
        case 2:
            EmitSound( CHAN_VOICE, "hgrunt/gr_pain5.wav", 1, ATTN_NORM );
            break;
        case 3:
            EmitSound( CHAN_VOICE, "hgrunt/gr_pain1.wav", 1, ATTN_NORM );
            break;
        case 4:
            EmitSound( CHAN_VOICE, "hgrunt/gr_pain2.wav", 1, ATTN_NORM );
            break;
        }

        m_flNextPainTime = gpGlobals->time + 1;
    }
}

void CHGrunt::DeathSound()
{
    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "hgrunt/gr_die1.wav", 1, ATTN_IDLE );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "hgrunt/gr_die2.wav", 1, ATTN_IDLE );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "hgrunt/gr_die3.wav", 1, ATTN_IDLE );
        break;
    }
}

Task_t tlGruntFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slGruntFail[] =
    {
        {tlGruntFail,
            std::size( tlGruntFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2 |
                bits_COND_CAN_MELEE_ATTACK1 |
                bits_COND_CAN_MELEE_ATTACK2,
            0,
            "Grunt Fail"},
};

Task_t tlGruntCombatFail[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)2},
        {TASK_WAIT_PVS, (float)0},
};

Schedule_t slGruntCombatFail[] =
    {
        {tlGruntCombatFail,
            std::size( tlGruntCombatFail ),
            bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2,
            0,
            "Grunt Combat Fail"},
};

Task_t tlGruntVictoryDance[] =
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

Schedule_t slGruntVictoryDance[] =
    {
        {tlGruntVictoryDance,
            std::size( tlGruntVictoryDance ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "GruntVictoryDance"},
};

Task_t tlGruntEstablishLineOfFire[] =
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
Schedule_t slGruntEstablishLineOfFire[] =
    {
        {tlGruntEstablishLineOfFire,
            std::size( tlGruntEstablishLineOfFire ),
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

Task_t tlGruntFoundEnemy[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE_FACE_ENEMY, (float)ACT_SIGNAL1},
};

/**
 *    @brief grunt established sight with an enemy that was hiding from the squad.
 */
Schedule_t slGruntFoundEnemy[] =
    {
        {tlGruntFoundEnemy,
            std::size( tlGruntFoundEnemy ),
            bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "GruntFoundEnemy"},
};

Task_t tlGruntCombatFace1[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_WAIT, (float)1.5},
        {TASK_SET_SCHEDULE, (float)SCHED_GRUNT_SWEEP},
};

Schedule_t slGruntCombatFace[] =
    {
        {tlGruntCombatFace1,
            std::size( tlGruntCombatFace1 ),
            bits_COND_NEW_ENEMY |
                bits_COND_ENEMY_DEAD |
                bits_COND_CAN_RANGE_ATTACK1 |
                bits_COND_CAN_RANGE_ATTACK2,
            0,
            "Combat Face"},
};

Task_t tlGruntSignalSuppress[] =
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
Schedule_t slGruntSignalSuppress[] =
    {
        {tlGruntSignalSuppress,
            std::size( tlGruntSignalSuppress ),
            bits_COND_ENEMY_DEAD |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_NO_AMMO_LOADED,

            bits_SOUND_DANGER,
            "SignalSuppress"},
};

Task_t tlGruntSuppress[] =
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

Schedule_t slGruntSuppress[] =
    {
        {tlGruntSuppress,
            std::size( tlGruntSuppress ),
            bits_COND_ENEMY_DEAD |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND |
                bits_COND_GRUNT_NOFIRE |
                bits_COND_NO_AMMO_LOADED,

            bits_SOUND_DANGER,
            "Suppress"},
};

Task_t tlGruntWaitInCover[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_FACE_ENEMY, (float)1},
};

/**
 *    @brief we don't allow danger or the ability to attack to break a grunt's run to cover schedule,
 *    but when a grunt is in cover, we do want them to attack if they can.
 */
Schedule_t slGruntWaitInCover[] =
    {
        {tlGruntWaitInCover,
            std::size( tlGruntWaitInCover ),
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
Task_t tlGruntTakeCover1[] =
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

Schedule_t slGruntTakeCover[] =
    {
        {tlGruntTakeCover1,
            std::size( tlGruntTakeCover1 ),
            0,
            0,
            "TakeCover"},
};

Task_t tlGruntGrenadeCover1[] =
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
Schedule_t slGruntGrenadeCover[] =
    {
        {tlGruntGrenadeCover1,
            std::size( tlGruntGrenadeCover1 ),
            0,
            0,
            "GrenadeCover"},
};

Task_t tlGruntTossGrenadeCover1[] =
    {
        {TASK_FACE_ENEMY, (float)0},
        {TASK_RANGE_ATTACK2, (float)0},
        {TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ENEMY},
};

/**
 *    @brief drop grenade then run to cover.
 */
Schedule_t slGruntTossGrenadeCover[] =
    {
        {tlGruntTossGrenadeCover1,
            std::size( tlGruntTossGrenadeCover1 ),
            0,
            0,
            "TossGrenadeCover"},
};

Task_t tlGruntTakeCoverFromBestSound[] =
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
Schedule_t slGruntTakeCoverFromBestSound[] =
    {
        {tlGruntTakeCoverFromBestSound,
            std::size( tlGruntTakeCoverFromBestSound ),
            0,
            0,
            "GruntTakeCoverFromBestSound"},
};

Task_t tlGruntHideReload[] =
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

Schedule_t slGruntHideReload[] =
    {
        {tlGruntHideReload,
            std::size( tlGruntHideReload ),
            bits_COND_HEAVY_DAMAGE |
                bits_COND_HEAR_SOUND,

            bits_SOUND_DANGER,
            "GruntHideReload"}};

Task_t tlGruntSweep[] =
    {
        {TASK_TURN_LEFT, (float)179},
        {TASK_WAIT, (float)1},
        {TASK_TURN_LEFT, (float)179},
        {TASK_WAIT, (float)1},
};

/**
 *    @brief Do a turning sweep of the area
 */
Schedule_t slGruntSweep[] =
    {
        {tlGruntSweep,
            std::size( tlGruntSweep ),

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

Task_t tlGruntRangeAttack1A[] =
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
Schedule_t slGruntRangeAttack1A[] =
    {
        {tlGruntRangeAttack1A,
            std::size( tlGruntRangeAttack1A ),
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

Task_t tlGruntRangeAttack1B[] =
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
Schedule_t slGruntRangeAttack1B[] =
    {
        {tlGruntRangeAttack1B,
            std::size( tlGruntRangeAttack1B ),
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

Task_t tlGruntRangeAttack2[] =
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
Schedule_t slGruntRangeAttack2[] =
    {
        {tlGruntRangeAttack2,
            std::size( tlGruntRangeAttack2 ),
            0,
            0,
            "RangeAttack2"},
};

Task_t tlGruntRepel[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_GLIDE},
};

Schedule_t slGruntRepel[] =
    {
        {tlGruntRepel,
            std::size( tlGruntRepel ),
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

Task_t tlGruntRepelAttack[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_FLY},
};

Schedule_t slGruntRepelAttack[] =
    {
        {tlGruntRepelAttack,
            std::size( tlGruntRepelAttack ),
            bits_COND_ENEMY_OCCLUDED,
            0,
            "Repel Attack"},
};

Task_t tlGruntRepelLand[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_LAND},
        {TASK_GET_PATH_TO_LASTPOSITION, (float)0},
        {TASK_RUN_PATH, (float)0},
        {TASK_WAIT_FOR_MOVEMENT, (float)0},
        {TASK_CLEAR_LASTPOSITION, (float)0},
};

Schedule_t slGruntRepelLand[] =
    {
        {tlGruntRepelLand,
            std::size( tlGruntRepelLand ),
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

BEGIN_CUSTOM_SCHEDULES( CHGrunt )
slGruntFail,
    slGruntCombatFail,
    slGruntVictoryDance,
    slGruntEstablishLineOfFire,
    slGruntFoundEnemy,
    slGruntCombatFace,
    slGruntSignalSuppress,
    slGruntSuppress,
    slGruntWaitInCover,
    slGruntTakeCover,
    slGruntGrenadeCover,
    slGruntTossGrenadeCover,
    slGruntTakeCoverFromBestSound,
    slGruntHideReload,
    slGruntSweep,
    slGruntRangeAttack1A,
    slGruntRangeAttack1B,
    slGruntRangeAttack2,
    slGruntRepel,
    slGruntRepelAttack,
    slGruntRepelLand
    END_CUSTOM_SCHEDULES();

std::tuple<int, Activity> CHGrunt::GetSequenceForActivity( Activity NewActivity )
{
    int iSequence = ACTIVITY_NOT_AVAILABLE;

    switch ( NewActivity )
    {
    case ACT_RANGE_ATTACK1:
        // grunt is either shooting standing or shooting crouched
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            if( m_fStanding )
            {
                // get aimable sequence
                iSequence = LookupSequence( "standing_mp5" );
            }
            else
            {
                // get crouching shoot
                iSequence = LookupSequence( "crouching_mp5" );
            }
        }
        else
        {
            if( m_fStanding )
            {
                // get aimable sequence
                iSequence = LookupSequence( "standing_shotgun" );
            }
            else
            {
                // get crouching shoot
                iSequence = LookupSequence( "crouching_shotgun" );
            }
        }
        break;
    case ACT_RANGE_ATTACK2:
        // grunt is going to a secondary long range attack. This may be a thrown
        // grenade or fired grenade, we must determine which and pick proper sequence
        if( ( pev->weapons & HGRUNT_HANDGRENADE ) != 0 )
        {
            // get toss anim
            iSequence = LookupSequence( "throwgrenade" );
        }
        else
        {
            // get launch anim
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

void CHGrunt::SetActivity( Activity NewActivity )
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

const Schedule_t* CHGrunt::GetSchedule()
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
                    sentences::g_Sentences.PlayRndSz( this, "HG_GREN", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
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
            // call base class, all code to handle dead enemies is centralized there.
            return CBaseMonster::GetSchedule();
        }

        // new enemy
        if( HasConditions( bits_COND_NEW_ENEMY ) )
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
                                sentences::g_Sentences.PlayRndSz( this, "HG_ALERT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                            }
                            else if( auto monster = m_hEnemy->MyMonsterPointer(); monster && monster->HasAlienGibs() )
                            {
                                // monster
                                sentences::g_Sentences.PlayRndSz( this, "HG_MONST", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
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
        }
        // no ammo
        else if( HasConditions( bits_COND_NO_AMMO_LOADED ) )
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
                    // sentences::g_Sentences.PlayRndSz(this, "HG_COVER", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
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

        else if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
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
                    sentences::g_Sentences.PlayRndSz( this, "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
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
                    // sentences::g_Sentences.PlayRndSz(this, "HG_CHARGE", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
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
                    sentences::g_Sentences.PlayRndSz( this, "HG_TAUNT", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return GetScheduleOfType( SCHED_STANDOFF );
            }
        }

        if( HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
        {
            return GetScheduleOfType( SCHED_GRUNT_ESTABLISH_LINE_OF_FIRE );
        }
    }
    }

    // no special cases here, call the base class
    return CSquadMonster::GetSchedule();
}

const Schedule_t* CHGrunt::GetScheduleOfType( int Type )
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
                    sentences::g_Sentences.PlayRndSz( this, "HG_THROW", HGRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch );
                    JustSpoke();
                }
                return slGruntTossGrenadeCover;
            }
            else
            {
                return &slGruntTakeCover[0];
            }
        }
        else
        {
            if( RANDOM_LONG( 0, 1 ) )
            {
                return &slGruntTakeCover[0];
            }
            else
            {
                return &slGruntGrenadeCover[0];
            }
        }
    }
    case SCHED_TAKE_COVER_FROM_BEST_SOUND:
    {
        return &slGruntTakeCoverFromBestSound[0];
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
        return &slGruntEstablishLineOfFire[0];
    }
    break;
    case SCHED_RANGE_ATTACK1:
    {
        // randomly stand or crouch
        if( RANDOM_LONG( 0, 9 ) == 0 )
            m_fStanding = RANDOM_LONG( 0, 1 );

        if( m_fStanding )
            return &slGruntRangeAttack1B[0];
        else
            return &slGruntRangeAttack1A[0];
    }
    case SCHED_RANGE_ATTACK2:
    {
        return &slGruntRangeAttack2[0];
    }
    case SCHED_COMBAT_FACE:
    {
        return &slGruntCombatFace[0];
    }
    case SCHED_GRUNT_WAIT_FACE_ENEMY:
    {
        return &slGruntWaitInCover[0];
    }
    case SCHED_GRUNT_SWEEP:
    {
        return &slGruntSweep[0];
    }
    case SCHED_GRUNT_COVER_AND_RELOAD:
    {
        return &slGruntHideReload[0];
    }
    case SCHED_GRUNT_FOUND_ENEMY:
    {
        return &slGruntFoundEnemy[0];
    }
    case SCHED_VICTORY_DANCE:
    {
        if( InSquad() )
        {
            if( !IsLeader() )
            {
                return &slGruntFail[0];
            }
        }

        return &slGruntVictoryDance[0];
    }
    case SCHED_GRUNT_SUPPRESS:
    {
        if( m_hEnemy->IsPlayer() && m_fFirstEncounter )
        {
            m_fFirstEncounter = false; // after first encounter, leader won't issue handsigns anymore when he has a new enemy
            return &slGruntSignalSuppress[0];
        }
        else
        {
            return &slGruntSuppress[0];
        }
    }
    case SCHED_FAIL:
    {
        if( m_hEnemy != nullptr )
        {
            // grunt has an enemy, so pick a different default fail schedule most likely to help recover.
            return &slGruntCombatFail[0];
        }

        return &slGruntFail[0];
    }
    case SCHED_GRUNT_REPEL:
    {
        if( pev->velocity.z > -128 )
            pev->velocity.z -= 32;
        return &slGruntRepel[0];
    }
    case SCHED_GRUNT_REPEL_ATTACK:
    {
        if( pev->velocity.z > -128 )
            pev->velocity.z -= 32;
        return &slGruntRepelAttack[0];
    }
    case SCHED_GRUNT_REPEL_LAND:
    {
        return &slGruntRepelLand[0];
    }
    default:
    {
        return CSquadMonster::GetScheduleOfType( Type );
    }
    }
}

BEGIN_DATAMAP( CHGruntRepel )
    DEFINE_FUNCTION( RepelUse ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( monster_grunt_repel, CHGruntRepel );

SpawnAction CHGruntRepel::Spawn()
{
    Precache();
    pev->solid = SOLID_NOT;

    SetUse( &CHGruntRepel::RepelUse );

    return SpawnAction::Spawn;
}

void CHGruntRepel::PrecacheCore( const char* classname )
{
    UTIL_PrecacheOther( classname );
    m_iSpriteTexture = PrecacheModel( "sprites/rope.spr" );
}

void CHGruntRepel::Precache()
{
    PrecacheCore( "monster_human_grunt" );
}

void CHGruntRepel::CreateMonster( const char* classname )
{
    TraceResult tr;
    UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096.0 ), dont_ignore_monsters, edict(), &tr );
    /*
    if ( tr.pHit && Instance( tr.pHit )->pev->solid != SOLID_BSP)
        return nullptr;
    */

    CBaseEntity* pEntity = Create( classname, pev->origin, pev->angles );
    CBaseMonster* pGrunt = pEntity->MyMonsterPointer();
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

void CHGruntRepel::RepelUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    CreateMonster( "monster_human_grunt" );
}

void CDeadHGrunt::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8;
    pev->model = MAKE_STRING( "models/hgrunt.mdl" );

    SetClassification( "human_military" );
}

bool CDeadHGrunt::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_hgrunt_dead, CDeadHGrunt );

void CDeadHGrunt::SpawnCore()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    pev->effects = 0;
    pev->yaw_speed = 8;
    pev->sequence = 0;
    m_bloodColor = BLOOD_COLOR_RED;

    pev->sequence = LookupSequence( m_szPoses[m_iPose] );

    if( pev->sequence == -1 )
    {
        AILogger->debug( "Dead hgrunt with bad pose" );
    }

    MonsterInitDead();
}

SpawnAction CDeadHGrunt::Spawn()
{
    SpawnCore();

    // map old bodies onto new bodies
    switch ( pev->body )
    {
    case 0: // Grunt with Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::Grunt );
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::MP5 );
        break;
    case 1: // Commander with Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::Commander );
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::MP5 );
        break;
    case 2: // Grunt no Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::Grunt );
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::Blank );
        break;
    case 3: // Commander no Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( HGruntBodyGroup::Head, HGruntHead::Commander );
        SetBodygroup( HGruntBodyGroup::Weapons, HGruntWeapon::Blank );
        break;
    }

    return SpawnAction::Spawn;
}
