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

#include "cbase.h"
#include "CSqueak.h"

enum w_squeak_e
{
    WSQUEAK_IDLE1 = 0,
    WSQUEAK_FIDGET,
    WSQUEAK_JUMP,
    WSQUEAK_RUN,
};

#ifndef CLIENT_DLL

class CSqueakGrenade : public CGrenade
{
    DECLARE_CLASS( CSqueakGrenade, CGrenade );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void SuperBounceTouch( CBaseEntity* pOther );
    void HuntThink();
    int BloodColor() override { return BLOOD_COLOR_YELLOW; }
    void Killed( CBaseEntity* attacker, int iGib ) override;
    void GibMonster() override;

    bool HasAlienGibs() override { return true; }

    bool IsBioWeapon() const override { return true; }

private:
    static float m_flNextBounceSoundTime;

    // CBaseEntity *m_pTarget;
    float m_flDie;
    Vector m_vecTarget;
    float m_flNextHunt;
    float m_flNextHit;
    Vector m_posPrev;
    EHANDLE m_hOwner;
};

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

LINK_ENTITY_TO_CLASS( monster_snark, CSqueakGrenade );

BEGIN_DATAMAP( CSqueakGrenade )
    DEFINE_FIELD( m_flDie, FIELD_TIME ),
    DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
    DEFINE_FIELD( m_flNextHunt, FIELD_TIME ),
    DEFINE_FIELD( m_flNextHit, FIELD_TIME ),
    DEFINE_FIELD( m_posPrev, FIELD_POSITION_VECTOR ),
    DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
    DEFINE_FUNCTION( SuperBounceTouch ),
    DEFINE_FUNCTION( HuntThink ),
END_DATAMAP();

#define SQUEEK_DETONATE_DELAY 15.0

void CSqueakGrenade::OnCreate()
{
    CGrenade::OnCreate();

    pev->model = MAKE_STRING( "models/w_squeak.mdl" );

    SetClassification( "alien_bioweapon" );
}

SpawnAction CSqueakGrenade::Spawn()
{
    Precache();
    // motor
    pev->movetype = MOVETYPE_BOUNCE;
    pev->solid = SOLID_BBOX;

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "snark_health"sv, 2 );

    SetModel( STRING( pev->model ) );
    SetSize( Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );
    SetOrigin( pev->origin );

    SetTouch( &CSqueakGrenade::SuperBounceTouch );
    SetThink( &CSqueakGrenade::HuntThink );
    pev->nextthink = gpGlobals->time + 0.1;
    m_flNextHunt = gpGlobals->time + 1E6;

    pev->flags |= FL_MONSTER;
    pev->takedamage = DAMAGE_AIM;
    pev->gravity = 0.5;
    pev->friction = 0.5;

    pev->dmg = g_cfg.GetValue<float>( "snark_dmg_pop"sv, 5, this );

    m_flDie = gpGlobals->time + SQUEEK_DETONATE_DELAY;

    m_flFieldOfView = 0; // 180 degrees

    if( pev->owner )
        m_hOwner = Instance( pev->owner );

    m_flNextBounceSoundTime = gpGlobals->time; // reset each time a snark is spawned.

    pev->sequence = WSQUEAK_RUN;
    ResetSequenceInfo();

    return SpawnAction::Spawn;
}

void CSqueakGrenade::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "squeek/sqk_blast1.wav" );
    PrecacheSound( "common/bodysplat.wav" );
    PrecacheSound( "squeek/sqk_die1.wav" );
    PrecacheSound( "squeek/sqk_hunt1.wav" );
    PrecacheSound( "squeek/sqk_hunt2.wav" );
    PrecacheSound( "squeek/sqk_hunt3.wav" );
    PrecacheSound( "squeek/sqk_deploy1.wav" );
}

void CSqueakGrenade::Killed( CBaseEntity* attacker, int iGib )
{
    pev->model = string_t::Null; // make invisible
    SetThink( &CSqueakGrenade::SUB_Remove );
    SetTouch( nullptr );
    pev->nextthink = gpGlobals->time + 0.1;

    // since squeak grenades never leave a body behind, clear out their takedamage now.
    // Squeaks do a bit of radius damage when they pop, and that radius damage will
    // continue to call this function unless we acknowledge the Squeak's death now. (sjb)
    pev->takedamage = DAMAGE_NO;

    // play squeek blast
    EmitSound( CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5 );

    CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0 );

    UTIL_BloodDrips( pev->origin, g_vecZero, BloodColor(), 80 );

    if( m_hOwner != nullptr )
        RadiusDamage( this, m_hOwner, pev->dmg, DMG_BLAST );
    else
        RadiusDamage( this, this, pev->dmg, DMG_BLAST );

    // reset owner so death message happens
    if( m_hOwner != nullptr )
        pev->owner = m_hOwner->edict();

    CBaseMonster::Killed( attacker, GIB_ALWAYS );
}

void CSqueakGrenade::GibMonster()
{
    EmitSoundDyn( CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200 );
}

void CSqueakGrenade::HuntThink()
{
    // AILogger->debug("think");

    if( !IsInWorld() )
    {
        SetTouch( nullptr );
        UTIL_Remove( this );
        return;
    }

    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.1;

    // explode when ready
    if( gpGlobals->time >= m_flDie )
    {
        g_vecAttackDir = pev->velocity.Normalize();
        pev->health = -1;
        Killed( this, 0 );
        return;
    }

    // float
    if( pev->waterlevel != WaterLevel::Dry )
    {
        if( pev->movetype == MOVETYPE_BOUNCE )
        {
            pev->movetype = MOVETYPE_FLY;
        }
        pev->velocity = pev->velocity * 0.9;
        pev->velocity.z += 8.0;
    }
    else if( pev->movetype == MOVETYPE_FLY )
    {
        pev->movetype = MOVETYPE_BOUNCE;
    }

    // return if not time to hunt
    if( m_flNextHunt > gpGlobals->time )
        return;

    m_flNextHunt = gpGlobals->time + 2.0;

    Vector vecDir;
    Vector vecFlat = pev->velocity;
    vecFlat.z = 0;
    vecFlat = vecFlat.Normalize();

    UTIL_MakeVectors( pev->angles );

    if( m_hEnemy == nullptr || !m_hEnemy->IsAlive() )
    {
        // find target, bounce a bit towards it.
        Look( 512 );
        m_hEnemy = BestVisibleEnemy();
    }

    // squeek if it's about time blow up
    if( ( m_flDie - gpGlobals->time <= 0.5 ) && ( m_flDie - gpGlobals->time >= 0.3 ) )
    {
        EmitSoundDyn( CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( 0, 0x3F ) );
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
    }

    // higher pitch as squeeker gets closer to detonation time
    float flpitch = 155.0 - 60.0 * ( ( m_flDie - gpGlobals->time ) / SQUEEK_DETONATE_DELAY );
    if( flpitch < 80 )
        flpitch = 80;

    if( m_hEnemy != nullptr )
    {
        if( FVisible( m_hEnemy ) )
        {
            vecDir = m_hEnemy->EyePosition() - pev->origin;
            m_vecTarget = vecDir.Normalize();
        }

        float flVel = pev->velocity.Length();
        float flAdj = 50.0 / ( flVel + 10.0 );

        if( flAdj > 1.2 )
            flAdj = 1.2;

        // AILogger->debug("think : enemy");
        // AILogger->debug("{:.0f} {:.2f}", flVel, m_vecTarget);

        pev->velocity = pev->velocity * flAdj + m_vecTarget * 300;
    }

    if( ( pev->flags & FL_ONGROUND ) != 0 )
    {
        pev->avelocity = Vector( 0, 0, 0 );
    }
    else
    {
        if( pev->avelocity == Vector( 0, 0, 0 ) )
        {
            pev->avelocity.x = RANDOM_FLOAT( -100, 100 );
            pev->avelocity.z = RANDOM_FLOAT( -100, 100 );
        }
    }

    if( ( pev->origin - m_posPrev ).Length() < 1.0 )
    {
        pev->velocity.x = RANDOM_FLOAT( -100, 100 );
        pev->velocity.y = RANDOM_FLOAT( -100, 100 );
    }
    m_posPrev = pev->origin;

    pev->angles = UTIL_VecToAngles( pev->velocity );
    pev->angles.z = 0;
    pev->angles.x = 0;

    // Update classification
    if( !HasCustomClassification() )
    {
        // TODO: maybe use a different classification that has the expected relationships for these classes.
        const char* classification = "alien_bioweapon";

        if( m_hEnemy != nullptr )
        {
            if( g_EntityClassifications.ClassNameIs( m_hEnemy->Classify(), {"player", "human_passive", "human_military"} ) )
            {
                // barney's get mad, grunts get mad at it
                classification = "alien_military";
            }
        }

        SetClassification( classification );
    }
}

void CSqueakGrenade::SuperBounceTouch( CBaseEntity* pOther )
{
    float flpitch;

    TraceResult tr = UTIL_GetGlobalTrace();

    // don't hit the guy that launched this grenade
    if( pev->owner && pOther->edict() == pev->owner )
        return;

    // at least until we've bounced once
    pev->owner = nullptr;

    pev->angles.x = 0;
    pev->angles.z = 0;

    // avoid bouncing too much
    if( m_flNextHit > gpGlobals->time )
        return;

    // higher pitch as squeeker gets closer to detonation time
    flpitch = 155.0 - 60.0 * ( ( m_flDie - gpGlobals->time ) / SQUEEK_DETONATE_DELAY );

    if( 0 != pOther->pev->takedamage && m_flNextAttack < gpGlobals->time )
    {
        // attack!

        // make sure it's me who has touched them
        if( tr.pHit == pOther->edict() )
        {
            // and it's not another squeakgrenade
            if( tr.pHit->v.modelindex != pev->modelindex )
            {
                // AILogger->debug("hit enemy");
                ClearMultiDamage();
                pOther->TraceAttack( this, g_cfg.GetValue<float>( "snark_dmg_bite"sv, 10, this ), gpGlobals->v_forward, &tr, DMG_SLASH );
                if( m_hOwner != nullptr )
                    ApplyMultiDamage( this, m_hOwner );
                else
                    ApplyMultiDamage( this, this );

                pev->dmg += g_cfg.GetValue<float>( "snark_dmg_pop"sv, 5, this ); // add more explosion damage
                // m_flDie += 2.0; // add more life

                // make bite sound
                EmitSoundDyn( CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch );
                m_flNextAttack = gpGlobals->time + 0.5;
            }
        }
        else
        {
            // AILogger->debug("been hit");
        }
    }

    m_flNextHit = gpGlobals->time + 0.1;
    m_flNextHunt = gpGlobals->time;

    if( g_GameMode->IsMultiplayer() )
    {
        // in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
        if( gpGlobals->time < m_flNextBounceSoundTime )
        {
            // too soon!
            return;
        }
    }

    if( ( pev->flags & FL_ONGROUND ) == 0 )
    {
        // play bounce sound
        float flRndSound = RANDOM_FLOAT( 0, 1 );

        if( flRndSound <= 0.33 )
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch );
        else if( flRndSound <= 0.66 )
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch );
        else
            EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch );
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
    }
    else
    {
        // skittering sound
        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 100, 0.1 );
    }

    m_flNextBounceSoundTime = gpGlobals->time + 0.5; // half second.
}
#endif

LINK_ENTITY_TO_CLASS( weapon_snark, CSqueak );

void CSqueak::OnCreate()
{
    CBasePlayerWeapon::OnCreate();
    m_iId = WEAPON_SNARK;
    m_iDefaultAmmo = SNARK_DEFAULT_GIVE;
    m_WorldModel = pev->model = MAKE_STRING( "models/w_sqknest.mdl" );
}

SpawnAction CSqueak::Spawn()
{
    CBasePlayerWeapon::Spawn();

    pev->sequence = 1;
    pev->animtime = gpGlobals->time;
    pev->framerate = 1.0;

    return SpawnAction::Spawn;
}

void CSqueak::Precache()
{
    CBasePlayerWeapon::Precache();
    PrecacheModel( STRING( m_WorldModel ) );
    PrecacheModel( "models/v_squeak.mdl" );
    PrecacheModel( "models/p_squeak.mdl" );
    PrecacheSound( "squeek/sqk_hunt2.wav" );
    PrecacheSound( "squeek/sqk_hunt3.wav" );
    UTIL_PrecacheOther( "monster_snark" );

    m_usSnarkFire = PRECACHE_EVENT( 1, "events/snarkfire.sc" );
}

bool CSqueak::GetWeaponInfo( WeaponInfo& info )
{
    info.Name = STRING( pev->classname );
    info.AttackModeInfo[0].AmmoType = "Snarks";
    info.AttackModeInfo[0].MagazineSize = WEAPON_NOCLIP;
    info.Slot = 4;
    info.Position = 3;
    info.Id = WEAPON_SNARK;
    info.Weight = SNARK_WEIGHT;
    info.Flags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

    return true;
}

bool CSqueak::Deploy()
{
    // play hunt sound
    float flRndSound = RANDOM_FLOAT( 0, 1 );

    if( flRndSound <= 0.5 )
        EmitSound( CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM );
    else
        EmitSound( CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM );

    m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

    const bool result = DefaultDeploy( "models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak" );

    if( result )
    {
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.7;
    }

    return result;
}

void CSqueak::Holster()
{
    m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

    if( 0 == m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
    {
        m_pPlayer->ClearWeaponBit( m_iId );
        SetThink( &CSqueak::DestroyItem );
        pev->nextthink = gpGlobals->time + 0.1;
        return;
    }

    SendWeaponAnim( SQUEAK_DOWN );
    m_pPlayer->EmitSound( CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM );
}

void CSqueak::PrimaryAttack()
{
    if( 0 != m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
    {
        UTIL_MakeVectors( m_pPlayer->pev->v_angle );
        TraceResult tr;
        Vector trace_origin;

        // HACK HACK:  Ugly hacks to handle change in origin based on new physics code for players
        // Move origin up if crouched and start trace a bit outside of body ( 20 units instead of 16 )
        trace_origin = m_pPlayer->pev->origin;
        if( ( m_pPlayer->pev->flags & FL_DUCKING ) != 0 )
        {
            trace_origin = trace_origin - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
        }

        // find place to toss monster
        UTIL_TraceLine( trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, nullptr, &tr );

        int flags;
#ifdef CLIENT_WEAPONS
        flags = FEV_NOTHOST;
#else
        flags = 0;
#endif

        PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSnarkFire, 0.0, g_vecZero, g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

        if( tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25 )
        {
            // player "shoot" animation
            m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
            CBaseEntity* pSqueak = CBaseEntity::Create( "monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer );
            pSqueak->pev->velocity = gpGlobals->v_forward * 200 + m_pPlayer->pev->velocity;
#endif

            // play hunt sound
            float flRndSound = RANDOM_FLOAT( 0, 1 );

            if( flRndSound <= 0.5 )
                EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105 );
            else
                EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105 );

            m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

            m_pPlayer->AdjustAmmoByIndex( m_iPrimaryAmmoType, -1 );

            m_fJustThrown = true;

            m_flNextPrimaryAttack = GetNextAttackDelay( 0.3 );
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
        }
    }
}

void CSqueak::SecondaryAttack()
{
}

void CSqueak::WeaponIdle()
{
    if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
        return;

    if( m_fJustThrown )
    {
        m_fJustThrown = false;

        if( 0 == m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
        {
            RetireWeapon();
            return;
        }

        SendWeaponAnim( SQUEAK_UP );
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
        return;
    }

    int iAnim;
    float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
    if( flRand <= 0.75 )
    {
        iAnim = SQUEAK_IDLE1;
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * ( 2 );
    }
    else if( flRand <= 0.875 )
    {
        iAnim = SQUEAK_FIDGETFIT;
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 70.0 / 16.0;
    }
    else
    {
        iAnim = SQUEAK_FIDGETNIP;
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 16.0;
    }
    SendWeaponAnim( iAnim );
}
