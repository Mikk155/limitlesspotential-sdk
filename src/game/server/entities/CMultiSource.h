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

#include "CPointEntity.h"

#define MS_MAX_TARGETS 32

class CMultiSource : public CPointEntity
{
    DECLARE_CLASS( CMultiSource, CPointEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    int ObjectCaps() override { return ( CPointEntity::ObjectCaps() | FCAP_MASTER ); }
    bool IsTriggered( CBaseEntity* pActivator ) override;
    void Register();

    EHANDLE m_rgEntities[MS_MAX_TARGETS];
    int m_rgTriggered[MS_MAX_TARGETS];

    int m_iTotal;
    string_t m_globalstate;
};
