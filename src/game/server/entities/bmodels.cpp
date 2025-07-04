/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *    This product contains software technology licensed from Id
 *    Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *    All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

/**
 *    @file
 *    spawn, think, and use functions for entities that use brush models
 */

#include "cbase.h"
#include "doors.h"

#define SF_BRUSH_ACCDCC 16         // brush should accelerate and decelerate when toggled
#define SF_BRUSH_HURT 32         // rotating brush that inflicts pain based on rotation speed
#define SF_ROTATING_NOT_SOLID 64 // some special rotating objects are not solid.

#define SF_PENDULUM_SWING 2 // spawnflag that makes a pendulum a rope swing.

/**
 *    @brief This is just a solid wall if not inhibited
 */
class CFuncWall : public CBaseEntity
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    // Bmodels don't go across transitions
    int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( func_wall, CFuncWall );

SpawnAction CFuncWall::Spawn()
{
    pev->angles = g_vecZero;
    pev->movetype = MOVETYPE_PUSH; // so it doesn't get pushed by anything
    pev->solid = SOLID_BSP;
    SetModel( STRING( pev->model ) );

    // If it can't move/go away, it's really part of the world
    pev->flags |= FL_WORLDBRUSH;

    return SpawnAction::Spawn;
}

void CFuncWall::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( ShouldToggle( useType, pev->frame != 0 ) )
        pev->frame = 1 - pev->frame;
}

#define SF_WALL_START_OFF 0x0001

class CFuncWallToggle : public CFuncWall
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    void TurnOff();
    void TurnOn();
    bool IsOn();
};

LINK_ENTITY_TO_CLASS( func_wall_toggle, CFuncWallToggle );

SpawnAction CFuncWallToggle::Spawn()
{
    CFuncWall::Spawn();

    if( ( pev->spawnflags & SF_WALL_START_OFF ) != 0 )
    {
        TurnOff();
    }

    return SpawnAction::Spawn;
}

void CFuncWallToggle::TurnOff()
{
    pev->solid = SOLID_NOT;
    pev->effects |= EF_NODRAW;
    SetOrigin( pev->origin );
}

void CFuncWallToggle::TurnOn()
{
    pev->solid = SOLID_BSP;
    pev->effects &= ~EF_NODRAW;
    SetOrigin( pev->origin );
}

bool CFuncWallToggle::IsOn()
{
    if( pev->solid == SOLID_NOT )
        return false;
    return true;
}

void CFuncWallToggle::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    bool status = IsOn();

    if( ShouldToggle( useType, status ) )
    {
        if( status )
            TurnOff();
        else
            TurnOn();
    }
}

#define SF_CONVEYOR_VISUAL 0x0001
#define SF_CONVEYOR_NOTSOLID 0x0002

class CFuncConveyor : public CFuncWall
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    void UpdateSpeed( float speed );
};

LINK_ENTITY_TO_CLASS( func_conveyor, CFuncConveyor );

SpawnAction CFuncConveyor::Spawn()
{
    SetMovedir( this );
    CFuncWall::Spawn();

    if( ( pev->spawnflags & SF_CONVEYOR_VISUAL ) == 0 )
        SetBits( pev->flags, FL_CONVEYOR );

    // HACKHACK - This is to allow for some special effects
    if( ( pev->spawnflags & SF_CONVEYOR_NOTSOLID ) != 0 )
    {
        pev->solid = SOLID_NOT;
        pev->skin = 0; // Don't want the engine thinking we've got special contents on this brush
    }

    if( pev->speed == 0 )
        pev->speed = 100;

    UpdateSpeed( pev->speed );

    return SpawnAction::Spawn;
}

// HACKHACK -- This is ugly, but encode the speed in the rendercolor to avoid adding more data to the network stream
void CFuncConveyor::UpdateSpeed( float speed )
{
    // Encode it as an integer with 4 fractional bits
    int speedCode = (int)( fabs( speed ) * 16.0 );

    if( speed < 0 )
        pev->rendercolor.x = 1;
    else
        pev->rendercolor.x = 0;

    pev->rendercolor.y = ( speedCode >> 8 );
    pev->rendercolor.z = ( speedCode & 0xFF );
}

void CFuncConveyor::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    pev->speed = -pev->speed;
    UpdateSpeed( pev->speed );
}

/**
 *    @brief A simple entity that looks solid but lets you walk through it.
 */
class CFuncIllusionary : public CBaseToggle
{
public:
    SpawnAction Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( func_illusionary, CFuncIllusionary );

bool CFuncIllusionary::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "skin" ) ) // skin is used for content type
    {
        pev->skin = atof( pkvd->szValue );
        return true;
    }

    return CBaseToggle::KeyValue( pkvd );
}

SpawnAction CFuncIllusionary::Spawn()
{
    pev->angles = g_vecZero;
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_NOT; // always solid_not
    SetModel( STRING( pev->model ) );

    // I'd rather eat the network bandwidth of this than figure out how to save/restore
    // these entities after they have been moved to the client, or respawn them ala Quake
    // Perhaps we can do this in deathmatch only.
    //    g_engfuncs.pfnMakeStatic(edict());
    return SpawnAction::Spawn;
}

/**
 *    @brief Monster only clip brush
 *    @details This brush will be solid for any entity who has the FL_MONSTERCLIP flag set in pev->flags
 *    otherwise it will be invisible and not solid.
 *    This can be used to keep specific monsters out of certain areas
 */
class CFuncMonsterClip : public CFuncWall
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value ) override {} // Clear out func_wall's use function
};

LINK_ENTITY_TO_CLASS( func_clip, CFuncMonsterClip );

SpawnAction CFuncMonsterClip::Spawn()
{
    CFuncWall::Spawn();
    if( CVAR_GET_FLOAT( "showtriggers" ) == 0 )
        pev->effects = EF_NODRAW;
    pev->flags |= FL_MONSTERCLIP;
    return SpawnAction::Spawn;
}

/**
 *    @brief You need to have an origin brush as part of this entity.
 *    The center of that brush will be the point around which it is rotated.
 *    It will rotate around the Z axis by default.
 *    You can check either the X_AXIS or Y_AXIS box to change that.
 */
class CFuncRotating : public CBaseEntity
{
    DECLARE_CLASS( CFuncRotating, CBaseEntity );
    DECLARE_DATAMAP();

public:
    // basic functions
    SpawnAction Spawn() override;
    void Precache() override;

    /**
     *    @brief accelerates a non-moving func_rotating up to its speed
     */
    void SpinUp();

    /**
     *    @brief decelerates a moving func_rotating to a standstill.
     */
    void SpinDown();

    bool KeyValue( KeyValueData* pkvd ) override;

    /**
     *    @brief will hurt others based on how fast the brush is spinning
     */
    void HurtTouch( CBaseEntity* pOther );

    void RotatingUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
    void Rotate();

    /**
     *    @brief ramp pitch and volume up to final values,
     *    based on difference between how fast we're going vs how fast we plan to go
     */
    void RampPitchVol( bool fUp );

    void Blocked( CBaseEntity* pOther ) override;
    int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

    float m_flFanFriction;
    float m_flAttenuation;
    float m_flVolume;
    float m_pitch;
    string_t m_sounds;
};

BEGIN_DATAMAP( CFuncRotating )
    DEFINE_FIELD( m_flFanFriction, FIELD_FLOAT ),
    DEFINE_FIELD( m_flAttenuation, FIELD_FLOAT ),
    DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
    DEFINE_FIELD( m_pitch, FIELD_FLOAT ),
    DEFINE_FIELD( m_sounds, FIELD_SOUNDNAME ),
    DEFINE_FUNCTION( SpinUp ),
    DEFINE_FUNCTION( SpinDown ),
    DEFINE_FUNCTION( HurtTouch ),
    DEFINE_FUNCTION( RotatingUse ),
    DEFINE_FUNCTION( Rotate ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( func_rotating, CFuncRotating );

bool CFuncRotating::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "fanfriction" ) )
    {
        m_flFanFriction = atof( pkvd->szValue ) / 100;
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "Volume" ) )
    {
        m_flVolume = atof( pkvd->szValue ) / 10.0;

        if( m_flVolume > 1.0 )
            m_flVolume = 1.0;
        if( m_flVolume < 0.0 )
            m_flVolume = 0.0;
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "spawnorigin" ) )
    {
        Vector tmp;
        UTIL_StringToVector( tmp, pkvd->szValue );
        if( tmp != g_vecZero )
            pev->origin = tmp;
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "sounds" ) )
    {
        m_sounds = ALLOC_STRING( pkvd->szValue );
        return true;
    }

    return CBaseEntity::KeyValue( pkvd );
}

SpawnAction CFuncRotating::Spawn()
{
    // set final pitch.  Must not be PITCH_NORM, since we
    // plan on pitch shifting later.

    m_pitch = PITCH_NORM - 1;

    // maintain compatibility with previous maps
    if( m_flVolume == 0.0 )
        m_flVolume = 1.0;

    // if the designer didn't set a sound attenuation, default to one.
    m_flAttenuation = ATTN_NORM;

    if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_SMALLRADIUS ) )
    {
        m_flAttenuation = ATTN_IDLE;
    }
    else if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_MEDIUMRADIUS ) )
    {
        m_flAttenuation = ATTN_STATIC;
    }
    else if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_LARGERADIUS ) )
    {
        m_flAttenuation = ATTN_NORM;
    }

    // prevent divide by zero if level designer forgets friction!
    if( m_flFanFriction == 0 )
    {
        m_flFanFriction = 1;
    }

    if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_Z_AXIS ) )
        pev->movedir = Vector( 0, 0, 1 );
    else if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_X_AXIS ) )
        pev->movedir = Vector( 1, 0, 0 );
    else
        pev->movedir = Vector( 0, 1, 0 ); // y-axis

    // check for reverse rotation
    if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_BACKWARDS ) )
        pev->movedir = pev->movedir * -1;

    // some rotating objects like fake volumetric lights will not be solid.
    if( FBitSet( pev->spawnflags, SF_ROTATING_NOT_SOLID ) )
    {
        pev->solid = SOLID_NOT;
        pev->skin = CONTENTS_EMPTY;
        pev->movetype = MOVETYPE_PUSH;
    }
    else
    {
        pev->solid = SOLID_BSP;
        pev->movetype = MOVETYPE_PUSH;
    }

    SetOrigin( pev->origin );
    SetModel( STRING( pev->model ) );

    SetUse( &CFuncRotating::RotatingUse );
    // did level designer forget to assign speed?
    if( pev->speed <= 0 )
        pev->speed = 0;

    // Removed this per level designers request.  -- JAY
    //    if (pev->dmg == 0)
    //        pev->dmg = 2;

    // instant-use brush?
    if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_INSTANT ) )
    {
        SetThink( &CFuncRotating::SUB_CallUseToggle );
        pev->nextthink = pev->ltime + 1.5; // leave a magic delay for client to start up
    }
    // can this brush inflict pain?
    if( FBitSet( pev->spawnflags, SF_BRUSH_HURT ) )
    {
        SetTouch( &CFuncRotating::HurtTouch );
    }

    Precache();
    return SpawnAction::Spawn;
}

void CFuncRotating::Precache()
{
    if( FStrEq( "", STRING( m_sounds ) ) )
    {
        m_sounds = ALLOC_STRING( "common/null.wav" );
    }

    PrecacheSound( STRING( m_sounds ) );

    if( pev->avelocity != g_vecZero )
    {
        // if fan was spinning, and we went through transition or save/restore,
        // make sure we restart the sound.  1.5 sec delay is magic number. KDB

        SetThink( &CFuncRotating::SpinUp );
        pev->nextthink = pev->ltime + 1.5;
    }
}

void CFuncRotating::HurtTouch( CBaseEntity* pOther )
{
    // we can't hurt this thing, so we're not concerned with it
    if( 0 == pOther->pev->takedamage )
        return;

    // calculate damage based on rotation speed
    pev->dmg = pev->avelocity.Length() / 10;

    pOther->TakeDamage( this, this, pev->dmg, DMG_CRUSH );

    pOther->pev->velocity = ( pOther->pev->origin - VecBModelOrigin( this ) ).Normalize() * pev->dmg;
}

#define FANPITCHMIN 30
#define FANPITCHMAX 100

void CFuncRotating::RampPitchVol( bool fUp )
{

    Vector vecAVel = pev->avelocity;
    vec_t vecCur;
    vec_t vecFinal;
    float fpct;
    float fvol;
    float fpitch;
    int pitch;

    // get current angular velocity

    vecCur = fabs( vecAVel.x != 0 ? vecAVel.x : ( vecAVel.y != 0 ? vecAVel.y : vecAVel.z ) );

    // get target angular velocity

    vecFinal = ( pev->movedir.x != 0 ? pev->movedir.x : ( pev->movedir.y != 0 ? pev->movedir.y : pev->movedir.z ) );
    vecFinal *= pev->speed;
    vecFinal = fabs( vecFinal );

    // calc volume and pitch as % of final vol and pitch

    fpct = vecCur / vecFinal;
    //    if (fUp)
    //        fvol = m_flVolume * (0.5 + fpct/2.0); // spinup volume ramps up from 50% max vol
    //    else
    fvol = m_flVolume * fpct; // slowdown volume ramps down to 0

    fpitch = FANPITCHMIN + ( FANPITCHMAX - FANPITCHMIN ) * fpct;

    pitch = (int)fpitch;
    if( pitch == PITCH_NORM )
        pitch = PITCH_NORM - 1;

    // change the fan's vol and pitch

    EmitSoundDyn( CHAN_STATIC, STRING( m_sounds ),
        fvol, m_flAttenuation, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch );
}

void CFuncRotating::SpinUp()
{
    Vector vecAVel; // rotational velocity

    pev->nextthink = pev->ltime + 0.1;
    pev->avelocity = pev->avelocity + ( pev->movedir * ( pev->speed * m_flFanFriction ) );

    vecAVel = pev->avelocity; // cache entity's rotational velocity

    // if we've met or exceeded target speed, set target speed and stop thinking
    if( fabs( vecAVel.x ) >= fabs( pev->movedir.x * pev->speed ) &&
        fabs( vecAVel.y ) >= fabs( pev->movedir.y * pev->speed ) &&
        fabs( vecAVel.z ) >= fabs( pev->movedir.z * pev->speed ) )
    {
        pev->avelocity = pev->movedir * pev->speed; // set speed in case we overshot
        EmitSoundDyn( CHAN_STATIC, STRING( m_sounds ),
            m_flVolume, m_flAttenuation, SND_CHANGE_PITCH | SND_CHANGE_VOL, FANPITCHMAX );

        SetThink( &CFuncRotating::Rotate );
        Rotate();
    }
    else
    {
        RampPitchVol( true );
    }
}

void CFuncRotating::SpinDown()
{
    Vector vecAVel; // rotational velocity
    vec_t vecdir;

    pev->nextthink = pev->ltime + 0.1;

    pev->avelocity = pev->avelocity - ( pev->movedir * ( pev->speed * m_flFanFriction ) ); // spin down slower than spinup

    vecAVel = pev->avelocity; // cache entity's rotational velocity

    if( pev->movedir.x != 0 )
        vecdir = pev->movedir.x;
    else if( pev->movedir.y != 0 )
        vecdir = pev->movedir.y;
    else
        vecdir = pev->movedir.z;

    // if we've met or exceeded target speed, set target speed and stop thinking
    // (note: must check for movedir > 0 or < 0)
    if( ( ( vecdir > 0 ) && ( vecAVel.x <= 0 && vecAVel.y <= 0 && vecAVel.z <= 0 ) ) ||
        ( ( vecdir < 0 ) && ( vecAVel.x >= 0 && vecAVel.y >= 0 && vecAVel.z >= 0 ) ) )
    {
        pev->avelocity = g_vecZero; // set speed in case we overshot

        // stop sound, we're done
        EmitSoundDyn(CHAN_STATIC, STRING(m_sounds /* Stop */),
            0, 0, SND_STOP, m_pitch );

        SetThink( &CFuncRotating::Rotate );
        Rotate();
    }
    else
    {
        RampPitchVol( false );
    }
}

void CFuncRotating::Rotate()
{
    pev->nextthink = pev->ltime + 10;
}

void CFuncRotating::RotatingUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    // is this a brush that should accelerate and decelerate when turned on/off (fan)?
    if( FBitSet( pev->spawnflags, SF_BRUSH_ACCDCC ) )
    {
        // fan is spinning, so stop it.
        if( pev->avelocity != g_vecZero )
        {
            SetThink( &CFuncRotating::SpinDown );
            // EmitSoundDyn(CHAN_WEAPON, (char *)STRING(pev->noise2),
            //    m_flVolume, m_flAttenuation, 0, m_pitch);

            pev->nextthink = pev->ltime + 0.1;
        }
        else // fan is not moving, so start it
        {
            SetThink( &CFuncRotating::SpinUp );
            EmitSoundDyn( CHAN_STATIC, STRING( m_sounds ),
                0.01, m_flAttenuation, 0, FANPITCHMIN );

            pev->nextthink = pev->ltime + 0.1;
        }
    }
    else if( !FBitSet( pev->spawnflags, SF_BRUSH_ACCDCC ) ) // this is a normal start/stop brush.
    {
        if( pev->avelocity != g_vecZero )
        {
            // play stopping sound here
            SetThink( &CFuncRotating::SpinDown );

            // EmitSoundDyn(CHAN_WEAPON, (char *)STRING(pev->noise2),
            //    m_flVolume, m_flAttenuation, 0, m_pitch);

            pev->nextthink = pev->ltime + 0.1;
            // pev->avelocity = g_vecZero;
        }
        else
        {
            EmitSoundDyn( CHAN_STATIC, STRING( m_sounds ),
                m_flVolume, m_flAttenuation, 0, FANPITCHMAX );
            pev->avelocity = pev->movedir * pev->speed;

            SetThink( &CFuncRotating::Rotate );
            Rotate();
        }
    }
}

void CFuncRotating::Blocked( CBaseEntity* pOther )

{
    pOther->TakeDamage( this, this, pev->dmg, DMG_CRUSH );
}

class CPendulum : public CBaseEntity
{
    DECLARE_CLASS( CPendulum, CBaseEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Swing();
    void PendulumUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
    void Stop();
    void Touch( CBaseEntity* pOther ) override;
    void RopeTouch( CBaseEntity* pOther ); // this touch func makes the pendulum a rope
    int ObjectCaps() override { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
    void Blocked( CBaseEntity* pOther ) override;

    float m_accel;      // Acceleration
    float m_distance; //
    float m_time;
    float m_damp;
    float m_maxSpeed;
    float m_dampSpeed;
    Vector m_center;
    Vector m_start;

    EHANDLE m_LastToucher;
};

LINK_ENTITY_TO_CLASS( func_pendulum, CPendulum );

BEGIN_DATAMAP( CPendulum )
    DEFINE_FIELD( m_accel, FIELD_FLOAT ),
    DEFINE_FIELD( m_distance, FIELD_FLOAT ),
    DEFINE_FIELD( m_time, FIELD_TIME ),
    DEFINE_FIELD( m_damp, FIELD_FLOAT ),
    DEFINE_FIELD( m_maxSpeed, FIELD_FLOAT ),
    DEFINE_FIELD( m_dampSpeed, FIELD_FLOAT ),
    DEFINE_FIELD( m_center, FIELD_VECTOR ),
    DEFINE_FIELD( m_start, FIELD_VECTOR ),
    DEFINE_FIELD( m_LastToucher, FIELD_EHANDLE ),
    DEFINE_FUNCTION( Swing ),
    DEFINE_FUNCTION( PendulumUse ),
    DEFINE_FUNCTION( Stop ),
    DEFINE_FUNCTION( RopeTouch ),
END_DATAMAP();

bool CPendulum::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "distance" ) )
    {
        m_distance = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "damp" ) )
    {
        m_damp = atof( pkvd->szValue ) * 0.001;
        return true;
    }

    return CBaseEntity::KeyValue( pkvd );
}

SpawnAction CPendulum::Spawn()
{
    // set the axis of rotation
    CBaseToggle::AxisDir( this );

    if( FBitSet( pev->spawnflags, SF_DOOR_PASSABLE ) )
        pev->solid = SOLID_NOT;
    else
        pev->solid = SOLID_BSP;
    pev->movetype = MOVETYPE_PUSH;
    SetOrigin( pev->origin );
    SetModel( STRING( pev->model ) );

    if( m_distance != 0 )
    {
        if( pev->speed == 0 )
            pev->speed = 100;

        m_accel = ( pev->speed * pev->speed ) / ( 2 * fabs( m_distance ) ); // Calculate constant acceleration from speed and distance
        m_maxSpeed = pev->speed;
        m_start = pev->angles;
        m_center = pev->angles + ( m_distance * 0.5 ) * pev->movedir;

        if( FBitSet( pev->spawnflags, SF_BRUSH_ROTATE_INSTANT ) )
        {
            SetThink( &CPendulum::SUB_CallUseToggle );
            pev->nextthink = gpGlobals->time + 0.1;
        }
        pev->speed = 0;
        SetUse( &CPendulum::PendulumUse );

        if( FBitSet( pev->spawnflags, SF_PENDULUM_SWING ) )
        {
            SetTouch( &CPendulum::RopeTouch );
        }
    }
    return SpawnAction::Spawn;
}

void CPendulum::PendulumUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( 0 != pev->speed ) // Pendulum is moving, stop it and auto-return if necessary
    {
        if( FBitSet( pev->spawnflags, SF_PENDULUM_AUTO_RETURN ) )
        {
            float delta;

            delta = CBaseToggle::AxisDelta( pev->spawnflags, pev->angles, m_start );

            pev->avelocity = m_maxSpeed * pev->movedir;
            pev->nextthink = pev->ltime + ( delta / m_maxSpeed );
            SetThink( &CPendulum::Stop );
        }
        else
        {
            pev->speed = 0; // Dead stop
            SetThink( nullptr );
            pev->avelocity = g_vecZero;
        }
    }
    else
    {
        pev->nextthink = pev->ltime + 0.1; // Start the pendulum moving
        m_time = gpGlobals->time;           // Save time to calculate dt
        SetThink( &CPendulum::Swing );
        m_dampSpeed = m_maxSpeed;
    }
}

void CPendulum::Stop()
{
    pev->angles = m_start;
    pev->speed = 0;
    SetThink( nullptr );
    pev->avelocity = g_vecZero;
}

void CPendulum::Blocked( CBaseEntity* pOther )
{
    m_time = gpGlobals->time;
}

void CPendulum::Swing()
{
    float delta, dt;

    delta = CBaseToggle::AxisDelta( pev->spawnflags, pev->angles, m_center );
    dt = gpGlobals->time - m_time; // How much time has passed?
    m_time = gpGlobals->time;       // Remember the last time called

    if( delta > 0 && m_accel > 0 )
        pev->speed -= m_accel * dt; // Integrate velocity
    else
        pev->speed += m_accel * dt;

    if( pev->speed > m_maxSpeed )
        pev->speed = m_maxSpeed;
    else if( pev->speed < -m_maxSpeed )
        pev->speed = -m_maxSpeed;
    // scale the destdelta vector by the time spent traveling to get velocity
    pev->avelocity = pev->speed * pev->movedir;

    // Call this again
    pev->nextthink = pev->ltime + 0.1;

    if( 0 != m_damp )
    {
        m_dampSpeed -= m_damp * m_dampSpeed * dt;
        if( m_dampSpeed < 30.0 )
        {
            pev->angles = m_center;
            pev->speed = 0;
            SetThink( nullptr );
            pev->avelocity = g_vecZero;
        }
        else if( pev->speed > m_dampSpeed )
            pev->speed = m_dampSpeed;
        else if( pev->speed < -m_dampSpeed )
            pev->speed = -m_dampSpeed;
    }
}

void CPendulum::Touch( CBaseEntity* pOther )
{
    if( pev->dmg <= 0 )
        return;

    // we can't hurt this thing, so we're not concerned with it
    if( 0 == pOther->pev->takedamage )
        return;

    // calculate damage based on rotation speed
    float damage = pev->dmg * pev->speed * 0.01;

    if( damage < 0 )
        damage = -damage;

    pOther->TakeDamage( this, this, damage, DMG_CRUSH );

    pOther->pev->velocity = ( pOther->pev->origin - VecBModelOrigin( this ) ).Normalize() * damage;
}

void CPendulum::RopeTouch( CBaseEntity* pOther )
{
    if( !pOther->IsPlayer() )
    { // not a player!
        Logger->debug( "Not a client" );
        return;
    }

    if( pOther == m_LastToucher )
    { // this player already on the rope.
        return;
    }

    m_LastToucher = pOther;
    pOther->pev->velocity = g_vecZero;
    pOther->pev->movetype = MOVETYPE_NONE;
}
