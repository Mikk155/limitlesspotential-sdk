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

#include "CBaseAnimating.h"
#include "CTFDefs.h"

class CItemSpawnCTF;

class CItemCTF : public CBaseAnimating
{
    DECLARE_CLASS( CItemCTF, CBaseAnimating );
    DECLARE_DATAMAP();

public:
    static CItemSpawnCTF* m_pLastSpawn;

public:
    CItemCTF* MyItemCTFPointer() override { return this; }

    bool KeyValue( KeyValueData* pkvd ) override;
    void Precache() override;
    SpawnAction Spawn() override;

    void SetObjectCollisionBox() override
    {
        pev->absmin = pev->origin + Vector( -16, -16, 0 );
        pev->absmax = pev->origin + Vector( 16, 16, 48 );
    }

    void DropPreThink();

    void DropThink();

    void CarryThink();

    void ItemTouch( CBaseEntity* pOther );

    virtual void RemoveEffect( CBasePlayer* pPlayer ) {}

    virtual bool MyTouch( CBasePlayer* pPlayer ) { return false; }

    void DropItem( CBasePlayer* pPlayer, bool bForceRespawn );
    void ScatterItem( CBasePlayer* pPlayer );
    void ThrowItem( CBasePlayer* pPlayer );

    CTFTeam team_no;
    int m_iLastTouched;
    float m_flNextTouchTime;
    float m_flPickupTime;
    CTFItem::CTFItem m_iItemFlag;
    const char* m_pszItemName;
};

class CItemSpawnCTF : public CPointEntity
{
public:
    bool KeyValue( KeyValueData* pkvd ) override;

    CTFTeam team_no;
};

class CItemAcceleratorCTF : public CItemCTF
{
public:
    void OnCreate() override;

    void Precache() override;

    SpawnAction Spawn() override;

    void RemoveEffect( CBasePlayer* pPlayer ) override;

    bool MyTouch( CBasePlayer* pPlayer ) override;
};

class CItemBackpackCTF : public CItemCTF
{
public:
    void OnCreate() override;

    void Precache() override;

    void RemoveEffect( CBasePlayer* pPlayer ) override;

    bool MyTouch( CBasePlayer* pPlayer ) override;

    SpawnAction Spawn() override;
};

class CItemLongJumpCTF : public CItemCTF
{
public:
    void OnCreate() override;

    void Precache() override;

    void RemoveEffect( CBasePlayer* pPlayer ) override;

    bool MyTouch( CBasePlayer* pPlayer ) override;

    SpawnAction Spawn() override;
};

class CItemPortableHEVCTF : public CItemCTF
{
public:
    void OnCreate() override;

    void Precache() override;

    void RemoveEffect( CBasePlayer* pPlayer ) override;

    bool MyTouch( CBasePlayer* pPlayer ) override;

    SpawnAction Spawn() override;
};

class CItemRegenerationCTF : public CItemCTF
{
public:
    void OnCreate() override;

    void Precache() override;

    void RemoveEffect( CBasePlayer* pPlayer ) override;

    bool MyTouch( CBasePlayer* pPlayer ) override;

    SpawnAction Spawn() override;
};
