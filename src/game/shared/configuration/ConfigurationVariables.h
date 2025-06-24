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

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

#include <fmt/core.h>
#include <spdlog/logger.h>

#include "networking/NetworkDataSystem.h"
#include "utils/json_fwd.h"
#include "utils/GameSystem.h"

#define INVALID_NETWORK_INDEX -1

/**
 *    @brief Loads variables from files and provides a means of looking them up.
 */
class ConfigurationVariables final : public IGameSystem, public INetworkDataBlockHandler
{
    public:

        enum VarTypes : int
        {
            Float = 0,
            String,
            Integer,
            Boolean
        };

        // Variable flags
        struct VarFlags
        {
            // If numeric, this is the minimun possible value.
            std::optional<float> Min{std::nullopt};
            // If numeric, this is the maximum possible value.
            std::optional<float> Max{std::nullopt};

            // If true. this is sent to the clients
            bool network{false};
        };

        // Variable value variant
        using VarVariant = std::variant<std::string, float>;

        // Variable data
        struct Variable
        {
            // Variable name
            const std::string name;

            // Variable value explicit set in the code
            const VarVariant default_value;

            // Variable value
            std::optional<VarVariant> value;

            // Variable type, The game code is sensitive about this, do not set/get a variable type other than this.
            const VarTypes type;

            // Variable flags
            VarFlags flags{};

            Variable( const std::string& n, VarVariant d, VarTypes t = VarTypes::Float, VarFlags f = {} ) :
                name( std::move(n) ),
                default_value( std::move( d ) ),
                type( t ),
                flags( f )
            {}

            // Get value if has a value else default_value
            VarVariant Get() {
                if( value.has_value() )
                    return *value;
                return default_value;
            }

            // This is managed by the game itself. do not modify
            int network_index{INVALID_NETWORK_INDEX};
        };

        const char* GetName() const override { return "Configuration Variables"; }

        bool Initialize() override;
        void PostInitialize() override {}
        void Shutdown() override;

        void HandleNetworkDataBlock( NetworkDataBlock& block ) override {};

    private:

        std::shared_ptr<spdlog::logger> m_Logger;

        int m_LastNetworkedVariable{INVALID_NETWORK_INDEX};

        std::vector<Variable> m_vars;

        Variable* RegisterVariable( Variable var );

        void RegisterNetworkedVariables();

        // Set a value to a variant, this apply the flags and debug proper info.
        void SetVariant( Variable* var, VarVariant value );

#ifndef CLIENT_DLL // The client don't need these.
        void RegisterVariables();
#endif
};

inline ConfigurationVariables g_ConfigVars;
