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

#ifdef CLIENT_DLL
#else
#include "player.h"
#include "ConCommandSystem.h"
#endif

class CBasePlayer;

class CAdminInterface final : public IGameSystem
{
    public:

        using StringPtr = std::shared_ptr<std::string>;

        using StringPoolList = std::vector<StringPtr>;

        using StringPoolMap = std::unordered_map<std::string_view, StringPtr>;

        using AdminRoleMap = std::unordered_map<std::string, StringPoolList>;

        const char* GetName() const override { return "Admin Interface"; }

        bool Initialize() override;
        void PostInitialize() override {}
        void Shutdown() override;

        /**
         * @brief Returns whatever this @c player can use the given @c command
         */ 
        bool HasAccess( CBasePlayer* player, std::string_view command );

        void RegisterCommands();

    private:

        // String pool containing all available commands
        StringPoolMap m_CommandsPool;

        AdminRoleMap m_ParsedAdmins;

        std::shared_ptr<spdlog::logger> m_Logger;

#ifdef CLIENT_DLL
    public:
//        void OpenCommandMenu();
#else
    public:
        void OnMapInit();

    private:
        ScopedClientCommand m_ScopedAdminMenu;
#endif
};

inline CAdminInterface g_AdminInterface;
