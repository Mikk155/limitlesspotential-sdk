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

#pragma once

enum PenguinAnim
{
    PENGUIN_IDLE1 = 0,
    PENGUIN_FIDGETFIT,
    PENGUIN_FIDGETNIP,
    PENGUIN_DOWN,
    PENGUIN_UP,
    PENGUIN_THROW,
};

class CPenguin : public CBasePlayerWeapon
{
    DECLARE_CLASS( CPenguin, CBasePlayerWeapon );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;

    void Precache() override;

    SpawnAction Spawn() override;

    bool Deploy() override;

    void Holster() override;

    void WeaponIdle() override;

    void PrimaryAttack() override;

    void SecondaryAttack() override;

    bool GetWeaponInfo( WeaponInfo& info ) override;

    bool UseDecrement() override
    {
#if defined( CLIENT_WEAPONS )
        return true;
#else
        return false;
#endif
    }

private:
    bool m_fJustThrown;
    unsigned short m_usPenguinFire;
};
