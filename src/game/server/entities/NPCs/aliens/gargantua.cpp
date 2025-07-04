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
#include "customentity.h"
#include "explode.h"
#include "func_break.h"

const float GARG_ATTACKDIST = 80.0;

// Garg animation events
#define GARG_AE_SLASH_LEFT 1
// #define GARG_AE_BEAM_ATTACK_RIGHT    2        // No longer used
#define GARG_AE_LEFT_FOOT 3
#define GARG_AE_RIGHT_FOOT 4
#define GARG_AE_STOMP 5
#define GARG_AE_BREATHE 6

// Gargantua is immune to any damage but this
#define GARG_DAMAGE (DMG_ENERGYBEAM | DMG_CRUSH | DMG_MORTAR | DMG_BLAST)
#define GARG_EYE_SPRITE_NAME "sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME "sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2 "sprites/xbeam3.spr"
#define GARG_STOMP_SPRITE_NAME "sprites/gargeye1.spr"
#define GARG_STOMP_BUZZ_SOUND "weapons/mine_charge.wav"
#define GARG_FLAME_LENGTH 330
#define GARG_GIB_MODEL "models/metalplategibs.mdl"

#define ATTN_GARG (ATTN_NORM)

#define STOMP_SPRITE_COUNT 10

int gStompSprite = 0, gGargGibModel = 0;
void SpawnExplosion( Vector center, float randomRange, float time, int magnitude );

class CSmoker;

class CSpiral : public CBaseEntity
{
public:
    SpawnAction Spawn() override;
    void Think() override;
    int ObjectCaps() override { return FCAP_DONT_SAVE; }
    static CSpiral* Create( const Vector& origin, float height, float radius, float duration, CBaseEntity* owner );
};

LINK_ENTITY_TO_CLASS( streak_spiral, CSpiral );

class CStomp : public CBaseEntity
{
    DECLARE_CLASS( CStomp, CBaseEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Think() override;
    static CStomp* StompCreate( const Vector& origin, const Vector& end, float speed, CBaseEntity* owner );

    float m_flLastThinkTime;

private:
    // UNDONE: re-use this sprite list instead of creating new ones all the time
    //    CSprite        *m_pSprites[ STOMP_SPRITE_COUNT ];
};

LINK_ENTITY_TO_CLASS( garg_stomp, CStomp );

BEGIN_DATAMAP( CStomp )
    DEFINE_FIELD( m_flLastThinkTime, FIELD_TIME ),
END_DATAMAP();

CStomp* CStomp::StompCreate( const Vector& origin, const Vector& end, float speed, CBaseEntity* owner )
{
    CStomp* pStomp = g_EntityDictionary->Create<CStomp>( "garg_stomp" );

    if( owner != nullptr && owner->m_config >= 0 )
        pStomp->m_config = owner->m_config;

    pStomp->pev->origin = origin;
    Vector dir = ( end - origin );
    pStomp->pev->scale = dir.Length();
    pStomp->pev->movedir = dir.Normalize();
    pStomp->pev->speed = speed;
    pStomp->Spawn();

    return pStomp;
}

SpawnAction CStomp::Spawn()
{
    pev->nextthink = gpGlobals->time;
    pev->dmgtime = gpGlobals->time;

    pev->framerate = 30;
    pev->model = MAKE_STRING( GARG_STOMP_SPRITE_NAME );
    pev->rendermode = kRenderTransTexture;
    pev->renderamt = 0;
    EmitSoundDyn( CHAN_BODY, GARG_STOMP_BUZZ_SOUND, 1, ATTN_NORM, 0, PITCH_NORM * 0.55 );

    return SpawnAction::Spawn;
}

#define STOMP_INTERVAL 0.025

void CStomp::Think()
{
    if( m_flLastThinkTime == 0 )
    {
        m_flLastThinkTime = gpGlobals->time - gpGlobals->frametime;
    }

    // Use 1/4th the delta time to match the original behavior more closely
    const float deltaTime = ( gpGlobals->time - m_flLastThinkTime ) / 4;

    m_flLastThinkTime = gpGlobals->time;

    TraceResult tr;

    pev->nextthink = gpGlobals->time + 0.1;

    // Do damage for this frame
    Vector vecStart = pev->origin;
    vecStart.z += 30;
    Vector vecEnd = vecStart + ( pev->movedir * pev->speed * deltaTime );

    UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, edict(), &tr );

    if( tr.pHit && tr.pHit != pev->owner )
    {
        CBaseEntity* pEntity = CBaseEntity::Instance( tr.pHit );
        auto owner = GetOwner();
        if( !owner )
            owner = this;

        if( pEntity )
            pEntity->TakeDamage( this, owner, g_cfg.GetValue<float>( "gargantua_dmg_stomp"sv, 100, this ), DMG_SONIC );
    }

    // Accelerate the effect
    pev->speed = pev->speed + deltaTime * pev->framerate;
    pev->framerate = pev->framerate + deltaTime * 1500;

    // Move and spawn trails
    while( gpGlobals->time - pev->dmgtime > STOMP_INTERVAL )
    {
        pev->origin = pev->origin + pev->movedir * pev->speed * STOMP_INTERVAL;
        for( int i = 0; i < 2; i++ )
        {
            CSprite* pSprite = CSprite::SpriteCreate( GARG_STOMP_SPRITE_NAME, pev->origin, true );
            if( pSprite )
            {
                UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 500 ), ignore_monsters, edict(), &tr );
                pSprite->pev->origin = tr.vecEndPos;
                pSprite->pev->velocity = Vector( RANDOM_FLOAT( -200, 200 ), RANDOM_FLOAT( -200, 200 ), 175 );
                // pSprite->AnimateAndDie( RANDOM_FLOAT( 8.0, 12.0 ) );
                pSprite->pev->nextthink = gpGlobals->time + 0.3;
                pSprite->SetThink( &CSprite::SUB_Remove );
                pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxFadeFast );
            }
        }
        pev->dmgtime += STOMP_INTERVAL;
        // Scale has the "life" of this effect
        pev->scale -= STOMP_INTERVAL * pev->speed;
        if( pev->scale <= 0 )
        {
            // Life has run out
            UTIL_Remove( this );
            StopSound( CHAN_BODY, GARG_STOMP_BUZZ_SOUND );
        }
    }
}

void StreakSplash( const Vector& origin, const Vector& direction, int color, int count, int speed, int velocityRange )
{
    MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
    WRITE_BYTE( TE_STREAK_SPLASH );
    WRITE_COORD( origin.x ); // origin
    WRITE_COORD( origin.y );
    WRITE_COORD( origin.z );
    WRITE_COORD( direction.x ); // direction
    WRITE_COORD( direction.y );
    WRITE_COORD( direction.z );
    WRITE_BYTE( color );    // Streak color 6
    WRITE_SHORT( count ); // count
    WRITE_SHORT( speed );
    WRITE_SHORT( velocityRange ); // Random velocity modifier
    MESSAGE_END();
}

class CGargantua : public CBaseMonster
{
    DECLARE_CLASS( CGargantua, CBaseMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SetYawSpeed() override;
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override;
    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;

    bool HasAlienGibs() override { return true; }

    bool CheckMeleeAttack1( float flDot, float flDist ) override; //!< Swipe
    bool CheckMeleeAttack2( float flDot, float flDist ) override; //!< Flames
    bool CheckRangeAttack1( float flDot, float flDist ) override; //!< Stomp attack
    void SetObjectCollisionBox() override
    {
        pev->absmin = pev->origin + Vector( -80, -80, 0 );
        pev->absmax = pev->origin + Vector( 80, 80, 214 );
    }

    const Schedule_t* GetScheduleOfType( int Type ) override;
    void StartTask( const Task_t* pTask ) override;
    void RunTask( const Task_t* pTask ) override;

    void PrescheduleThink() override;

    void Killed( CBaseEntity* attacker, int iGib ) override;
    void DeathEffect();

    void DestroyEffects();

    void UpdateOnRemove() override;

    void EyeOff();
    void EyeOn( int level );
    void EyeUpdate();
    void StompAttack();
    void FlameCreate();
    void FlameUpdate();
    void FlameControls( float angleX, float angleY );
    void FlameDestroy();
    inline bool FlameIsOn() { return m_pFlame[0] != nullptr; }

    void FlameDamage( Vector vecStart, Vector vecEnd, CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage,
        int bitsDamageType, EntityClassification iClassIgnore );

private:
    static const char* pAttackHitSounds[];
    static const char* pBeamAttackSounds[];
    static const char* pAttackMissSounds[];
    static const char* pRicSounds[];
    static const char* pFootSounds[];
    static const char* pIdleSounds[];
    static const char* pAlertSounds[];
    static const char* pPainSounds[];
    static const char* pAttackSounds[];
    static const char* pStompSounds[];
    static const char* pBreatheSounds[];

    /**
     *    @brief expects a length to trace, amount of damage to do, and damage type.
     *    Used for many contact-range melee attacks. Bites, claws, etc.
     *    @details Overridden for Gargantua because his swing starts lower as a percentage of his height
     *    (otherwise he swings over the players head)
     *    @return A pointer to the damaged entity in case the monster
     *        wishes to do other stuff to the victim (punchangle, etc)
     */
    CBaseEntity* GargantuaCheckTraceHullAttack( float flDist, int iDamage, int iDmgType );

    CSprite* m_pEyeGlow; // Glow around the eyes
    CBeam* m_pFlame[4];     // Flame beams

    int m_eyeBrightness;   // Brightness target
    float m_seeTime;       // Time to attack (when I see the enemy, I set this)
    float m_flameTime;       // Time of next flame attack
    float m_painSoundTime; // Time of next pain sound
    float m_streakTime;       // streak timer (don't send too many)
    float m_flameX;           // Flame thrower aim
    float m_flameY;
};

LINK_ENTITY_TO_CLASS( monster_gargantua, CGargantua );

BEGIN_DATAMAP( CGargantua )
    DEFINE_FIELD( m_pEyeGlow, FIELD_CLASSPTR ),
    DEFINE_FIELD( m_eyeBrightness, FIELD_INTEGER ),
    DEFINE_FIELD( m_seeTime, FIELD_TIME ),
    DEFINE_FIELD( m_flameTime, FIELD_TIME ),
    DEFINE_FIELD( m_streakTime, FIELD_TIME ),
    DEFINE_FIELD( m_painSoundTime, FIELD_TIME ),
    DEFINE_ARRAY( m_pFlame, FIELD_CLASSPTR, 4 ),
    DEFINE_FIELD( m_flameX, FIELD_FLOAT ),
    DEFINE_FIELD( m_flameY, FIELD_FLOAT ),
END_DATAMAP();

const char* CGargantua::pAttackHitSounds[] =
    {
        "zombie/claw_strike1.wav",
        "zombie/claw_strike2.wav",
        "zombie/claw_strike3.wav",
};

const char* CGargantua::pBeamAttackSounds[] =
    {
        "garg/gar_flameoff1.wav",
        "garg/gar_flameon1.wav",
        "garg/gar_flamerun1.wav",
};


const char* CGargantua::pAttackMissSounds[] =
    {
        "zombie/claw_miss1.wav",
        "zombie/claw_miss2.wav",
};

const char* CGargantua::pRicSounds[] =
    {
#if 0
    "weapons/ric1.wav",
    "weapons/ric2.wav",
    "weapons/ric3.wav",
    "weapons/ric4.wav",
    "weapons/ric5.wav",
#else
        "debris/metal4.wav",
        "debris/metal6.wav",
        "weapons/ric4.wav",
        "weapons/ric5.wav",
#endif
};

const char* CGargantua::pFootSounds[] =
    {
        "garg/gar_step1.wav",
        "garg/gar_step2.wav",
};


const char* CGargantua::pIdleSounds[] =
    {
        "garg/gar_idle1.wav",
        "garg/gar_idle2.wav",
        "garg/gar_idle3.wav",
        "garg/gar_idle4.wav",
        "garg/gar_idle5.wav",
};

const char* CGargantua::pAttackSounds[] =
    {
        "garg/gar_attack1.wav",
        "garg/gar_attack2.wav",
        "garg/gar_attack3.wav",
};

const char* CGargantua::pAlertSounds[] =
    {
        "garg/gar_alert1.wav",
        "garg/gar_alert2.wav",
        "garg/gar_alert3.wav",
};

const char* CGargantua::pPainSounds[] =
    {
        "garg/gar_pain1.wav",
        "garg/gar_pain2.wav",
        "garg/gar_pain3.wav",
};

const char* CGargantua::pStompSounds[] =
    {
        "garg/gar_stomp1.wav",
};

const char* CGargantua::pBreatheSounds[] =
    {
        "garg/gar_breathe1.wav",
        "garg/gar_breathe2.wav",
        "garg/gar_breathe3.wav",
};

#if 0
enum
{
    SCHED_ = LAST_COMMON_SCHEDULE + 1,
};
#endif

enum
{
    TASK_SOUND_ATTACK = LAST_COMMON_TASK + 1,
    TASK_FLAME_SWEEP,
};

Task_t tlGargFlame[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_SOUND_ATTACK, (float)0},
        // { TASK_PLAY_SEQUENCE,        (float)ACT_SIGNAL1    },
        {TASK_SET_ACTIVITY, (float)ACT_MELEE_ATTACK2},
        {TASK_FLAME_SWEEP, (float)4.5},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
};

Schedule_t slGargFlame[] =
    {
        {tlGargFlame,
            std::size( tlGargFlame ),
            0,
            0,
            "GargFlame"},
};

// primary melee attack
Task_t tlGargSwipe[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_ENEMY, (float)0},
        {TASK_MELEE_ATTACK1, (float)0},
};

Schedule_t slGargSwipe[] =
    {
        {tlGargSwipe,
            std::size( tlGargSwipe ),
            bits_COND_CAN_MELEE_ATTACK2,
            0,
            "GargSwipe"},
};

BEGIN_CUSTOM_SCHEDULES( CGargantua )
slGargFlame,
    slGargSwipe
    END_CUSTOM_SCHEDULES();

void CGargantua::OnCreate()
{
    CBaseMonster::OnCreate();

    pev->model = MAKE_STRING( "models/garg.mdl" );

    SetClassification( "alien_monster" );
}

void CGargantua::EyeOn( int level )
{
    m_eyeBrightness = level;
}

void CGargantua::EyeOff()
{
    m_eyeBrightness = 0;
}

void CGargantua::EyeUpdate()
{
    if( m_pEyeGlow )
    {
        m_pEyeGlow->pev->renderamt = UTIL_Approach( m_eyeBrightness, m_pEyeGlow->pev->renderamt, 26 );
        if( m_pEyeGlow->pev->renderamt == 0 )
            m_pEyeGlow->pev->effects |= EF_NODRAW;
        else
            m_pEyeGlow->pev->effects &= ~EF_NODRAW;
        m_pEyeGlow->SetOrigin( pev->origin );
    }
}

void CGargantua::StompAttack()
{
    TraceResult trace;

    UTIL_MakeVectors( pev->angles );
    Vector vecStart = pev->origin + Vector( 0, 0, 60 ) + 35 * gpGlobals->v_forward;
    Vector vecAim = ShootAtEnemy( vecStart );
    Vector vecEnd = ( vecAim * 1024 ) + vecStart;

    UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, edict(), &trace );
    CStomp::StompCreate( vecStart, trace.vecEndPos, 0, this );
    UTIL_ScreenShake( pev->origin, 12.0, 100.0, 2.0, 1000 );
    EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pStompSounds ), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );

    UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 20 ), ignore_monsters, edict(), &trace );
    if( trace.flFraction < 1.0 )
        UTIL_DecalTrace( &trace, DECAL_GARGSTOMP1 );
}

void CGargantua::FlameCreate()
{
    int i;
    Vector posGun, angleGun;
    TraceResult trace;

    UTIL_MakeVectors( pev->angles );

    for( i = 0; i < 4; i++ )
    {
        if( i < 2 )
            m_pFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE_NAME, 240 );
        else
            m_pFlame[i] = CBeam::BeamCreate( GARG_BEAM_SPRITE2, 140 );
        if( m_pFlame[i] )
        {
            int attach = i % 2;
            // attachment is 0 based in GetAttachment
            GetAttachment( attach + 1, posGun, angleGun );

            Vector vecEnd = ( gpGlobals->v_forward * GARG_FLAME_LENGTH ) + posGun;
            UTIL_TraceLine( posGun, vecEnd, dont_ignore_monsters, edict(), &trace );

            m_pFlame[i]->PointEntInit( trace.vecEndPos, entindex() );
            if( i < 2 )
                m_pFlame[i]->SetColor( 255, 130, 90 );
            else
                m_pFlame[i]->SetColor( 0, 120, 255 );
            m_pFlame[i]->SetBrightness( 190 );
            m_pFlame[i]->SetFlags( BEAM_FSHADEIN );
            m_pFlame[i]->SetScrollRate( 20 );
            // attachment is 1 based in SetEndAttachment
            m_pFlame[i]->SetEndAttachment( attach + 2 );
            CSoundEnt::InsertSound( bits_SOUND_COMBAT, posGun, 384, 0.3 );
        }
    }
    EmitSound( CHAN_BODY, pBeamAttackSounds[1], 1.0, ATTN_NORM );
    EmitSound( CHAN_WEAPON, pBeamAttackSounds[2], 1.0, ATTN_NORM );
}

void CGargantua::FlameControls( float angleX, float angleY )
{
    if( angleY < -180 )
        angleY += 360;
    else if( angleY > 180 )
        angleY -= 360;

    if( angleY < -45 )
        angleY = -45;
    else if( angleY > 45 )
        angleY = 45;

    m_flameX = UTIL_ApproachAngle( angleX, m_flameX, 4 );
    m_flameY = UTIL_ApproachAngle( angleY, m_flameY, 8 );
    SetBoneController( 0, m_flameY );
    SetBoneController( 1, m_flameX );
}

void CGargantua::FlameUpdate()
{
    int i;
    TraceResult trace;
    Vector vecStart, angleGun;
    bool streaks = false;

    for( i = 0; i < 2; i++ )
    {
        if( m_pFlame[i] )
        {
            Vector vecAim = pev->angles;
            vecAim.x += m_flameX;
            vecAim.y += m_flameY;

            UTIL_MakeVectors( vecAim );

            GetAttachment( i + 1, vecStart, angleGun );
            Vector vecEnd = vecStart + ( gpGlobals->v_forward * GARG_FLAME_LENGTH ); //  - offset[i] * gpGlobals->v_right;

            UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &trace );

            m_pFlame[i]->SetStartPos( trace.vecEndPos );
            m_pFlame[i + 2]->SetStartPos( ( vecStart * 0.6 ) + ( trace.vecEndPos * 0.4 ) );

            if( trace.flFraction != 1.0 && gpGlobals->time > m_streakTime )
            {
                StreakSplash( trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400 );
                streaks = true;
                UTIL_DecalTrace( &trace, DECAL_SMALLSCORCH1 + RANDOM_LONG( 0, 2 ) );
            }
            // RadiusDamage(trace.vecEndPos, this, this, g_cfg.GetValue<float>( "gargantua_dmg_fire"sv, 5, this ), DMG_BURN, Classify());
            FlameDamage( vecStart, trace.vecEndPos, this, this, g_cfg.GetValue<float>( "gargantua_dmg_fire"sv, 5, this ), DMG_BURN, Classify() );

            MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
            WRITE_BYTE( TE_ELIGHT );
            WRITE_SHORT( entindex() + 0x1000 * ( i + 2 ) ); // entity, attachment
            WRITE_COORD( vecStart.x );                    // origin
            WRITE_COORD( vecStart.y );
            WRITE_COORD( vecStart.z );
            WRITE_COORD( RANDOM_FLOAT( 32, 48 ) ); // radius
            WRITE_BYTE( 255 );                   // R
            WRITE_BYTE( 255 );                   // G
            WRITE_BYTE( 255 );                   // B
            WRITE_BYTE( 2 );                       // life * 10
            WRITE_COORD( 0 );                       // decay
            MESSAGE_END();
        }
    }
    if( streaks )
        m_streakTime = gpGlobals->time;
}

void CGargantua::FlameDamage( Vector vecStart, Vector vecEnd, CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage,
    int bitsDamageType, EntityClassification iClassIgnore )
{
    CBaseEntity* pEntity = nullptr;
    TraceResult tr;
    float flAdjustedDamage;
    Vector vecSpot;

    Vector vecMid = ( vecStart + vecEnd ) * 0.5;

    float searchRadius = ( vecStart - vecMid ).Length();

    Vector vecAim = ( vecEnd - vecStart ).Normalize();

    // iterate on all entities in the vicinity.
    while( ( pEntity = UTIL_FindEntityInSphere( pEntity, vecMid, searchRadius ) ) != nullptr )
    {
        if( pEntity->pev->takedamage != DAMAGE_NO )
        {
            // UNDONE: this should check a damage mask, not an ignore
            if( iClassIgnore != ENTCLASS_NONE && pEntity->Classify() == iClassIgnore )
            { // houndeyes don't hurt other houndeyes with their attack
                continue;
            }

            vecSpot = pEntity->BodyTarget( vecMid );

            float dist = DotProduct( vecAim, vecSpot - vecMid );
            if( dist > searchRadius )
                dist = searchRadius;
            else if( dist < -searchRadius )
                dist = searchRadius;

            Vector vecSrc = vecMid + dist * vecAim;

            UTIL_TraceLine( vecSrc, vecSpot, dont_ignore_monsters, edict(), &tr );

            if( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
            { // the explosion can 'see' this entity, so hurt them!
                // decrease damage for an ent that's farther from the flame.
                dist = ( vecSrc - tr.vecEndPos ).Length();

                if( dist > 64 )
                {
                    flAdjustedDamage = flDamage - ( dist - 64 ) * 0.4;
                    if( flAdjustedDamage <= 0 )
                        continue;
                }
                else
                {
                    flAdjustedDamage = flDamage;
                }

                // AILogger->debug("hit {}", STRING(pEntity->pev->classname));
                if( tr.flFraction != 1.0 )
                {
                    ClearMultiDamage();
                    pEntity->TraceAttack( inflictor, flAdjustedDamage, ( tr.vecEndPos - vecSrc ).Normalize(), &tr, bitsDamageType );
                    ApplyMultiDamage( inflictor, attacker );
                }
                else
                {
                    pEntity->TakeDamage( inflictor, attacker, flAdjustedDamage, bitsDamageType );
                }
            }
        }
    }
}

void CGargantua::FlameDestroy()
{
    int i;

    EmitSound( CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM );
    for( i = 0; i < 4; i++ )
    {
        if( m_pFlame[i] )
        {
            UTIL_Remove( m_pFlame[i] );
            m_pFlame[i] = nullptr;
        }
    }
}

void CGargantua::PrescheduleThink()
{
    if( !HasConditions( bits_COND_SEE_ENEMY ) )
    {
        m_seeTime = gpGlobals->time + 5;
        EyeOff();
    }
    else
        EyeOn( 200 );

    EyeUpdate();
}

void CGargantua::SetYawSpeed()
{
    int ys;

    switch ( m_Activity )
    {
    case ACT_IDLE:
        ys = 60;
        break;
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        ys = 180;
        break;
    case ACT_WALK:
    case ACT_RUN:
        ys = 60;
        break;

    default:
        ys = 60;
        break;
    }

    pev->yaw_speed = ys;
}

SpawnAction CGargantua::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "gargantua_health"sv, 1000, this );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_GREEN;
    // pev->view_ofs        = Vector ( 0, 0, 96 );// taken from mdl file
    m_flFieldOfView = -0.2; // width of forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;

    MonsterInit();

    m_pEyeGlow = CSprite::SpriteCreate( GARG_EYE_SPRITE_NAME, pev->origin, false );
    m_pEyeGlow->SetTransparency( kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation );
    m_pEyeGlow->SetAttachment( edict(), 1 );
    EyeOff();
    m_seeTime = gpGlobals->time + 5;
    m_flameTime = gpGlobals->time + 2;

    return SpawnAction::Spawn;
}

void CGargantua::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheModel( GARG_EYE_SPRITE_NAME );
    PrecacheModel( GARG_BEAM_SPRITE_NAME );
    PrecacheModel( GARG_BEAM_SPRITE2 );
    gStompSprite = PrecacheModel( GARG_STOMP_SPRITE_NAME );
    gGargGibModel = PrecacheModel( GARG_GIB_MODEL );
    PrecacheSound( GARG_STOMP_BUZZ_SOUND );

    PRECACHE_SOUND_ARRAY( pAttackHitSounds );
    PRECACHE_SOUND_ARRAY( pBeamAttackSounds );
    PRECACHE_SOUND_ARRAY( pAttackMissSounds );
    PRECACHE_SOUND_ARRAY( pRicSounds );
    PRECACHE_SOUND_ARRAY( pFootSounds );
    PRECACHE_SOUND_ARRAY( pIdleSounds );
    PRECACHE_SOUND_ARRAY( pAlertSounds );
    PRECACHE_SOUND_ARRAY( pPainSounds );
    PRECACHE_SOUND_ARRAY( pAttackSounds );
    PRECACHE_SOUND_ARRAY( pStompSounds );
    PRECACHE_SOUND_ARRAY( pBreatheSounds );
}

void CGargantua::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    AILogger->debug( "CGargantua::TraceAttack" );

    if( !IsAlive() )
    {
        CBaseMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
        return;
    }

    // UNDONE: Hit group specific damage?
    if( ( bitsDamageType & ( GARG_DAMAGE | DMG_BLAST ) ) != 0 )
    {
        if( m_painSoundTime < gpGlobals->time )
        {
            EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_GARG );
            m_painSoundTime = gpGlobals->time + RANDOM_FLOAT( 2.5, 4 );
        }
    }

    bitsDamageType &= GARG_DAMAGE;

    if( bitsDamageType == 0 )
    {
        if( pev->dmgtime != gpGlobals->time || ( RANDOM_LONG( 0, 100 ) < 20 ) )
        {
            UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 0.5, 1.5 ) );
            pev->dmgtime = gpGlobals->time;
            //            if ( RANDOM_LONG(0,100) < 25 )
            //                EmitSound(CHAN_BODY, pRicSounds[ RANDOM_LONG(0,std::size(pRicSounds)-1) ], 1.0, ATTN_NORM);
        }
        flDamage = 0;
    }

    CBaseMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
}

bool CGargantua::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    AILogger->debug( "CGargantua::TakeDamage" );

    if( IsAlive() )
    {
        if( ( bitsDamageType & GARG_DAMAGE ) == 0 )
            flDamage *= 0.01;
        if( ( bitsDamageType & DMG_BLAST ) != 0 )
            SetConditions( bits_COND_LIGHT_DAMAGE );
    }

    return CBaseMonster::TakeDamage( inflictor, attacker, flDamage, bitsDamageType );
}

void CGargantua::DeathEffect()
{
    int i;
    UTIL_MakeVectors( pev->angles );
    Vector deathPos = pev->origin + gpGlobals->v_forward * 100;

    // Create a spiral of streaks
    CSpiral::Create( deathPos, ( pev->absmax.z - pev->absmin.z ) * 0.6, 125, 1.5, this );

    Vector position = pev->origin;
    position.z += 32;
    for( i = 0; i < 7; i += 2 )
    {
        SpawnExplosion( position, 70, ( i * 0.3 ), 60 + ( i * 20 ) );
        position.z += 15;
    }

    CBaseEntity* pSmoker = CBaseEntity::Create( "env_smoker", pev->origin, g_vecZero, nullptr );
    pSmoker->pev->health = 1;                         // 1 smoke balls
    pSmoker->pev->scale = 46;                         // 4.6X normal size
    pSmoker->pev->dmg = 0;                             // 0 radial distribution
    pSmoker->pev->nextthink = gpGlobals->time + 2.5; // Start in 2.5 seconds
}

void CGargantua::Killed( CBaseEntity* attacker, int iGib )
{
    DestroyEffects();
    CBaseMonster::Killed( attacker, GIB_NEVER );
}

void CGargantua::DestroyEffects()
{
    EyeOff();
    UTIL_Remove( m_pEyeGlow );
    m_pEyeGlow = nullptr;

    FlameDestroy();
}

void CGargantua::UpdateOnRemove()
{
    DestroyEffects();
    CBaseMonster::UpdateOnRemove();
}

bool CGargantua::CheckMeleeAttack1( float flDot, float flDist )
{
    //    AILogger->debug("CheckMelee({}, {})", flDot, flDist);

    if( flDot >= 0.7 )
    {
        if( flDist <= GARG_ATTACKDIST )
            return true;
    }
    return false;
}

bool CGargantua::CheckMeleeAttack2( float flDot, float flDist )
{
    //    AILogger->debug("CheckMelee({}, {})", flDot, flDist);

    if( gpGlobals->time > m_flameTime )
    {
        if( flDot >= 0.8 && flDist > GARG_ATTACKDIST )
        {
            if( flDist <= GARG_FLAME_LENGTH )
                return true;
        }
    }
    return false;
}

bool CGargantua::CheckRangeAttack1( float flDot, float flDist )
{
    if( gpGlobals->time > m_seeTime )
    {
        if( flDot >= 0.7 && flDist > GARG_ATTACKDIST )
        {
            return true;
        }
    }
    return false;
}

void CGargantua::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case GARG_AE_SLASH_LEFT:
    {
        // HACKHACK!!!
        CBaseEntity* pHurt = GargantuaCheckTraceHullAttack( GARG_ATTACKDIST + 10.0, g_cfg.GetValue<float>( "gargantua_dmg_slash"sv, 30, this ), DMG_SLASH );
        if( pHurt )
        {
            if( ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) ) != 0 )
            {
                pHurt->pev->punchangle.x = -30; // pitch
                pHurt->pev->punchangle.y = -30; // yaw
                pHurt->pev->punchangle.z = 30;    // roll
                // UTIL_MakeVectors(pev->angles);    // called by CheckTraceHullAttack
                pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
            }
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG( 0, 15 ) );
        }
        else // Play a random attack miss sound
            EmitSoundDyn( CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG( 0, 15 ) );

        Vector forward;
        AngleVectors( pev->angles, &forward, nullptr, nullptr );
    }
    break;

    case GARG_AE_RIGHT_FOOT:
    case GARG_AE_LEFT_FOOT:
        UTIL_ScreenShake( pev->origin, 4.0, 3.0, 1.0, 750 );
        EmitSoundDyn( CHAN_BODY, RANDOM_SOUND_ARRAY( pFootSounds ), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );
        break;

    case GARG_AE_STOMP:
        StompAttack();
        m_seeTime = gpGlobals->time + 12;
        break;

    case GARG_AE_BREATHE:
        EmitSoundDyn( CHAN_VOICE, RANDOM_SOUND_ARRAY( pBreatheSounds ), 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG( -10, 10 ) );
        break;

    default:
        CBaseMonster::HandleAnimEvent( pEvent );
        break;
    }
}

CBaseEntity* CGargantua::GargantuaCheckTraceHullAttack( float flDist, int iDamage, int iDmgType )
{
    TraceResult tr;

    UTIL_MakeVectors( pev->angles );
    Vector vecStart = pev->origin;
    vecStart.z += 64;
    Vector vecEnd = vecStart + ( gpGlobals->v_forward * flDist ) - ( gpGlobals->v_up * flDist * 0.3 );

    UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, edict(), &tr );

    if( tr.pHit )
    {
        CBaseEntity* pEntity = CBaseEntity::Instance( tr.pHit );

        if( iDamage > 0 )
        {
            pEntity->TakeDamage( this, this, iDamage, iDmgType );
        }

        return pEntity;
    }

    return nullptr;
}

const Schedule_t* CGargantua::GetScheduleOfType( int Type )
{
    // HACKHACK - turn off the flames if they are on and garg goes scripted / dead
    if( FlameIsOn() )
        FlameDestroy();

    switch ( Type )
    {
    case SCHED_MELEE_ATTACK2:
        return slGargFlame;
    case SCHED_MELEE_ATTACK1:
        return slGargSwipe;
        break;
    }

    return CBaseMonster::GetScheduleOfType( Type );
}

void CGargantua::StartTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_FLAME_SWEEP:
        FlameCreate();
        m_flWaitFinished = gpGlobals->time + pTask->flData;
        m_flameTime = gpGlobals->time + 6;
        m_flameX = 0;
        m_flameY = 0;
        break;

    case TASK_SOUND_ATTACK:
        if( RANDOM_LONG( 0, 100 ) < 30 )
            EmitSound( CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_GARG );
        TaskComplete();
        break;

    case TASK_DIE:
        m_flWaitFinished = gpGlobals->time + 1.6;
        DeathEffect();
        [[fallthrough]];
    default:
        CBaseMonster::StartTask( pTask );
        break;
    }
}

void CGargantua::RunTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_DIE:
        if( gpGlobals->time > m_flWaitFinished )
        {
            pev->renderfx = kRenderFxExplode;
            pev->rendercolor.x = 255;
            pev->rendercolor.y = 0;
            pev->rendercolor.z = 0;
            StopAnimation();
            pev->nextthink = gpGlobals->time + 0.15;
            SetThink( &CGargantua::SUB_Remove );
            int i;
            int parts = MODEL_FRAMES( gGargGibModel );
            for( i = 0; i < 10; i++ )
            {
                CGib* pGib = g_EntityDictionary->Create<CGib>( "gib" );

                if( m_config >= 0 )
                    pGib->m_config = m_config;
        
                pGib->Spawn( GARG_GIB_MODEL );

                int bodyPart = 0;
                if( parts > 1 )
                    bodyPart = RANDOM_LONG( 0, pev->body - 1 );

                pGib->pev->body = bodyPart;
                pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
                pGib->m_material = matNone;
                pGib->pev->origin = pev->origin;
                pGib->pev->velocity = UTIL_RandomBloodVector() * RANDOM_FLOAT( 300, 500 );
                pGib->pev->nextthink = gpGlobals->time + 1.25;
                pGib->SetThink( &CGib::SUB_FadeOut );
            }
            MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
            WRITE_BYTE( TE_BREAKMODEL );

            // position
            WRITE_COORD( pev->origin.x );
            WRITE_COORD( pev->origin.y );
            WRITE_COORD( pev->origin.z );

            // size
            WRITE_COORD( 200 );
            WRITE_COORD( 200 );
            WRITE_COORD( 128 );

            // velocity
            WRITE_COORD( 0 );
            WRITE_COORD( 0 );
            WRITE_COORD( 0 );

            // randomization
            WRITE_BYTE( 200 );

            // Model
            WRITE_SHORT( gGargGibModel ); // model id#

            // # of shards
            WRITE_BYTE( 50 );

            // duration
            WRITE_BYTE( 20 ); // 3.0 seconds

            // flags

            WRITE_BYTE( BREAK_FLESH );
            MESSAGE_END();

            return;
        }
        else
            CBaseMonster::RunTask( pTask );
        break;

    case TASK_FLAME_SWEEP:
        if( gpGlobals->time > m_flWaitFinished )
        {
            FlameDestroy();
            TaskComplete();
            FlameControls( 0, 0 );
            SetBoneController( 0, 0 );
            SetBoneController( 1, 0 );
        }
        else
        {
            bool cancel = false;

            Vector angles = g_vecZero;

            FlameUpdate();
            CBaseEntity* pEnemy = m_hEnemy;
            if( pEnemy )
            {
                Vector org = pev->origin;
                org.z += 64;
                Vector dir = pEnemy->BodyTarget( org ) - org;
                angles = UTIL_VecToAngles( dir );
                angles.x = -angles.x;
                angles.y -= pev->angles.y;
                if( dir.Length() > 400 )
                    cancel = true;
            }
            if( fabs( angles.y ) > 60 )
                cancel = true;

            if( cancel )
            {
                m_flWaitFinished -= 0.5;
                m_flameTime -= 0.5;
            }
            // FlameControls( angles.x + 2 * sin(gpGlobals->time*8), angles.y + 28 * sin(gpGlobals->time*8.5) );
            FlameControls( angles.x, angles.y );
        }
        break;

    default:
        CBaseMonster::RunTask( pTask );
        break;
    }
}

class CSmoker : public CBaseEntity
{
public:
    SpawnAction Spawn() override;
    void Think() override;
};

LINK_ENTITY_TO_CLASS( env_smoker, CSmoker );

SpawnAction CSmoker::Spawn()
{
    pev->movetype = MOVETYPE_NONE;
    pev->nextthink = gpGlobals->time;
    pev->solid = SOLID_NOT;
    SetSize( g_vecZero, g_vecZero );
    pev->effects |= EF_NODRAW;
    pev->angles = g_vecZero;

    return SpawnAction::Spawn;
}

void CSmoker::Think()
{
    // lots of smoke
    MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
    WRITE_BYTE( TE_SMOKE );
    WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -pev->dmg, pev->dmg ) );
    WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -pev->dmg, pev->dmg ) );
    WRITE_COORD( pev->origin.z );
    WRITE_SHORT( g_sModelIndexSmoke );
    WRITE_BYTE( RANDOM_LONG( pev->scale, pev->scale * 1.1 ) );
    WRITE_BYTE( RANDOM_LONG( 8, 14 ) ); // framerate
    MESSAGE_END();

    pev->health--;
    if( pev->health > 0 )
        pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.2 );
    else
        UTIL_Remove( this );
}

SpawnAction CSpiral::Spawn()
{
    pev->movetype = MOVETYPE_NONE;
    pev->nextthink = gpGlobals->time;
    pev->solid = SOLID_NOT;
    SetSize( g_vecZero, g_vecZero );
    pev->effects |= EF_NODRAW;
    pev->angles = g_vecZero;

    return SpawnAction::Spawn;
}

CSpiral* CSpiral::Create( const Vector& origin, float height, float radius, float duration, CBaseEntity* owner )
{
    if( duration <= 0 )
        return nullptr;

    CSpiral* pSpiral = g_EntityDictionary->Create<CSpiral>( "streak_spiral" );

    if( owner != nullptr && owner->m_config >= 0 )
        pSpiral->m_config = owner->m_config;

    pSpiral->Spawn();
    pSpiral->pev->dmgtime = pSpiral->pev->nextthink;
    pSpiral->pev->origin = origin;
    pSpiral->pev->scale = radius;
    pSpiral->pev->dmg = height;
    pSpiral->pev->speed = duration;
    pSpiral->pev->health = 0;
    pSpiral->pev->angles = g_vecZero;

    return pSpiral;
}

#define SPIRAL_INTERVAL 0.1 // 025

void CSpiral::Think()
{
    float time = gpGlobals->time - pev->dmgtime;

    while( time > SPIRAL_INTERVAL )
    {
        Vector position = pev->origin;
        Vector direction = Vector( 0, 0, 1 );

        float fraction = 1.0 / pev->speed;

        float radius = ( pev->scale * pev->health ) * fraction;

        position.z += ( pev->health * pev->dmg ) * fraction;
        pev->angles.y = ( pev->health * 360 * 8 ) * fraction;
        UTIL_MakeVectors( pev->angles );
        position = position + gpGlobals->v_forward * radius;
        direction = ( direction + gpGlobals->v_forward ).Normalize();

        StreakSplash( position, Vector( 0, 0, 1 ), RANDOM_LONG( 8, 11 ), 20, RANDOM_LONG( 50, 150 ), 400 );

        // Jeez, how many counters should this take ? :)
        pev->dmgtime += SPIRAL_INTERVAL;
        pev->health += SPIRAL_INTERVAL;
        time -= SPIRAL_INTERVAL;
    }

    pev->nextthink = gpGlobals->time;

    if( pev->health >= pev->speed )
        UTIL_Remove( this );
}

// TODO: move to explode.cpp
// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosion( Vector center, float randomRange, float time, int magnitude )
{
    KeyValueData kvd;
    char buf[128];

    center.x += RANDOM_FLOAT( -randomRange, randomRange );
    center.y += RANDOM_FLOAT( -randomRange, randomRange );

    CBaseEntity* pExplosion = CBaseEntity::Create( "env_explosion", center, g_vecZero, nullptr );
    sprintf( buf, "%3d", magnitude );
    kvd.szKeyName = "iMagnitude";
    kvd.szValue = buf;
    pExplosion->KeyValue( &kvd );
    pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

    pExplosion->Spawn();
    pExplosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
    pExplosion->pev->nextthink = gpGlobals->time + time;
}
