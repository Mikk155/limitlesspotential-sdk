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

class CBasePlayer;

class CHUDIconTrigger : public CBaseToggle
{
public:
    SpawnAction Spawn() override;

    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    bool KeyValue( KeyValueData* pkvd ) override;

    void UpdateUser( CBasePlayer* pPlayer );

    float m_flNextActiveTime;
    int m_nCustomIndex;
    string_t m_iszCustomName;
    bool m_fIsActive;
    int m_nLeft;
    int m_nTop;
    int m_nWidth;
    int m_nHeight;
};

void RefreshCustomHUD( CBasePlayer* pPlayer );
