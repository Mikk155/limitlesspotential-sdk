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
#include "CShotgun.h"
#include "UserMessages.h"

LINK_ENTITY_TO_CLASS( weapon_shotgun, CShotgun );

BEGIN_DATAMAP( CShotgun )
    DEFINE_FIELD( m_flNextReload, FIELD_TIME ),
    DEFINE_FIELD( m_fInSpecialReload, FIELD_INTEGER ),
    // DEFINE_FIELD(m_iShell, FIELD_INTEGER),
    DEFINE_FIELD( m_flPumpTime, FIELD_TIME ),
END_DATAMAP();

void CShotgun::OnCreate()
{
    CBasePlayerWeapon::OnCreate();
    m_iId = WEAPON_SHOTGUN;
    m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;
    m_WorldModel = pev->model = MAKE_STRING( "models/w_shotgun.mdl" );
}

void CShotgun::Precache()
{
    CBasePlayerWeapon::Precache();
    PrecacheModel( "models/v_shotgun.mdl" );
    PrecacheModel( STRING( m_WorldModel ) );
    PrecacheModel( "models/p_shotgun.mdl" );

    m_iShell = PrecacheModel( "models/shotgunshell.mdl" ); // shotgun shell

    PrecacheSound( "items/9mmclip1.wav" );

    PrecacheSound( "weapons/dbarrel1.wav" ); // shotgun
    PrecacheSound( "weapons/sbarrel1.wav" ); // shotgun

    PrecacheSound( "weapons/reload1.wav" ); // shotgun reload
    PrecacheSound( "weapons/reload3.wav" ); // shotgun reload

    //    PrecacheSound ("weapons/sshell1.wav");    // shotgun reload - played on client
    //    PrecacheSound ("weapons/sshell3.wav");    // shotgun reload - played on client

    PrecacheSound( "weapons/357_cock1.wav" ); // gun empty sound
    PrecacheSound( "weapons/scock1.wav" );    // cock gun

    m_usSingleFire = PRECACHE_EVENT( 1, "events/shotgun1.sc" );
    m_usDoubleFire = PRECACHE_EVENT( 1, "events/shotgun2.sc" );
}

bool CShotgun::GetWeaponInfo( WeaponInfo& info )
{
    info.Name = STRING( pev->classname );
    info.AttackModeInfo[0].AmmoType = "buckshot";
    info.AttackModeInfo[0].MagazineSize = SHOTGUN_MAX_CLIP;
    info.Slot = 2;
    info.Position = 1;
    info.Id = WEAPON_SHOTGUN;
    info.Weight = SHOTGUN_WEIGHT;

    return true;
}

void CShotgun::IncrementAmmo( CBasePlayer* pPlayer )
{
    if( pPlayer->GiveAmmo( 1, "buckshot" ) >= 0 )
    {
        pPlayer->EmitSound( CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
    }
}

bool CShotgun::Deploy()
{
    return DefaultDeploy( "models/v_shotgun.mdl", "models/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun" );
}

void CShotgun::PrimaryAttack()
{
    // don't fire underwater
    if( m_pPlayer->pev->waterlevel == WaterLevel::Head )
    {
        PlayEmptySound();
        m_flNextPrimaryAttack = GetNextAttackDelay( 0.15 );
        return;
    }

    if( GetMagazine1() <= 0 )
    {
        Reload();
        if( GetMagazine1() == 0 )
            PlayEmptySound();
        return;
    }

    m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
    m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

    AdjustMagazine1( -1 );

    int flags;
#if defined( CLIENT_WEAPONS )
    flags = FEV_NOTHOST;
#else
    flags = 0;
#endif


    m_pPlayer->pev->effects = m_pPlayer->pev->effects | EF_MUZZLEFLASH;

    Vector vecSrc = m_pPlayer->GetGunPosition();
    Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

    Vector vecDir;

    if( g_cfg.GetValue<bool>( "shotgun_single_tight_spread"sv, false ) )
    {
        vecDir = m_pPlayer->FireBulletsPlayer( 4, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer, m_pPlayer->random_seed );
    }
    else
    {
        // regular old, untouched spread.
        vecDir = m_pPlayer->FireBulletsPlayer( 6, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer, m_pPlayer->random_seed );
    }

    PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, g_vecZero, g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );


    if( 0 == GetMagazine1() && m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) <= 0 )
        // HEV suit - indicate out of ammo condition
        m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0 );

    // if (GetMagazine1() != 0)
    m_flPumpTime = gpGlobals->time + 0.5;

    m_flNextPrimaryAttack = GetNextAttackDelay( 0.75 );
    m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75;
    if( GetMagazine1() != 0 )
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
    else
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
    m_fInSpecialReload = 0;
}

void CShotgun::SecondaryAttack()
{
    // don't fire underwater
    if( m_pPlayer->pev->waterlevel == WaterLevel::Head )
    {
        PlayEmptySound();
        m_flNextPrimaryAttack = GetNextAttackDelay( 0.15 );
        return;
    }

    if( GetMagazine1() < 2 )
    {
        Reload();
        PlayEmptySound();
        return;
    }

    m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
    m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

    AdjustMagazine1( -2 );

    int flags;
#if defined( CLIENT_WEAPONS )
    flags = FEV_NOTHOST;
#else
    flags = 0;
#endif

    m_pPlayer->pev->effects = m_pPlayer->pev->effects | EF_MUZZLEFLASH;

    // player "shoot" animation
    m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

    Vector vecSrc = m_pPlayer->GetGunPosition();
    Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

    Vector vecDir;

    if( g_cfg.GetValue<bool>( "shotgun_double_wide_spread"sv, false ) )
    {
        // tuned for deathmatch
        vecDir = m_pPlayer->FireBulletsPlayer( 8, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer, m_pPlayer->random_seed );
    }
    else
    {
        // untouched default single player
        vecDir = m_pPlayer->FireBulletsPlayer( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer, m_pPlayer->random_seed );
    }

    PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usDoubleFire, 0.0, g_vecZero, g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

    if( 0 == GetMagazine1() && m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) <= 0 )
        // HEV suit - indicate out of ammo condition
        m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0 );

    // if (GetMagazine1() != 0)
    m_flPumpTime = gpGlobals->time + 0.95;

    m_flNextPrimaryAttack = GetNextAttackDelay( 1.5 );
    m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
    if( GetMagazine1() != 0 )
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.0;
    else
        m_flTimeWeaponIdle = 1.5;

    m_fInSpecialReload = 0;
}

void CShotgun::Reload()
{
    int maxClip = SHOTGUN_MAX_CLIP;

    if( ( m_pPlayer->m_iItems & CTFItem::Backpack ) != 0 )
    {
        maxClip *= 2;
    }

    if( m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) <= 0 || GetMagazine1() == maxClip )
        return;

    // don't reload until recoil is done
    if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
        return;

    // check to see if we're ready to reload
    if( m_fInSpecialReload == 0 )
    {
        SendWeaponAnim( SHOTGUN_START_RELOAD );
        m_fInSpecialReload = 1;
        m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.6;
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.6;
        m_flNextPrimaryAttack = GetNextAttackDelay( 1.0 );
        m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
        return;
    }
    else if( m_fInSpecialReload == 1 )
    {
        if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
            return;
        // was waiting for gun to move to side
        m_fInSpecialReload = 2;

        if( RANDOM_LONG( 0, 1 ) )
            m_pPlayer->EmitSoundDyn( CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );
        else
            m_pPlayer->EmitSoundDyn( CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );

        SendWeaponAnim( SHOTGUN_RELOAD );

        m_flNextReload = UTIL_WeaponTimeBase() + 0.5;
        m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
    }
    else
    {
        // Add them to the clip
        AdjustMagazine1( 1 );
        m_pPlayer->AdjustAmmoByIndex( m_iPrimaryAmmoType, -1 );
        m_fInSpecialReload = 1;
    }
}

void CShotgun::WeaponIdle()
{
    ResetEmptySound();

    m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

    // Moved to ItemPostFrame
    /*
    if ( m_flPumpTime && m_flPumpTime < gpGlobals->time )
    {
        // play pumping sound
        m_pPlayer->EmitSoundDyn(CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,0x1f));
        m_flPumpTime = 0;
    }
    */

    if( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
    {
        if( GetMagazine1() == 0 && m_fInSpecialReload == 0 && 0 != m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
        {
            Reload();
        }
        else if( m_fInSpecialReload != 0 )
        {
            int maxClip = SHOTGUN_MAX_CLIP;

            if( ( m_pPlayer->m_iItems & CTFItem::Backpack ) != 0 )
            {
                maxClip *= 2;
            }

            if( GetMagazine1() != maxClip && 0 != m_pPlayer->GetAmmoCountByIndex( m_iPrimaryAmmoType ) )
            {
                Reload();
            }
            else
            {
                // reload debounce has timed out
                SendWeaponAnim( SHOTGUN_PUMP );

                // play cocking sound
                m_pPlayer->EmitSoundDyn( CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 0x1f ) );
                m_fInSpecialReload = 0;
                m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
            }
        }
        else
        {
            int iAnim;
            float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
            if( flRand <= 0.8 )
            {
                iAnim = SHOTGUN_IDLE_DEEP;
                m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ( 60.0 / 12.0 ); // * RANDOM_LONG(2, 5);
            }
            else if( flRand <= 0.95 )
            {
                iAnim = SHOTGUN_IDLE;
                m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ( 20.0 / 9.0 );
            }
            else
            {
                iAnim = SHOTGUN_IDLE4;
                m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ( 20.0 / 9.0 );
            }
            SendWeaponAnim( iAnim );
        }
    }
}

void CShotgun::ItemPostFrame()
{
    if( 0 != m_flPumpTime && m_flPumpTime < gpGlobals->time )
    {
        // play pumping sound
        m_pPlayer->EmitSoundDyn( CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 0x1f ) );
        m_flPumpTime = 0;
    }

    CBasePlayerWeapon::ItemPostFrame();
}

class CShotgunAmmo : public CBasePlayerAmmo
{
public:
    void OnCreate() override
    {
        CBasePlayerAmmo::OnCreate();
        m_AmmoAmount = AMMO_BUCKSHOTBOX_GIVE;
        m_AmmoName = MAKE_STRING( "buckshot" );
        pev->model = MAKE_STRING( "models/w_shotbox.mdl" );
    }
};

LINK_ENTITY_TO_CLASS( ammo_buckshot, CShotgunAmmo );
