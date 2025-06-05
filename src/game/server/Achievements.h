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

#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "utils/json_fwd.h"

#include "utils/GameSystem.h"

#include "player.h"

class CAchievements final : public IGameSystem
{
    public:

        const char* GetName() const override { return "Achievements"; }

        bool Initialize() override;
        void PostInitialize() override {}
        void Shutdown() override {};

        void PreMapActivate();
        void Save();

        void Achieve( CBasePlayer* player, const std::string& name, const std::string& description );
        void Achieve( const std::string& name, const std::string& description );

    private:

        bool m_bShouldSave;
        float m_flNextThink;
        json m_achievements;
};

inline CAchievements g_Achievement;
