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

#include "ServerConfigContext.h"

#include "config/GameConfig.h"

#include "utils/JSONSystem.h"

/**
 *    @brief Allows a configuration file to specify if entities should store their keyvalues for reading
 */
class KeyvalueManagerSection final : public GameConfigSection<ServerConfigContext>
{
public:
    explicit KeyvalueManagerSection() = default;

    std::string_view GetName() const override final { return "KeyvalueManager"; }

    json::value_t GetType() const override final { return json::value_t::boolean; }

    std::string GetSchema() const override final { return ""; }

    bool TryParse( GameConfigContext<ServerConfigContext>& context ) const override final
    {
        context.Data.State.KeyValueManager = context.Input.get<bool>();
        return true;
    }
};
