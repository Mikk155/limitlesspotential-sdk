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

#include "CTFDefs.h"
#include "info/player_start.h"

class CTFSpawn : public CPlayerSpawnPoint
{
    DECLARE_CLASS( CTFSpawn, CPlayerSpawnPoint );

public:
    bool Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void SpawnPlayer( CBasePlayer* player ) override;
    bool CanPlayerSpawn( CBasePlayer* player ) override;

    CTFTeam team_no;
    bool m_fState;
};
