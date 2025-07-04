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

#include "CPenguin.h"

// TODO: this isn't in vanilla Op4 so it won't save properly there
BEGIN_DATAMAP( CPenguin )
    DEFINE_FIELD( m_fJustThrown, FIELD_BOOLEAN ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( weapon_penguin, CPenguin );

void CPenguin::OnCreate()
{
    CBasePlayerWeapon::OnCreate();
    m_iId = WEAPON_PENGUIN;
    m_iDefaultAmmo = PENGUIN_MAX_CLIP;
    m_WorldModel = pev->model = MAKE_STRING( "models/w_penguinnest.mdl" );
}

void CPenguin::Precache()
{
    CBasePlayerWeapon::Precache();
    PrecacheModel( STRING( m_WorldModel ) );
    PrecacheModel( "models/v_penguin.mdl" );
    PrecacheModel( "models/p_penguin.mdl" );
    PrecacheSound( "squeek/sqk_hunt2.wav" );
    PrecacheSound( "squeek/sqk_hunt3.wav" );
    UTIL_PrecacheOther( "monster_penguin" );
    m_usPenguinFire = g_engfuncs.pfnPrecacheEvent( 1, "events/penguinfire.sc" );
}

SpawnAction CPenguin::Spawn()
{
    CBasePlayerWeapon::Spawn();

    pev->sequence = 1;
    pev->animtime = gpGlobals->time;
    pev->framerate = 1;

    return SpawnAction::Spawn;
}

bool CPenguin::Deploy()
{
    if( g_engfuncs.pfnRandomFloat( 0.0, 1.0 ) <= 0.5 )
        EmitSound( CHAN_VOICE, "squeek/sqk_hunt2.wav", VOL_NORM, ATTN_NORM );
    else
        EmitSound( CHAN_VOICE, "squeek/sqk_hunt3.wav", VOL_NORM, ATTN_NORM );

    m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

    return DefaultDeploy( "models/v_penguin.mdl", "models/p_penguin.mdl", PENGUIN_UP, "penguin" );
}

void CPenguin::Holster()
{
    m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

    if( 0 == m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
    {
        m_pPlayer->ClearWeaponBit( m_iId );
        SetThink( &CPenguin::DestroyItem );
        pev->nextthink = gpGlobals->time + 0.1;
        return;
    }

    SendWeaponAnim( PENGUIN_DOWN );
    m_pPlayer->EmitSound( CHAN_WEAPON, "common/null.wav", VOL_NORM, ATTN_NORM );
}

void CPenguin::WeaponIdle()
{
    if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
        return;

    if( m_fJustThrown )
    {
        m_fJustThrown = false;

        if( 0 != m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
        {
            SendWeaponAnim( PENGUIN_UP );
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
        }
        else
        {
            RetireWeapon();
        }
    }
    else
    {
        const float fRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );

        int iAnim;

        if( fRand <= 0.75 )
        {
            iAnim = 0;
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.75;
        }
        else if( fRand <= 0.875 )
        {
            iAnim = 1;
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.375;
        }
        else
        {
            iAnim = 2;
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
        }

        SendWeaponAnim( iAnim );
    }
}

void CPenguin::PrimaryAttack()
{
    if( 0 != m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
    {
        UTIL_MakeVectors( m_pPlayer->pev->v_angle );

        Vector vecSrc = m_pPlayer->pev->origin;

        if( ( m_pPlayer->pev->flags & FL_DUCKING ) != 0 )
        {
            vecSrc.z += 18;
        }

        const Vector vecStart = vecSrc + ( gpGlobals->v_forward * 20 );
        const Vector vecEnd = vecSrc + ( gpGlobals->v_forward * 64 );

        TraceResult tr;
        UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, nullptr, &tr );

        int flags;
#if defined( CLIENT_WEAPONS )
        flags = FEV_NOTHOST;
#else
        flags = 0;
#endif

        PLAYBACK_EVENT( flags, edict(), m_usPenguinFire );

        if( 0 == tr.fAllSolid && 0 == tr.fStartSolid && tr.flFraction > 0.25 )
        {
            m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
            auto penguin = CBaseEntity::Create( "monster_penguin", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer );

            penguin->pev->velocity = m_pPlayer->pev->velocity + ( gpGlobals->v_forward * 200 );
#endif

            if( g_engfuncs.pfnRandomFloat( 0, 1 ) <= 0.5 )
                EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt2.wav", VOL_NORM, ATTN_NORM, 0, 105 );
            else
                EmitSoundDyn( CHAN_VOICE, "squeek/sqk_hunt3.wav", VOL_NORM, ATTN_NORM, 0, 105 );

            m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
            m_pPlayer->AdjustAmmoByIndex( m_iPrimaryAmmoType, -1 );
            m_fJustThrown = true;

            m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.9;
            m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.8;
        }
    }
}

void CPenguin::SecondaryAttack()
{
    // Nothing
}

bool CPenguin::GetWeaponInfo( WeaponInfo& info )
{
    info.AttackModeInfo[0].AmmoType = "Penguins";
    info.Name = STRING( pev->classname );
    info.AttackModeInfo[0].MagazineSize = WEAPON_NOCLIP;
    info.Slot = 4;
    info.Position = 4;
    info.Id = WEAPON_PENGUIN;
    info.Weight = PENGUIN_WEIGHT;
    info.Flags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

    return true;
}
