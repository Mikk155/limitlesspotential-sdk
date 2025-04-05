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

#include "cbase.h"

#define SF_SPAWNPOINT_STARTOFF ( 1 << 0 )
#define MAX_SPAWNPOINT_KEYVALUES 16

class CPlayerSpawnPoint : public CPointEntity
{
    DECLARE_CLASS( CPlayerSpawnPoint, CPointEntity );
    DECLARE_DATAMAP();

public:
    bool Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    virtual bool CanPlayerSpawn( CBasePlayer* player );
    virtual void SpawnPlayer( CBasePlayer* player );

private:
    float m_flOffSet;
    string_t m_PlayerFilterName;
    int m_PlayerFilterType;
    bool InitialState;
    int m_cTargets;
    string_t m_iKey[MAX_SPAWNPOINT_KEYVALUES];
    string_t m_iValue[MAX_SPAWNPOINT_KEYVALUES];
};

CPlayerSpawnPoint* EntSelectSpawnPoint( CBasePlayer* pPlayer, bool spawn );
CPlayerSpawnPoint* FindBestPlayerSpawn( CBasePlayer* player, const char* spawn_name );
