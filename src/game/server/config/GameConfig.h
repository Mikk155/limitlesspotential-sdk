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

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include <spdlog/logger.h>

#include "ConditionEvaluator.h"
#include "GameConfigIncludeStack.h"

#include "utils/GameSystem.h"
#include "utils/JSONSystem.h"
#include "utils/string_utils.h"

template <typename DataContext>
class GameConfigDefinition;

template <typename DataContext>
struct GameConfigContext final
{
    const std::string_view ConfigFileName;
    const json& Input;
    const GameConfigDefinition<DataContext>& Definition;

    spdlog::logger& Logger;
    DataContext& Data;
};

/**
 *    @brief Defines a section of a game configuration, and the logic to parse it.
 *    @tparam DataContext The type of the data context this section operates on.
 */
template <typename DataContext>
class GameConfigSection
{
protected:
    GameConfigSection() = default;

public:
    GameConfigSection( const GameConfigSection& ) = delete;
    GameConfigSection& operator=( const GameConfigSection& ) = delete;
    virtual ~GameConfigSection() = default;

    /**
     *    @brief Gets the name of this section
     */
    virtual std::string_view GetName() const = 0;

    /**
     *    @brief Gets the section type (e.g. @c object, @c array, etc).
     */
    virtual json::value_t GetType() const = 0;

    /**
     *    @brief Gets the schema for this section.
     *    @details @c title and @c type are provided by the game config system.
     *    For objects the schema should contain at minimum the object's @c properties.
     *    It can also contain @c required and other schema properties not provided automatically.
     *    This can be an empty string if no additional information is needed (e.g. a number with no validation required).
     */
    virtual std::string GetSchema() const = 0;

    /**
     *    @brief Tries to parse the given configuration.
     */
    virtual bool TryParse( GameConfigContext<DataContext>& context ) const = 0;
};

struct GameConfigFileData
{
    std::string FileName;
    json Data;
};

/**
 *    @brief Contains the contents of a loaded game configuration, ready to be parsed.
 */
template <typename DataContext>
class GameConfig final
{
public:
    explicit GameConfig( const GameConfigDefinition<DataContext>* definition,
        std::shared_ptr<spdlog::logger> logger,
        std::vector<GameConfigFileData>&& contents )
        : m_Definition( definition ),
          m_Logger( logger ),
          m_Contents( std::move( contents ) )
    {
    }

    void Parse( DataContext& dataContext ) const;

private:
    void ParseSectionGroups( DataContext& dataContext, const GameConfigFileData& input ) const;

    void ParseSections( DataContext& dataContext, std::string_view configFileName, const json& sectionGroup ) const;

    bool ShouldProcessSectionGroup( const json& input ) const;

private:
    const GameConfigDefinition<DataContext>* m_Definition;
    std::shared_ptr<spdlog::logger> m_Logger;
    // List of loaded file contents, in parse order.
    std::vector<GameConfigFileData> m_Contents;
};

/**
 *    @brief Immutable definition of a game configuration.
 *    Contains a set of sections that configuration files can use.
 *    Instances should be created with GameConfigLoader::CreateDefinition.
 *    @details Each section specifies what kind of data it supports, and provides a means of parsing it.
 *    @tparam DataContext Type of the data context this definition operates on.
 */
template <typename DataContext>
class GameConfigDefinition
{
public:
    GameConfigDefinition( 
        std::shared_ptr<spdlog::logger> logger,
        std::string&& name,
        bool hasGameModeConfig,
        std::vector<std::unique_ptr<const GameConfigSection<DataContext>>>&& sections )
        : m_Logger( logger ),
          m_Name( std::move( name ) ),
          m_HasGameModeConfig( hasGameModeConfig ),
          m_Sections( std::move( sections ) )
    {
    }

    GameConfigDefinition( const GameConfigDefinition& ) = delete;
    GameConfigDefinition& operator=( const GameConfigDefinition& ) = delete;

    ~GameConfigDefinition() = default;

    std::string_view GetName() const { return m_Name; }

    const GameConfigSection<DataContext>* FindSection( std::string_view name ) const;

    /**
     *    @brief Gets the complete JSON Schema for this definition.
     *    This includes all of the sections.
     */
    std::string GetSchema() const;

    std::optional<GameConfig<DataContext>> TryLoad( const char* fileName, const char* pathID = nullptr ) const;

private:
    struct LoadContext
    {
        const char* PathID{};
        GameConfigIncludeStack& IncludeStack;
        int Depth = 1;
        std::vector<GameConfigFileData> Contents;
    };

    bool TryLoadCore( LoadContext& loadContext, const char* fileName ) const;

    void ParseIncludedFiles( LoadContext& loadContext, const json& input ) const;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    std::string m_Name;
    bool m_HasGameModeConfig;
    std::vector<std::unique_ptr<const GameConfigSection<DataContext>>> m_Sections;
};

/**
 *    @brief Loads JSON-based configuration files and processes them according to a provided definition.
 */
class GameConfigSystem final : public IGameSystem
{
public:
    GameConfigSystem() = default;
    ~GameConfigSystem() = default;

    const char* GetName() const override { return "GameConfig"; }

    bool Initialize() override;
    void PostInitialize() override {}
    void Shutdown() override;

    std::shared_ptr<spdlog::logger> GetLogger() { return m_Logger; }

    template <typename DataContext>
    std::shared_ptr<const GameConfigDefinition<DataContext>> CreateDefinition( 
        std::string&& name, bool hasGameModeConfig,
        std::vector<std::unique_ptr<const GameConfigSection<DataContext>>>&& sections );

private:
    std::shared_ptr<spdlog::logger> m_Logger;
};

inline GameConfigSystem g_GameConfigSystem;

template <typename DataContext>
void GameConfig<DataContext>::Parse( DataContext& dataContext ) const
{
    for( const auto& data : m_Contents )
    {
        try
        {
            ParseSectionGroups( dataContext, data );
        }
        catch ( const std::exception& e )
        {
            m_Logger->error( "Error parsing map configuration: {}", e.what() );
        }
    }
}

template <typename DataContext>
void GameConfig<DataContext>::ParseSectionGroups( DataContext& dataContext, const GameConfigFileData& input ) const
{
    const auto sectionGroups = input.Data.find( "SectionGroups" );

    if( sectionGroups == input.Data.end() || !sectionGroups->is_array() )
    {
        return;
    }

    for( std::size_t index = 0; const auto& sectionGroup : *sectionGroups )
    {
        m_Logger->trace( "Entering section group {}", index );

        if( ShouldProcessSectionGroup( sectionGroup ) )
        {
            ParseSections( dataContext, input.FileName, sectionGroup );
        }

        m_Logger->trace( "Exiting section group {}", index );

        ++index;
    }
}

template <typename DataContext>
void GameConfig<DataContext>::ParseSections( DataContext& dataContext, std::string_view configFileName, const json& sectionGroup ) const
{
    const auto sections = sectionGroup.find( "Sections" );

    if( sections == sectionGroup.end() || !sections->is_object() )
    {
        return;
    }

    m_Logger->trace( "{} sections in group", sections->size() );

    for( const auto& [name, section] : sections->items() )
    {
        m_Logger->debug( "Processing section \"{}\" ({})", name, section.type_name() );

        if( const auto sectionObj = m_Definition->FindSection( name ); sectionObj )
        {
            if( sectionObj->GetType() != section.type() )
            {
                continue;
            }

            GameConfigContext<DataContext> context{
                .ConfigFileName = configFileName,
                .Input = section,
                .Definition = *m_Definition,
                .Logger = *m_Logger,
                .Data = dataContext};

            if( !sectionObj->TryParse( context ) )
            {
                m_Logger->error( "Error parsing section \"{}\"", sectionObj->GetName() );
            }
        }
        else
        {
            m_Logger->warn( "Unknown section \"{}\"", name );
        }
    }
}

template <typename DataContext>
bool GameConfig<DataContext>::ShouldProcessSectionGroup( const json& input ) const
{
    if( const auto condition = Trim( input.value( "Condition"sv, ""sv ) ); !condition.empty() )
    {
        m_Logger->trace( "Testing section group condition \"{}\"", condition );

        const auto shouldUseSection = g_ConditionEvaluator.Evaluate( condition );

        if( !shouldUseSection.has_value() )
        {
            // Error already reported
            m_Logger->trace( "Skipping section group because an error occurred while evaluating condition" );
            return false;
        }

        if( !shouldUseSection.value() )
        {
            m_Logger->trace( "Skipping section group because condition evaluated false" );
            return false;
        }
        else
        {
            m_Logger->trace( "Using section group because condition evaluated true" );
        }
    }

    return true;
}

template <typename DataContext>
inline const GameConfigSection<DataContext>* GameConfigDefinition<DataContext>::FindSection( std::string_view name ) const
{
    if( auto it = std::find_if( m_Sections.begin(), m_Sections.end(), [&]( const auto& section )
            { return section->GetName() == name; } );
        it != m_Sections.end() )
    {
        return it->get();
    }

    return nullptr;
}

template <typename DataContext>
inline std::string GameConfigDefinition<DataContext>::GetSchema() const
{
    // A configuration is structured like this:
    /*
    {
        "include": [
            "some_filename.json"
        ],
        "SectionGroups": [
            {
                "Condition": "<some condition>", // Optional.
                "Sections": {
                    "SectionName": <Section data here> // Value can be any type chosen by section.
                }
            }
        ]
    }
    */

    // This structure allows for future expansion without making breaking changes

    const auto sections = [this]()
    {
        std::string buffer;

        bool first = true;

        for( const auto& section : m_Sections )
        {
            if( first )
            {
                first = false;
            }
            else
            {
                buffer += ',';
            }

            const auto schema = section->GetSchema();

            buffer += fmt::format( R"(
"{0}": {{
    "title": "{0} Section",
    "type": "{1}"{2}
    {3}
}})",
                section->GetName(), json( section->GetType() ).type_name(), schema.empty() ? ""sv : ","sv, schema );
        }

        return buffer;
    }();

    const std::string_view gameModeConfig = R"("GameMode": {
            "title": "Game Mode",
            "description": "Game mode to use for this map",
            "type": "string"
        },
        "LockGameMode": {
            "title": "Lock Game Mode to configured setting",
            "description": "Prevents the server from changing the game mode setting for this map",
            "type": "boolean"
        },)"sv;

    return fmt::format( R"(
{{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "{} Game Configuration",
    "type": "object",
    "properties": {{
        "include": {{
            "title": "Included Files",
            "description": "List of JSON files to include before this file.",
            "type": "array",
            "items": {{
                "title": "Include",
                "type": "string",
                "pattern": "^[\\w/]+.json$"
            }}
        }},
        {}
        "SectionGroups": {{
            "title": "Section groups",
            "type": "array",
            "items": {{
                "title": "Section group",
                "description": "A group of sections with an optional condition",
                "type": "object",
                "properties": {{
                    "Condition": {{
                        "description": "If specified, this section group will only be used if the condition evaluates true",
                        "type": "string"
                    }},
                    "Sections": {{
                        "title": "Sections",
                        "type": "object",
                        "properties": {{
                            {}
                        }},
                        "additionalProperties": false
                    }}
                }}
            }}
        }}
    }}
}}
)",
        this->GetName(), m_HasGameModeConfig ? gameModeConfig : "", sections );
}

template <typename DataContext>
std::optional<GameConfig<DataContext>> GameConfigDefinition<DataContext>::TryLoad( 
    const char* fileName, const char* pathID ) const
{
    GameConfigIncludeStack includeStack;

    // So you can't get a stack overflow including yourself over and over.
    includeStack.Add( fileName );

    LoadContext loadContext{
        .PathID = pathID,
        .IncludeStack = includeStack};

    if( !TryLoadCore( loadContext, fileName ) )
    {
        return {};
    }

    return GameConfig<DataContext>{this, m_Logger, std::move( loadContext.Contents )};
}

template <typename DataContext>
bool GameConfigDefinition<DataContext>::TryLoadCore( LoadContext& loadContext, const char* fileName ) const
{
    m_Logger->debug( "Loading \"{}\" configuration file \"{}\" (depth {})", GetName(), fileName, loadContext.Depth );

    auto result = g_JSON.ParseJSONFile( 
        fileName,
        {.SchemaName = GetName(), .PathID = loadContext.PathID},
        [&, this]( json& input )
        {
            ParseIncludedFiles( loadContext, input );
            loadContext.Contents.emplace_back( fileName, std::move( input ) );
            return true; // Just return something so optional doesn't complain.
        } );

    if( result.has_value() )
    {
        m_Logger->trace( "Successfully loaded configuration file" );
    }
    else
    {
        m_Logger->trace( "Failed to load configuration file" );
    }

    return result.has_value();
}

template <typename DataContext>
void GameConfigDefinition<DataContext>::ParseIncludedFiles( LoadContext& loadContext, const json& input ) const
{
    if( auto includes = input.find( "include" ); includes != input.end() )
    {
        if( !includes->is_array() )
        {
            return;
        }

        for( const auto& include : *includes )
        {
            if( !include.is_string() )
            {
                continue;
            }

            auto includeFileName = include.get<std::string>();

            includeFileName = Trim( includeFileName );

            switch ( loadContext.IncludeStack.Add( includeFileName.c_str() ) )
            {
                case IncludeAddResult::Success:
                {
                    m_Logger->trace( "Including file \"{}\"", includeFileName );

                    ++loadContext.Depth;

                    TryLoadCore( loadContext, includeFileName.c_str() );

                    --loadContext.Depth;
                    break;
                }

                case IncludeAddResult::AlreadyIncluded:
                {
                    m_Logger->debug( "Included file \"{}\" already included before", includeFileName );
                    break;
                }


                case IncludeAddResult::CouldNotResolvePath:
                {
                    m_Logger->error( "Couldn't resolve path for included configuration file \"{}\"", includeFileName );
                    break;
                }
            }
        }
    }
}

template <typename DataContext>
inline std::shared_ptr<const GameConfigDefinition<DataContext>> GameConfigSystem::CreateDefinition( 
    std::string&& name, bool hasGameModeConfig,
    std::vector<std::unique_ptr<const GameConfigSection<DataContext>>>&& sections )
{
    auto definition = std::make_shared<const GameConfigDefinition<DataContext>>( 
        m_Logger, std::move( name ), hasGameModeConfig, std::move( sections ) );

    g_JSON.RegisterSchema( std::string{definition->GetName()}, [=]()
        { return definition->GetSchema(); } );

    return definition;
}
