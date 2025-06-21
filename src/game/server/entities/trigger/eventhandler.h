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

#define SF_EVENT_STARTOFF 1

enum class TriggerEventType
{
    None = 0,
    PlayerDie,
    PlayerLeftServer,
    PlayerKilled,
    PlayerActivate,
    PlayerJoin,
    PlayerSpawn
};

struct trigger_event_t
{
    TriggerEventType event;
    int entity;
};

class CTriggerEventHandler : public CPointEntity
{
    DECLARE_CLASS( CTriggerEventHandler, CPointEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    string_t m_Caller;
    TriggerEventType m_EventType = TriggerEventType::None;

private:
    bool InitialState;
};

void TriggerEvent( TriggerEventType event, CBaseEntity* activator, CBaseEntity* caller = nullptr, UseValue value = {} );
