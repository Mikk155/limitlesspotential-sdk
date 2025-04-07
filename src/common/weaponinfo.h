/***
 *
 *  Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *  This product contains software technology licensed from Id
 *  Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *  All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#pragma once

// Info about weapons player might have in his/her possession
struct weapon_data_t
{
    int m_iId;
    int m_iClip;

    float m_flNextPrimaryAttack;
    float m_flNextSecondaryAttack;
    float m_flTimeWeaponIdle;

    int m_fInReload;
    int m_fInSpecialReload;
    float m_flNextReload;
    float m_flPumpTime;
    float m_fReloadTime;

    float m_fAimedDamage;
    float m_fNextAimBonus;
    int m_fInZoom;
    int m_iWeaponState;

    int iuser1; // ON USE! for CBasePlayer
    int iuser2; // ON USE! for CBasePlayer
    int iuser3; // ON USE! for CBasePlayer
    int iuser4; // ON USE! for CBasePlayer
    float fuser1; // ON USE! for CBasePlayer
    float fuser2; // ON USE! for CBasePlayer
    float fuser3; // ON USE! for CBasePlayer
    float fuser4;
};
