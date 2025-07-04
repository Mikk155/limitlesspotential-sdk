function(add_game_shared_sources target)
    target_sources(${target}
        PRIVATE
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GameLibrary.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GameLibrary.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/palette.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ProjectInfoSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ProjectInfoSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AdminInterface.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AdminInterface.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Achievements.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Achievements.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ConfigurationSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ConfigurationSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/voice_common.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/configuration/ConfigurationVariables.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/configuration/ConfigurationVariables.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/ehandle.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/ehandle.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/EntityClassificationSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/EntityClassificationSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/EntityDictionary.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/entity_shared.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/entity_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/entity_utils.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/player_shared.cpp

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/CBaseItem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/CBaseItem.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/AmmoTypeSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/AmmoTypeSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CCrossbow.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CCrossbow.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CCrowbar.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CCrowbar.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CDisplacer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CDisplacer.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CEagle.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CEagle.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CEgon.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CEgon.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGauss.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGauss.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGlock.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGlock.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGrapple.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CGrapple.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CHandGrenade.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CHandGrenade.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CHgun.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CHgun.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CKnife.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CKnife.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CM249.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CM249.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CMP5.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CMP5.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPenguin.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPenguin.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPipewrench.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPipewrench.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPython.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CPython.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CRpg.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CRpg.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSatchel.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSatchel.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CShockRifle.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CShockRifle.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CShotgun.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CShotgun.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSniperRifle.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSniperRifle.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSporeLauncher.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSporeLauncher.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSqueak.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CSqueak.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CTripmine.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/CTripmine.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/weapons_shared.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/weapons.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/WeaponDataSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/entities/items/weapons/WeaponDataSystem.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GameMode.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GameMode.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Multiplayer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Multiplayer.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Singleplayer.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Singleplayer.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_CaptureTheFlag.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_CaptureTheFlag.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Cooperative.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Cooperative.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Deathmatch.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_Deathmatch.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_TeamDeathmatch.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gamemodes/GM_TeamDeathmatch.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/models/BspLoader.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/models/BspLoader.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/networking/NetworkDataSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/networking/NetworkDataSystem.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_debug.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_debug.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_defs.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_info.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_materials.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_movevars.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_shared.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/player_movement/pm_shared.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/DataFieldSerializers.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/DataFieldSerializers.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/DataMap.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/DataMap.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/saverestore.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/saverestore/saverestore.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripting/AS/as_addons.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripting/AS/as_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripting/AS/as_utils.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripting/AS/ASManager.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripting/AS/ASManager.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sound/MaterialSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sound/MaterialSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sound/sentence_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sound/sentence_utils.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ui/hud/HudReplacementSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ui/hud/HudReplacementSystem.h

            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/ConCommandSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/ConCommandSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/filesystem_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/filesystem_utils.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/GameSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/GameSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/heterogeneous_lookup.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/json_fwd.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/JSONSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/JSONSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/LogSystem.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/LogSystem.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/PrecacheList.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/PrecacheList.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/ReplacementMaps.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/ReplacementMaps.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/shared_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/shared_utils.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/SteamID.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/string_utils.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/string_utils.h
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/StringPool.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utils/StringPool.h)
endfunction()
