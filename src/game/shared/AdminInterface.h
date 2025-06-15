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
#include "networking/NetworkDataSystem.h"

#ifdef CLIENT_DLL
class CBasePlayer;
#else
#include "player.h"
#include "ConCommandSystem.h"
#endif

class CAdminInterface final : public IGameSystem, public INetworkDataBlockHandler
{
    public:

        using StringPtr = std::shared_ptr<std::string>;

        using StringPoolList = std::vector<StringPtr>;

        using StringPoolMap = std::unordered_map<std::string_view, StringPtr>;

        using AdminRoleMap = std::unordered_map<std::string_view, StringPoolList>;

        const char* GetName() const override { return "Admin Interface"; }

        bool Initialize() override;
        void PostInitialize() override {}
        void Shutdown() override;
        void HandleNetworkDataBlock( NetworkDataBlock& block ) override;

        /**
         * @brief Returns whatever this @c player can use the given @c command
         * 
         * @details this always return true for the host player if singleplayer or listen server
         */ 
        bool HasAccess( CBasePlayer* player, std::string_view command );

        StringPtr AdminName( StringPtr SteamID );

    private:

        StringPoolList m_StringPool;
        StringPtr ToStringPool( const std::string& str );

        StringPoolMap m_AdminNames;
        AdminRoleMap m_ParsedAdmins;

        bool m_Active;

        json ParsePermissions( json& input );

        std::shared_ptr<spdlog::logger> m_Logger;

#ifdef CLIENT_DLL
#else
    public:
        void RegisterCommands();

        // Utilities
        // For multiple iterations run ParseJson yourself first, if is a one-time task use the ParseKeyvalues definition with JsonString
        bool ParseKeyvalues( CBasePlayer* player, CBaseEntity* entity, std::optional<json> KeyValuesOpt = std::nullopt );
        bool ParseKeyvalues( CBasePlayer* player, CBaseEntity* entity, const char* JsonString ) {
            return ParseKeyvalues( player, entity, ParseJson(player, JsonString) );
        }
        // Replaces ' -> " and then parses a json object
        std::optional<json> ParseJson( CBasePlayer* player, std::string text );

    private:

        bool m_binitialized{false};
#endif
};

inline CAdminInterface g_AdminInterface;
