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

#include "player.h"

// -TODO Need saverestore
class PlayerList final
{
public:
    void add( CBasePlayer* player, CBaseEntity* target );
    void remove( CBasePlayer* player, CBaseEntity* target );
    bool toggle( CBasePlayer* player, CBaseEntity* target );
    bool exists( CBasePlayer* player, CBaseEntity* target );
    void clear( CBasePlayer* player );
};

inline PlayerList g_PlayerList;
