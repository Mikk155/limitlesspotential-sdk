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

#include "GameMode.h"

class GM_Singleplayer : public GM_Base
{
    DECLARE_CLASS( GM_Singleplayer, GM_Base );
    GM_Singleplayer() = default;

public:

    static constexpr char GameModeName[] = "singleplayer";
    const char* GetName() const override { return GameModeName; }
    const char* GetBaseName() const override { return BaseClass::GetName(); }

    void OnRegister() override;
    void OnUnRegister() override;

    bool IsMultiplayer() override { return false; }
    void OnClientInit( CBasePlayer* player ) override;
    void OnThink() override;

private:
    // Used for the gamemode to restart in singleplayer if this is a multiplayer game in a listen server.
    bool m_IsMultiplayer;
    float m_MultiplayerRestart;

#ifndef CLIENT_DLL
    ScopedClientCommand m_VModEnableCommand;
    ScopedClientCommand m_MapSelectionTrigger;
#endif
};
