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
#include "nodes.h"
#include "squadmonster.h"

// houndeye does 20 points of damage spread over a sphere 384 units in diameter, and each additional
// squad member increases the BASE damage by 110%, per the spec.
#define HOUNDEYE_MAX_SQUAD_SIZE 4
#define HOUNDEYE_MAX_ATTACK_RADIUS 384
#define HOUNDEYE_SQUAD_BONUS (float)1.1

// Marphy Fact Files Fix - Fix various instances of Houndeye not correctly blinking/closing eyes
#define HOUNDEYE_EYE_FRAMES 3 // how many different switchable maps for the eye

#define HOUNDEYE_SOUND_STARTLE_VOLUME 128 // how loud a sound has to be to badly scare a sleeping houndeye

enum
{
    TASK_HOUND_CLOSE_EYE = LAST_COMMON_TASK + 1,
    TASK_HOUND_OPEN_EYE,
    TASK_HOUND_THREAT_DISPLAY,
    TASK_HOUND_FALL_ASLEEP,
    TASK_HOUND_WAKE_UP,
    TASK_HOUND_HOP_BACK
};

enum
{
    SCHED_HOUND_AGITATED = LAST_COMMON_SCHEDULE + 1,
    SCHED_HOUND_HOP_RETREAT,
    SCHED_HOUND_FAIL,
};

#define HOUND_AE_WARN 1
#define HOUND_AE_STARTATTACK 2
#define HOUND_AE_THUMP 3
#define HOUND_AE_ANGERSOUND1 4
#define HOUND_AE_ANGERSOUND2 5
#define HOUND_AE_HOPBACK 6
#define HOUND_AE_CLOSE_EYE 7

/**
 *    @brief spooky sonic dog.
 */
class CHoundeye : public CSquadMonster
{
    DECLARE_CLASS( CHoundeye, CSquadMonster );
    DECLARE_DATAMAP();
    DECLARE_CUSTOM_SCHEDULES();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    void SetYawSpeed() override;
    void WarmUpSound();
    void AlertSound() override;
    void DeathSound() override;
    void WarnSound();
    void PainSound() override;
    void IdleSound() override;
    void StartTask( const Task_t* pTask ) override;
    void RunTask( const Task_t* pTask ) override;
    void SonicAttack();
    void PrescheduleThink() override;
    void SetActivity( Activity NewActivity ) override;

    bool HasAlienGibs() override { return true; }

    bool CanRecruit( CSquadMonster* recruit ) override
    {
        return recruit->ClassnameIs( GetClassname() );
    }

    /**
     *    @brief writes a color vector to the network based on the size of the group.
     */
    void WriteBeamColor();

    /**
     *    @brief overridden for houndeyes so that they try to get within half of their max attack radius
     *    before attacking, so as to increase their chances of doing damage.
     */
    bool CheckRangeAttack1( float flDot, float flDist ) override;
    bool FValidateHintType( short sHint ) override;
    bool FCanActiveIdle() override;
    const Schedule_t* GetScheduleOfType( int Type ) override;
    const Schedule_t* GetSchedule() override;

    int m_iSpriteTexture;
    bool m_fAsleep;            // some houndeyes sleep in idle mode if this is set, the houndeye is lying down
    bool m_fDontBlink;        // don't try to open/close eye if this bit is set!
    Vector m_vecPackCenter; // the center of the pack. The leader maintains this by averaging the origins of all pack members.
};

LINK_ENTITY_TO_CLASS( monster_houndeye, CHoundeye );

BEGIN_DATAMAP( CHoundeye )
    DEFINE_FIELD( m_iSpriteTexture, FIELD_INTEGER ),
    DEFINE_FIELD( m_fAsleep, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fDontBlink, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_vecPackCenter, FIELD_POSITION_VECTOR ),
END_DATAMAP();

void CHoundeye::OnCreate()
{
    CSquadMonster::OnCreate();

    pev->model = MAKE_STRING( "models/houndeye.mdl" );

    SetClassification( "alien_monster" );
}

bool CHoundeye::FValidateHintType( short sHint )
{
    static short sHoundHints[] =
        {
            HINT_WORLD_MACHINERY,
            HINT_WORLD_BLINKING_LIGHT,
            HINT_WORLD_HUMAN_BLOOD,
            HINT_WORLD_ALIEN_BLOOD,
        };

    for( std::size_t i = 0; i < std::size( sHoundHints ); i++ )
    {
        if( sHoundHints[i] == sHint )
        {
            return true;
        }
    }

    AILogger->debug( "Couldn't validate hint type" );
    return false;
}

bool CHoundeye::FCanActiveIdle()
{
    if( InSquad() )
    {
        CSquadMonster* pSquadLeader = MySquadLeader();

        for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
        {
            CSquadMonster* pMember = pSquadLeader->MySquadMember( i );

            if( pMember != nullptr && pMember != this && pMember->m_iHintNode != NO_NODE )
            {
                // someone else in the group is active idling right now!
                return false;
            }
        }

        return true;
    }

    return true;
}

bool CHoundeye::CheckRangeAttack1( float flDot, float flDist )
{
    if( flDist <= ( HOUNDEYE_MAX_ATTACK_RADIUS * 0.5 ) && flDot >= 0.3 )
    {
        return true;
    }
    return false;
}

void CHoundeye::SetYawSpeed()
{
    int ys;

    ys = 90;

    switch ( m_Activity )
    {
    case ACT_CROUCHIDLE: // sleeping!
        ys = 0;
        break;
    case ACT_IDLE:
        ys = 60;
        break;
    case ACT_WALK:
        ys = 90;
        break;
    case ACT_RUN:
        ys = 90;
        break;
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        ys = 90;
        break;
    }

    pev->yaw_speed = ys;
}

void CHoundeye::SetActivity( Activity NewActivity )
{
    int iSequence;

    if( NewActivity == m_Activity )
        return;

    if( m_MonsterState == MONSTERSTATE_COMBAT && NewActivity == ACT_IDLE && RANDOM_LONG( 0, 1 ) )
    {
        // play pissed idle.
        iSequence = LookupSequence( "madidle" );

        m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

        // In case someone calls this with something other than the ideal activity
        m_IdealActivity = m_Activity;

        // Set to the desired anim, or default anim if the desired is not present
        if( iSequence > ACTIVITY_NOT_AVAILABLE )
        {
            pev->sequence = iSequence; // Set to the reset anim (if it's there)
            pev->frame = 0;               // FIX: frame counter shouldn't be reset when its the same activity as before
            ResetSequenceInfo();
            SetYawSpeed();
        }
    }
    else
    {
        CSquadMonster::SetActivity( NewActivity );
    }
}

void CHoundeye::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case HOUND_AE_WARN:
        // do stuff for this event.
        WarnSound();
        break;

    case HOUND_AE_STARTATTACK:
        WarmUpSound();
        break;

    case HOUND_AE_HOPBACK:
    {
        float flGravity = g_psv_gravity->value;

        pev->flags &= ~FL_ONGROUND;

        pev->velocity = gpGlobals->v_forward * -200;
        pev->velocity.z += ( 0.6 * flGravity ) * 0.5;

        break;
    }

    case HOUND_AE_THUMP:
        // emit the shockwaves
        SonicAttack();
        break;

    case HOUND_AE_ANGERSOUND1:
        EmitSound( CHAN_VOICE, "houndeye/he_pain3.wav", 1, ATTN_NORM );
        break;

    case HOUND_AE_ANGERSOUND2:
        EmitSound( CHAN_VOICE, "houndeye/he_pain1.wav", 1, ATTN_NORM );
        break;

    case HOUND_AE_CLOSE_EYE:
        if( !m_fDontBlink )
        {
            pev->skin = HOUNDEYE_EYE_FRAMES - 1;
        }
        break;

    default:
        CSquadMonster::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CHoundeye::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "houndeye_health"sv, 30, this );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -16, -16, 0 ), Vector( 16, 16, 36 ) );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_YELLOW;
    pev->effects = 0;
    pev->yaw_speed = 5;       //!!! should we put this in the monster's changeanim function since turn rates may vary with state/anim?
    m_flFieldOfView = 0.5; // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;
    m_fAsleep = false; // everyone spawns awake
    m_fDontBlink = false;
    m_afCapability |= bits_CAP_SQUAD;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CHoundeye::Precache()
{
    PrecacheModel( STRING( pev->model ) );

    PrecacheSound( "houndeye/he_alert1.wav" );
    PrecacheSound( "houndeye/he_alert2.wav" );
    PrecacheSound( "houndeye/he_alert3.wav" );

    PrecacheSound( "houndeye/he_die1.wav" );
    PrecacheSound( "houndeye/he_die2.wav" );
    PrecacheSound( "houndeye/he_die3.wav" );

    PrecacheSound( "houndeye/he_idle1.wav" );
    PrecacheSound( "houndeye/he_idle2.wav" );
    PrecacheSound( "houndeye/he_idle3.wav" );

    PrecacheSound( "houndeye/he_hunt1.wav" );
    PrecacheSound( "houndeye/he_hunt2.wav" );
    PrecacheSound( "houndeye/he_hunt3.wav" );

    PrecacheSound( "houndeye/he_pain1.wav" );
    PrecacheSound( "houndeye/he_pain3.wav" );
    PrecacheSound( "houndeye/he_pain4.wav" );
    PrecacheSound( "houndeye/he_pain5.wav" );

    PrecacheSound( "houndeye/he_attack1.wav" );
    PrecacheSound( "houndeye/he_attack3.wav" );

    PrecacheSound( "houndeye/he_blast1.wav" );
    PrecacheSound( "houndeye/he_blast2.wav" );
    PrecacheSound( "houndeye/he_blast3.wav" );

    m_iSpriteTexture = PrecacheModel( "sprites/shockwave.spr" );
}

void CHoundeye::IdleSound()
{
    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "houndeye/he_idle1.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "houndeye/he_idle2.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "houndeye/he_idle3.wav", 1, ATTN_NORM );
        break;
    }
}

void CHoundeye::WarmUpSound()
{
    switch ( RANDOM_LONG( 0, 1 ) )
    {
    case 0:
        EmitSound( CHAN_WEAPON, "houndeye/he_attack1.wav", 0.7, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_WEAPON, "houndeye/he_attack3.wav", 0.7, ATTN_NORM );
        break;
    }
}

void CHoundeye::WarnSound()
{
    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "houndeye/he_hunt1.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "houndeye/he_hunt2.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "houndeye/he_hunt3.wav", 1, ATTN_NORM );
        break;
    }
}

void CHoundeye::AlertSound()
{

    if( InSquad() && !IsLeader() )
    {
        return; // only leader makes ALERT sound.
    }

    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "houndeye/he_alert1.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "houndeye/he_alert2.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "houndeye/he_alert3.wav", 1, ATTN_NORM );
        break;
    }
}

void CHoundeye::DeathSound()
{
    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "houndeye/he_die1.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "houndeye/he_die2.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "houndeye/he_die3.wav", 1, ATTN_NORM );
        break;
    }
}

void CHoundeye::PainSound()
{
    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_VOICE, "houndeye/he_pain3.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_VOICE, "houndeye/he_pain4.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_VOICE, "houndeye/he_pain5.wav", 1, ATTN_NORM );
        break;
    }
}

void CHoundeye::WriteBeamColor()
{
    byte bRed, bGreen, bBlue;

    if( InSquad() )
    {
        switch ( SquadCount() )
        {
        case 2:
            // no case for 0 or 1, cause those are impossible for monsters in Squads.
            bRed = 101;
            bGreen = 133;
            bBlue = 221;
            break;
        case 3:
            bRed = 67;
            bGreen = 85;
            bBlue = 255;
            break;
        case 4:
            bRed = 62;
            bGreen = 33;
            bBlue = 211;
            break;
        default:
            AILogger->debug( "Unsupported Houndeye SquadSize!" );
            bRed = 188;
            bGreen = 220;
            bBlue = 255;
            break;
        }
    }
    else
    {
        // solo houndeye - weakest beam
        bRed = 188;
        bGreen = 220;
        bBlue = 255;
    }

    WRITE_BYTE( bRed );
    WRITE_BYTE( bGreen );
    WRITE_BYTE( bBlue );
}

void CHoundeye::SonicAttack()
{
    float flAdjustedDamage;
    float flDist;

    switch ( RANDOM_LONG( 0, 2 ) )
    {
    case 0:
        EmitSound( CHAN_WEAPON, "houndeye/he_blast1.wav", 1, ATTN_NORM );
        break;
    case 1:
        EmitSound( CHAN_WEAPON, "houndeye/he_blast2.wav", 1, ATTN_NORM );
        break;
    case 2:
        EmitSound( CHAN_WEAPON, "houndeye/he_blast3.wav", 1, ATTN_NORM );
        break;
    }

    // blast circles
    MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
    WRITE_BYTE( TE_BEAMCYLINDER );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 16 );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 16 + HOUNDEYE_MAX_ATTACK_RADIUS / .2 ); // reach damage radius over .3 seconds
    WRITE_SHORT( m_iSpriteTexture );
    WRITE_BYTE( 0 );    // startframe
    WRITE_BYTE( 0 );    // framerate
    WRITE_BYTE( 2 );    // life
    WRITE_BYTE( 16 ); // width
    WRITE_BYTE( 0 );    // noise

    WriteBeamColor();

    WRITE_BYTE( 255 ); // brightness
    WRITE_BYTE( 0 );     // speed
    MESSAGE_END();

    MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
    WRITE_BYTE( TE_BEAMCYLINDER );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 16 );
    WRITE_COORD( pev->origin.x );
    WRITE_COORD( pev->origin.y );
    WRITE_COORD( pev->origin.z + 16 + ( HOUNDEYE_MAX_ATTACK_RADIUS / 2 ) / .2 ); // reach damage radius over .3 seconds
    WRITE_SHORT( m_iSpriteTexture );
    WRITE_BYTE( 0 );    // startframe
    WRITE_BYTE( 0 );    // framerate
    WRITE_BYTE( 2 );    // life
    WRITE_BYTE( 16 ); // width
    WRITE_BYTE( 0 );    // noise

    WriteBeamColor();

    WRITE_BYTE( 255 ); // brightness
    WRITE_BYTE( 0 );     // speed
    MESSAGE_END();


    CBaseEntity* pEntity = nullptr;
    // iterate on all entities in the vicinity.
    while( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, HOUNDEYE_MAX_ATTACK_RADIUS ) ) != nullptr )
    {
        if( pEntity->pev->takedamage != DAMAGE_NO )
        {
            if( !pEntity->ClassnameIs( "monster_houndeye" ) )
            { // houndeyes don't hurt other houndeyes with their attack

                // houndeyes do FULL damage if the ent in question is visible. Half damage otherwise.
                // This means that you must get out of the houndeye's attack range entirely to avoid damage.
                // Calculate full damage first

                if( SquadCount() > 1 )
                {
                    // squad gets attack bonus.
                    flAdjustedDamage = g_cfg.GetValue<float>( "houndeye_dmg_blast"sv, 15, this ) + g_cfg.GetValue<float>( "houndeye_dmg_blast"sv, 15, this ) * ( HOUNDEYE_SQUAD_BONUS * ( SquadCount() - 1 ) );
                }
                else
                {
                    // solo
                    flAdjustedDamage = g_cfg.GetValue<float>( "houndeye_dmg_blast"sv, 15, this );
                }

                flDist = ( pEntity->Center() - pev->origin ).Length();

                flAdjustedDamage -= ( flDist / HOUNDEYE_MAX_ATTACK_RADIUS ) * flAdjustedDamage;

                if( !FVisible( pEntity ) )
                {
                    if( pEntity->IsPlayer() )
                    {
                        // if this entity is a client, and is not in full view, inflict half damage. We do this so that players still
                        // take the residual damage if they don't totally leave the houndeye's effective radius. We restrict it to clients
                        // so that monsters in other parts of the level don't take the damage and get pissed.
                        flAdjustedDamage *= 0.5;
                    }
                    else if( !pEntity->ClassnameIs( "func_breakable" ) && !pEntity->ClassnameIs( "func_pushable" ) )
                    {
                        // do not hurt nonclients through walls, but allow damage to be done to breakables
                        flAdjustedDamage = 0;
                    }
                }

                // AILogger->debug("Damage: {}", flAdjustedDamage);

                if( flAdjustedDamage > 0 )
                {
                    pEntity->TakeDamage( this, this, flAdjustedDamage, DMG_SONIC | DMG_ALWAYSGIB );
                }
            }
        }
    }
}

void CHoundeye::StartTask( const Task_t* pTask )
{
    m_iTaskStatus = TASKSTATUS_RUNNING;

    switch ( pTask->iTask )
    {
    case TASK_HOUND_FALL_ASLEEP:
    {
        m_fAsleep = true; // signal that hound is lying down (must stand again before doing anything else!)
        m_iTaskStatus = TASKSTATUS_COMPLETE;
        break;
    }
    case TASK_HOUND_WAKE_UP:
    {
        m_fAsleep = false; // signal that hound is standing again
        m_iTaskStatus = TASKSTATUS_COMPLETE;
        break;
    }
    case TASK_HOUND_OPEN_EYE:
    {
        m_fDontBlink = false; // turn blinking back on and that code will automatically open the eye
        m_iTaskStatus = TASKSTATUS_COMPLETE;
        break;
    }
    case TASK_HOUND_CLOSE_EYE:
    {
        pev->skin = 0;
        m_fDontBlink = true; // tell blink code to leave the eye alone.
        break;
    }
    case TASK_HOUND_THREAT_DISPLAY:
    {
        m_IdealActivity = ACT_IDLE_ANGRY;
        break;
    }
    case TASK_HOUND_HOP_BACK:
    {
        m_IdealActivity = ACT_LEAP;
        break;
    }
    case TASK_RANGE_ATTACK1:
    {
        m_IdealActivity = ACT_RANGE_ATTACK1;

        /*
                    if ( InSquad() )
                    {
                        // see if there is a battery to connect to.
                        CSquadMonster *pSquad = m_pSquadLeader;

                        while ( pSquad )
                        {
                            if ( pSquad->m_iMySlot == bits_SLOT_HOUND_BATTERY )
                            {
                                // draw a beam.
                                MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
                                    WRITE_BYTE( TE_BEAMENTS );
                                    WRITE_SHORT(entindex());
                                    WRITE_SHORT(pSquad->entindex());
                                    WRITE_SHORT( m_iSpriteTexture );
                                    WRITE_BYTE( 0 ); // framestart
                                    WRITE_BYTE( 0 ); // framerate
                                    WRITE_BYTE( 10 ); // life
                                    WRITE_BYTE( 40 );  // width
                                    WRITE_BYTE( 10 );   // noise
                                    WRITE_BYTE( 0  );   // r, g, b
                                    WRITE_BYTE( 50 );   // r, g, b
                                    WRITE_BYTE( 250);   // r, g, b
                                    WRITE_BYTE( 255 );    // brightness
                                    WRITE_BYTE( 30 );        // speed
                                MESSAGE_END();
                                break;
                            }

                            pSquad = pSquad->m_pSquadNext;
                        }
                    }
        */

        break;
    }
    case TASK_SPECIAL_ATTACK1:
    {
        m_IdealActivity = ACT_SPECIAL_ATTACK1;
        break;
    }
    case TASK_GUARD:
    {
        m_IdealActivity = ACT_GUARD;
        break;
    }
    default:
    {
        CSquadMonster::StartTask( pTask );
        break;
    }
    }
}

void CHoundeye::RunTask( const Task_t* pTask )
{
    switch ( pTask->iTask )
    {
    case TASK_HOUND_THREAT_DISPLAY:
    {
        MakeIdealYaw( m_vecEnemyLKP );
        ChangeYaw( pev->yaw_speed );

        if( m_fSequenceFinished )
        {
            TaskComplete();
        }

        break;
    }
    case TASK_HOUND_CLOSE_EYE:
    {
        if( pev->skin < HOUNDEYE_EYE_FRAMES - 1 )
        {
            pev->skin++;
        }
        break;
    }
    case TASK_HOUND_HOP_BACK:
    {
        if( m_fSequenceFinished )
        {
            TaskComplete();
        }
        break;
    }
    case TASK_SPECIAL_ATTACK1:
    {
        pev->skin = RANDOM_LONG( 0, HOUNDEYE_EYE_FRAMES - 1 );

        MakeIdealYaw( m_vecEnemyLKP );
        ChangeYaw( pev->yaw_speed );

        float life;
        life = ( ( 255 - pev->frame ) / ( pev->framerate * m_flFrameRate ) );
        if( life < 0.1 )
            life = 0.1;

        MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
        WRITE_BYTE( TE_IMPLOSION );
        WRITE_COORD( pev->origin.x );
        WRITE_COORD( pev->origin.y );
        WRITE_COORD( pev->origin.z + 16 );
        WRITE_BYTE( 50 * life + 100 );   // radius
        WRITE_BYTE( pev->frame / 25.0 ); // count
        WRITE_BYTE( life * 10 );           // life
        MESSAGE_END();

        if( m_fSequenceFinished )
        {
            SonicAttack();
            TaskComplete();
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

void CHoundeye::PrescheduleThink()
{
    // if the hound is mad and is running, make hunt noises.
    if( m_MonsterState == MONSTERSTATE_COMBAT && m_Activity == ACT_RUN && RANDOM_FLOAT( 0, 1 ) < 0.2 )
    {
        WarnSound();
    }

    // at random, initiate a blink if not already blinking or sleeping
    if( !m_fDontBlink )
    {
        if( ( pev->skin == 0 ) && RANDOM_LONG( 0, 0x7F ) == 0 )
        { // start blinking!
            pev->skin = HOUNDEYE_EYE_FRAMES - 1;
        }
        else if( pev->skin != 0 )
        { // already blinking
            pev->skin--;
        }
    }

    // if you are the leader, average the origins of each pack member to get an approximate center.
    if( IsLeader() )
    {
        CSquadMonster* pSquadMember;
        int iSquadCount = 0;

        for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
        {
            pSquadMember = MySquadMember( i );

            if( pSquadMember )
            {
                iSquadCount++;
                m_vecPackCenter = m_vecPackCenter + pSquadMember->pev->origin;
            }
        }

        m_vecPackCenter = m_vecPackCenter / iSquadCount;
    }
}

// Marphy Fact Files Fix - Fix freeze stutter after leaderlook sequence
Task_t tlHoundGuardPack[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_GUARD},
};

Schedule_t slHoundGuardPack[] =
    {
        {tlHoundGuardPack,
            std::size( tlHoundGuardPack ),
            bits_COND_SEE_HATE |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_PROVOKED |
                bits_COND_HEAR_SOUND,

            bits_SOUND_COMBAT | // sound flags
                bits_SOUND_WORLD |
                bits_SOUND_MEAT |
                bits_SOUND_PLAYER,
            "GuardPack"},
};

Task_t tlHoundYell1[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
        {TASK_SET_SCHEDULE, (float)SCHED_HOUND_AGITATED},
};

Task_t tlHoundYell2[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_RANGE_ATTACK1, (float)0},
};

Schedule_t slHoundRangeAttack[] =
    {
        {tlHoundYell1,
            std::size( tlHoundYell1 ),
            bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "HoundRangeAttack1"},
        {tlHoundYell2,
            std::size( tlHoundYell2 ),
            bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "HoundRangeAttack2"},
};

Task_t tlHoundSleep[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_RANDOM, (float)5},
        {TASK_PLAY_SEQUENCE, (float)ACT_CROUCH},
        {TASK_SET_ACTIVITY, (float)ACT_CROUCHIDLE},
        {TASK_HOUND_FALL_ASLEEP, (float)0},
        {TASK_WAIT_RANDOM, (float)25},
        {TASK_HOUND_CLOSE_EYE, (float)0},
        //{ TASK_WAIT,                (float)10                },
        //{ TASK_WAIT_RANDOM,            (float)10                },
};

/**
 *    @brief lie down and fall asleep
 */
Schedule_t slHoundSleep[] =
    {
        {tlHoundSleep,
            std::size( tlHoundSleep ),
            bits_COND_HEAR_SOUND |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_NEW_ENEMY,

            bits_SOUND_COMBAT |
                bits_SOUND_PLAYER |
                bits_SOUND_WORLD,
            "Hound Sleep"},
};

Task_t tlHoundWakeLazy[] =
    {
        {TASK_STOP_MOVING, (float)0},
        {TASK_HOUND_OPEN_EYE, (float)0},
        {TASK_WAIT_RANDOM, (float)2.5},
        {TASK_PLAY_SEQUENCE, (float)ACT_STAND},
        {TASK_HOUND_WAKE_UP, (float)0},
};

/**
 *    @brief wake and stand up lazily
 */
Schedule_t slHoundWakeLazy[] =
    {
        {tlHoundWakeLazy,
            std::size( tlHoundWakeLazy ),
            0,
            0,
            "WakeLazy"},
};

Task_t tlHoundWakeUrgent[] =
    {
        {TASK_HOUND_OPEN_EYE, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_HOP},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_HOUND_WAKE_UP, (float)0},
};

/**
 *    @brief wake and stand up with great urgency!
 */
Schedule_t slHoundWakeUrgent[] =
    {
        {tlHoundWakeUrgent,
            std::size( tlHoundWakeUrgent ),
            0,
            0,
            "WakeUrgent"},
};

Task_t tlHoundSpecialAttack1[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_FACE_IDEAL, (float)0},
        {TASK_SPECIAL_ATTACK1, (float)0},
        {TASK_PLAY_SEQUENCE, (float)ACT_IDLE_ANGRY},
};

Schedule_t slHoundSpecialAttack1[] =
    {
        {tlHoundSpecialAttack1,
            std::size( tlHoundSpecialAttack1 ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE |
                bits_COND_ENEMY_OCCLUDED,

            0,
            "Hound Special Attack1"},
};

Task_t tlHoundAgitated[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_HOUND_THREAT_DISPLAY, 0},
};

Schedule_t slHoundAgitated[] =
    {
        {tlHoundAgitated,
            std::size( tlHoundAgitated ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "Hound Agitated"},
};

Task_t tlHoundHopRetreat[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_HOUND_HOP_BACK, 0},
        {TASK_SET_SCHEDULE, (float)SCHED_TAKE_COVER_FROM_ENEMY},
};

Schedule_t slHoundHopRetreat[] =
    {
        {tlHoundHopRetreat,
            std::size( tlHoundHopRetreat ),
            0,
            0,
            "Hound Hop Retreat"},
};

Task_t tlHoundCombatFailPVS[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_HOUND_THREAT_DISPLAY, 0},
        {TASK_WAIT_FACE_ENEMY, (float)1},
};

/**
 *    @brief hound fails in combat with client in the PVS
 */
Schedule_t slHoundCombatFailPVS[] =
    {
        {tlHoundCombatFailPVS,
            std::size( tlHoundCombatFailPVS ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "HoundCombatFailPVS"},
};

Task_t tlHoundCombatFailNoPVS[] =
    {
        {TASK_STOP_MOVING, 0},
        {TASK_HOUND_THREAT_DISPLAY, 0},
        {TASK_WAIT_FACE_ENEMY, (float)2},
        {TASK_SET_ACTIVITY, (float)ACT_IDLE},
        {TASK_WAIT_PVS, 0},
};

/**
 *    @brief hound fails in combat with no client in the PVS. Don't keep peeping!
 */
Schedule_t slHoundCombatFailNoPVS[] =
    {
        {tlHoundCombatFailNoPVS,
            std::size( tlHoundCombatFailNoPVS ),
            bits_COND_NEW_ENEMY |
                bits_COND_LIGHT_DAMAGE |
                bits_COND_HEAVY_DAMAGE,
            0,
            "HoundCombatFailNoPVS"},
};

BEGIN_CUSTOM_SCHEDULES( CHoundeye )
slHoundGuardPack,
    slHoundRangeAttack,
    &slHoundRangeAttack[1],
    slHoundSleep,
    slHoundWakeLazy,
    slHoundWakeUrgent,
    slHoundSpecialAttack1,
    slHoundAgitated,
    slHoundHopRetreat,
    slHoundCombatFailPVS,
    slHoundCombatFailNoPVS
    END_CUSTOM_SCHEDULES();

const Schedule_t* CHoundeye::GetScheduleOfType( int Type )
{
    if( m_fAsleep )
    {
        // if the hound is sleeping, must wake and stand!
        if( HasConditions( bits_COND_HEAR_SOUND ) )
        {
            CSound* pWakeSound;

            pWakeSound = PBestSound();
            ASSERT( pWakeSound != nullptr );
            if( pWakeSound )
            {
                MakeIdealYaw( pWakeSound->m_vecOrigin );

                if( FLSoundVolume( pWakeSound ) >= HOUNDEYE_SOUND_STARTLE_VOLUME )
                {
                    // awakened by a loud sound
                    return &slHoundWakeUrgent[0];
                }
            }
            // sound was not loud enough to scare the bejesus out of houndeye
            return &slHoundWakeLazy[0];
        }
        else if( HasConditions( bits_COND_NEW_ENEMY ) )
        {
            // get up fast, to fight.
            return &slHoundWakeUrgent[0];
        }

        else
        {
            // hound is waking up on its own
            return &slHoundWakeLazy[0];
        }
    }
    switch ( Type )
    {
    case SCHED_IDLE_STAND:
    {
        // we may want to sleep instead of stand!
        if( InSquad() && !IsLeader() && !m_fAsleep && RANDOM_LONG( 0, 29 ) < 1 )
        {
            return &slHoundSleep[0];
        }
        // Marphy Fact Files Fix - Restore squad leader leaderlook animation
        if( InSquad() && IsLeader() && !m_fAsleep && RANDOM_LONG( 0, 14 ) < 1 )
        {
            return &slHoundGuardPack[0];
        }
        else
        {
            return CSquadMonster::GetScheduleOfType( Type );
        }
    }
    case SCHED_RANGE_ATTACK1:
    {
        return &slHoundRangeAttack[0];
        /*
                    if ( InSquad() )
                    {
                        return &slHoundRangeAttack[ RANDOM_LONG( 0, 1 ) ];
                    }

                    return &slHoundRangeAttack[ 1 ];
        */
    }
    case SCHED_SPECIAL_ATTACK1:
    {
        return &slHoundSpecialAttack1[0];
    }
    case SCHED_GUARD:
    {
        return &slHoundGuardPack[0];
    }
    case SCHED_HOUND_AGITATED:
    {
        return &slHoundAgitated[0];
    }
    case SCHED_HOUND_HOP_RETREAT:
    {
        return &slHoundHopRetreat[0];
    }
    case SCHED_FAIL:
    {
        if( m_MonsterState == MONSTERSTATE_COMBAT )
        {
            if( UTIL_FindClientInPVS( this ) )
            {
                // client in PVS
                return &slHoundCombatFailPVS[0];
            }
            else
            {
                // client has taken off!
                return &slHoundCombatFailNoPVS[0];
            }
        }
        else
        {
            return CSquadMonster::GetScheduleOfType( Type );
        }
    }
    default:
    {
        return CSquadMonster::GetScheduleOfType( Type );
    }
    }
}

const Schedule_t* CHoundeye::GetSchedule()
{
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

        if( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
        {
            if( RANDOM_FLOAT( 0, 1 ) <= 0.4 )
            {
                TraceResult tr;
                UTIL_MakeVectors( pev->angles );
                UTIL_TraceHull( pev->origin, pev->origin + gpGlobals->v_forward * -128, dont_ignore_monsters, head_hull, edict(), &tr );

                if( tr.flFraction == 1.0 )
                {
                    // it's clear behind, so the hound will jump
                    return GetScheduleOfType( SCHED_HOUND_HOP_RETREAT );
                }
            }

            return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
        }

        if( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
        {
            if( OccupySlot( bits_SLOTS_HOUND_ATTACK ) )
            {
                return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
            }

            return GetScheduleOfType( SCHED_HOUND_AGITATED );
        }
        break;
    }
    }

    return CSquadMonster::GetSchedule();
}

class CDeadHoundeye : public CBaseMonster
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;

    bool HasAlienGibs() override { return true; }

    bool KeyValue( KeyValueData* pkvd ) override;

    int m_iPose; // which sequence to display    -- temporary, don't need to save
    static const char* m_szPoses[3];
};

const char* CDeadHoundeye::m_szPoses[] = {"dead"};

void CDeadHoundeye::OnCreate()
{
    CBaseMonster::OnCreate();

    // Corpses have less health
    pev->health = 8;
    pev->model = MAKE_STRING( "models/houndeye_dead.mdl" );

    SetClassification( "alien_passive" );
}

bool CDeadHoundeye::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pose" ) )
    {
        m_iPose = atoi( pkvd->szValue );
        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_houndeye_dead, CDeadHoundeye );

SpawnAction CDeadHoundeye::Spawn()
{
    PrecacheModel( STRING( pev->model ) );
    SetModel( STRING( pev->model ) );

    pev->effects = 0;
    pev->yaw_speed = 8;
    pev->sequence = 0;
    m_bloodColor = BLOOD_COLOR_GREEN;

    pev->sequence = LookupSequence( m_szPoses[m_iPose] );

    if( pev->sequence == -1 )
    {
        AILogger->debug( "Dead houndeye with bad pose" );
    }

    MonsterInitDead();

    return SpawnAction::Spawn;
}
