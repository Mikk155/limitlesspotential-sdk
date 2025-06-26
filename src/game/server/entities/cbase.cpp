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

#include <unordered_map>

#include "cbase.h"
#include "ServerLibrary.h"
#include "MapState.h"
#include "pm_shared.h"
#include "world.h"
#include "sound/ServerSoundSystem.h"
#include "utils/ReplacementMaps.h"

static void SetObjectCollisionBox( entvars_t* pev );

int DispatchSpawn( edict_t* pent )
{
    // Initialize these or entities who don't link to the world won't have anything in here
    pent->v.absmin = pent->v.origin - Vector( 1, 1, 1 );
    pent->v.absmax = pent->v.origin + Vector( 1, 1, 1 );

    CBaseEntity* entity = (CBaseEntity*)GET_PRIVATE( pent );

    if( entity == nullptr )
        return -1;

    // Removing these will cause all sorts of problems.
    if( entity == CBaseEntity::World || entity->IsPlayer() )
    {
        g_GameMode->OnEntityPreSpawn( entity );
        entity->Spawn();
        g_GameMode->OnEntityPostSpawn( entity );
        return 0;
    }

    // We map these keyvalues regardless of the map config
    entity->m_CustomKeyValues.custom = true;

    auto EntityHasSpawned = [&]( SpawnAction code ) -> int
    {
        entity = (CBaseEntity*)GET_PRIVATE( pent );

        if( entity == nullptr )
        {
            if( !FNullEnt(pent) )
                CBaseEntity::Logger->error( "Unable to handle Spawn for entity {}", STRING( pent->v.classname ) );
            return -1;
        }

        switch( code )
        {
            case SpawnAction::DelayRemove:
            {
                entity->UpdateOnRemove();
                entity->pev->flags |= FL_KILLME;
                entity->pev->targetname = string_t::Null;
                return -1;
            }
            case SpawnAction::RemoveNow:
            {
                g_engfuncs.pfnRemoveEntity( pent );
                return -1;
            }
            case SpawnAction::HandledRemoval:
            {
                return -1;
            }
            case SpawnAction::HandledSpawn:
            {
                return 0;
            }
            case SpawnAction::Spawn:
            default:
            {
                return 1;
            }
        }
    };

    // Whatever code != SpawnAction::Spawn is either -1 for removal or 0 for Spawn handled
    if( int code = EntityHasSpawned( g_GameMode->OnEntityPreSpawn( entity ) ); code != 1 )
        return code;
    if( int code = EntityHasSpawned( entity->m_appearflags ); code != 1 )
        return code;
    if( int code = EntityHasSpawned( entity->Spawn() ); code != 1 )
        return code;
    if( int code = EntityHasSpawned( entity->GlobalState() ); code != 1 )
        return code;
    if( int code = EntityHasSpawned( g_GameMode->OnEntityPostSpawn( entity ) ); code != 1 )
        return code;

    return 0;
}

void EntvarsKeyvalue( entvars_t* pev, KeyValueData* pkvd )
{
    for( const auto& member : entvars_t::GetLocalDataMap()->Members )
    {
        auto field = std::get_if<DataFieldDescription>( &member );

        if( field && !stricmp( field->fieldName, pkvd->szKeyName ) )
        {
            switch ( field->fieldType )
            {
            case FIELD_MODELNAME:
            case FIELD_SOUNDNAME:
            case FIELD_STRING:
                ( *( string_t* )( (char*)pev + field->fieldOffset ) ) = ALLOC_STRING( pkvd->szValue );
                break;

            case FIELD_TIME:
            case FIELD_FLOAT:
                ( *(float*)( (char*)pev + field->fieldOffset ) ) = atof( pkvd->szValue );
                break;

            case FIELD_INTEGER:
                ( *(int*)( (char*)pev + field->fieldOffset ) ) = atoi( pkvd->szValue );
                break;

            case FIELD_POSITION_VECTOR:
            case FIELD_VECTOR:
                UTIL_StringToVector( *( ( Vector* )( (char*)pev + field->fieldOffset ) ), pkvd->szValue );
                break;

            default:
            case FIELD_CLASSPTR:
            case FIELD_EDICT:
                CBaseEntity::Logger->error( "Bad field in entity!!" );
                break;
            }
            pkvd->fHandled = 1;
            return;
        }
    }
}

void DispatchKeyValue( edict_t* pentKeyvalue, KeyValueData* pkvd )
{
    if( !pkvd || !pentKeyvalue )
        return;

    if( g_Server.CheckForNewMapStart( false ) )
    {
        // HACK: If we get here that means we're loading a new map and we're setting worldspawn's classname.
        // We've wiped out com_token by calling SERVER_EXECUTE() which stores the classname so we need to reset it.
        pkvd->szValue = "worldspawn";
    }

    // Don't allow classname changes once the classname has been set.
    if( !FStringNull( pentKeyvalue->v.classname ) && FStrEq( pkvd->szKeyName, "classname" ) )
    {
        CBaseEntity::Logger->debug( "{}: Duplicate classname \"{}\" ignored",
            STRING( pentKeyvalue->v.classname ), pkvd->szValue );
        return;
    }

    EntvarsKeyvalue( &pentKeyvalue->v, pkvd );

    // If the key was an entity variable, or there's no class set yet, don't look for the object, it may
    // not exist yet.
    if( 0 != pkvd->fHandled || pkvd->szClassName == nullptr )
        return;

    // Get the actualy entity object
    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pentKeyvalue );

    if( !pEntity )
        return;

    if( g_Server.GetMapState()->KeyValueManager )
    {
        pEntity->m_KeyValues.SetValue( pkvd->szKeyName, pkvd->szValue );
    }

    pkvd->fHandled = static_cast<int32>( pEntity->RequiredKeyValue( pkvd ) );

    if( pkvd->fHandled != 0 )
    {
        return;
    }

    pkvd->fHandled = static_cast<int32>( pEntity->KeyValue( pkvd ) );
}

void DispatchTouch( edict_t* pentTouched, edict_t* pentOther )
{
    if( gTouchDisabled )
        return;

    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pentTouched );
    CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE( pentOther );

    if( pEntity && pOther && ( ( pEntity->pev->flags | pOther->pev->flags ) & FL_KILLME ) == 0 && !FBitSet( pEntity->m_UseLocked, USE_VALUE_TOUCH ) )
        pEntity->Touch( pOther );
}

void DispatchUse( edict_t* pentUsed, edict_t* pentOther )
{
    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pentUsed );
    CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE( pentOther );

    if( pEntity && ( pEntity->pev->flags & FL_KILLME ) == 0 && !FBitSet( pEntity->m_UseLocked, USE_VALUE_USE ) )
        pEntity->Use( pOther, pOther, USE_TOGGLE );
}

void DispatchThink( edict_t* pent )
{
    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pent );
    if( pEntity )
    {
        if( FBitSet( pEntity->pev->flags, FL_DORMANT ) )
            CBaseEntity::Logger->error( "Dormant entity {} is thinking!!", STRING( pEntity->pev->classname ) );

        pEntity->Think();
    }
}

void DispatchBlocked( edict_t* pentBlocked, edict_t* pentOther )
{
    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pentBlocked );
    CBaseEntity* pOther = (CBaseEntity*)GET_PRIVATE( pentOther );

    if( pEntity )
        pEntity->Blocked( pOther );
}

void DispatchSave( edict_t* pent, SAVERESTOREDATA* pSaveData )
{
    gpGlobals->time = pSaveData->time;

    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pent );

    if( pEntity && CSaveRestoreBuffer::IsValidSaveRestoreData( pSaveData ) )
    {
        ENTITYTABLE* pTable = &pSaveData->pTable[pSaveData->currentIndex];

        if( pTable->pent != pent )
            CBaseEntity::Logger->error( "ENTITY TABLE OR INDEX IS WRONG!!!!" );

        if( ( pEntity->ObjectCaps() & FCAP_DONT_SAVE ) != 0 )
            return;

        // These don't use ltime & nextthink as times really, but we'll fudge around it.
        if( pEntity->pev->movetype == MOVETYPE_PUSH )
        {
            float delta = pEntity->pev->nextthink - pEntity->pev->ltime;
            pEntity->pev->ltime = gpGlobals->time;
            pEntity->pev->nextthink = pEntity->pev->ltime + delta;
        }

        // Move the map into arrays of string_t
        pEntity->m_KeyValues.Save();
        pEntity->m_CustomKeyValues.Save();

        pTable->location = pSaveData->size;             // Remember entity position for file I/O
        pTable->classname = pEntity->pev->classname; // Remember entity class for respawn

        CSave saveHelper( *pSaveData );
        pEntity->Save( saveHelper );

        pTable->size = pSaveData->size - pTable->location; // Size of entity block is data size written to block
    }
}

void OnFreeEntPrivateData( edict_t* pEdict )
{
    if( pEdict && pEdict->pvPrivateData )
    {
        auto entity = reinterpret_cast<CBaseEntity*>( pEdict->pvPrivateData );

        g_EntityDictionary->Destroy( entity );

        // Zero this out so the engine doesn't try to free it again.
        pEdict->pvPrivateData = nullptr;
    }
}

/**
 *    @brief Find the matching global entity.
 *    Spit out an error if the designer made entities of different classes with the same global name
 */
CBaseEntity* FindGlobalEntity( string_t classname, string_t globalname )
{
    auto pReturn = UTIL_FindEntityByString( nullptr, "globalname", STRING( globalname ) );

    if( pReturn )
    {
        if( !pReturn->ClassnameIs( STRING( classname ) ) )
        {
            CBaseEntity::Logger->debug( "Global entity found {}, wrong class {}", STRING( globalname ), STRING( pReturn->pev->classname ) );
            pReturn = nullptr;
        }
    }

    return pReturn;
}

int DispatchRestore( edict_t* pent, SAVERESTOREDATA* pSaveData, int globalEntity )
{
    gpGlobals->time = pSaveData->time;

    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pent );

    if( pEntity && CSaveRestoreBuffer::IsValidSaveRestoreData( pSaveData ) )
    {
        entvars_t tmpVars;
        Vector oldOffset;

        CRestore restoreHelper( *pSaveData );
        if( 0 != globalEntity )
        {
            CRestore tmpRestore( *pSaveData );
            tmpRestore.PrecacheMode( false );
            tmpRestore.ReadFields( &tmpVars, *entvars_t::GetLocalDataMap(), *entvars_t::GetLocalDataMap() );

            // HACKHACK - reset the save pointers, we're going to restore for real this time
            pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
            pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
            // -------------------


            const globalentity_t* pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );

            // Don't overlay any instance of the global that isn't the latest
            // pSaveData->szCurrentMapName is the level this entity is coming from
            // pGlobla->levelName is the last level the global entity was active in.
            // If they aren't the same, then this global update is out of date.
            if( !FStrEq( pSaveData->szCurrentMapName, pGlobal->levelName ) )
                return 0;

            // Compute the new global offset
            oldOffset = pSaveData->vecLandmarkOffset;
            CBaseEntity* pNewEntity = FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
            if( pNewEntity )
            {
                // CBaseEntity::Logger->debug("Overlay {} with {}", STRING(pNewEntity->pev->classname), STRING(tmpVars.classname));
                //  Tell the restore code we're overlaying a global entity from another level
                restoreHelper.SetGlobalMode( true ); // Don't overwrite global fields
                pSaveData->vecLandmarkOffset = ( pSaveData->vecLandmarkOffset - pNewEntity->pev->mins ) + tmpVars.mins;
                pEntity = pNewEntity; // we're going to restore this data OVER the old entity
                pent = pEntity->edict();
                // Update the global table to say that the global definition of this entity should come from this level
                gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobals->mapname );
            }
            else
            {
                // This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
                // or call EntityUpdate() to move it to this level, we haven't changed global state at all.
                return 0;
            }
        }

        pEntity->Restore( restoreHelper );
        pEntity->PostRestore();

        // Move arrays of string_t into the map
        pEntity->m_KeyValues.Restore();
        pEntity->m_CustomKeyValues.Restore();

        if( ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN ) != 0 )
        {
            if( !pEntity->Spawn() )
            {
                REMOVE_ENTITY( pent );
                return -1;
            }
        }

        pEntity->Precache();

#if 0
        if( pEntity && pEntity->pev->globalname && globalEntity )
        {
            CBaseEntity::Logger->debug( "Global {} is {}", STRING( pEntity->pev->globalname ), STRING( pEntity->pev->model ) );
        }
#endif

        // Is this an overriding global entity (coming over the transition), or one restoring in a level
        if( 0 != globalEntity )
        {
            // CBaseEntity::Logger->debug("After: {} {}", pEntity->pev->origin, STRING(pEntity->pev->model));
            pSaveData->vecLandmarkOffset = oldOffset;
            if( pEntity )
            {
                pEntity->SetOrigin( pEntity->pev->origin );
                pEntity->OverrideReset();
            }
        }
        else if( pEntity && !FStringNull( pEntity->pev->globalname ) )
        {
            const globalentity_t* pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
            if( pGlobal )
            {
                // Already dead? delete
                if( pGlobal->state == GLOBAL_DEAD )
                    return -1;
                else if( !FStrEq( STRING( gpGlobals->mapname ), pGlobal->levelName ) )
                {
                    pEntity->MakeDormant(); // Hasn't been moved to this level yet, wait but stay alive
                }
                // In this level & not dead, continue on as normal
            }
            else
            {
                CBaseEntity::Logger->error( "Global Entity {} ({}) not in table!!!\n",
                    STRING( pEntity->pev->globalname ), STRING( pEntity->pev->classname ) );
                // Spawned entities default to 'On'
                gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
            }
        }
    }
    return 0;
}

void DispatchObjectCollsionBox( edict_t* pent )
{
    CBaseEntity* pEntity = (CBaseEntity*)GET_PRIVATE( pent );
    if( pEntity )
    {
        pEntity->SetObjectCollisionBox();
    }
    else
        SetObjectCollisionBox( &pent->v );
}

// The engine uses the old type description data, so we need to translate it to the game's version.
static FIELDTYPE RemapEngineFieldType( ENGINEFIELDTYPE fieldType )
{
    // The engine only uses these data types.
    // Some custom engine may use others, but those engines are not supported.
    switch ( fieldType )
    {
    case ENGINEFIELDTYPE::FIELD_FLOAT: return FIELD_FLOAT;
    case ENGINEFIELDTYPE::FIELD_STRING: return FIELD_STRING;
    case ENGINEFIELDTYPE::FIELD_EDICT: return FIELD_EDICT;
    case ENGINEFIELDTYPE::FIELD_VECTOR: return FIELD_VECTOR;
    case ENGINEFIELDTYPE::FIELD_INTEGER: return FIELD_INTEGER;
    case ENGINEFIELDTYPE::FIELD_CHARACTER: return FIELD_CHARACTER;
    case ENGINEFIELDTYPE::FIELD_TIME: return FIELD_TIME;

    default: return FIELD_TYPECOUNT;
    }
}

static const IDataFieldSerializer* RemapEngineFieldTypeToSerializer( ENGINEFIELDTYPE fieldType )
{
    // The engine only uses these data types.
    // Some custom engine may use others, but those engines are not supported.
    switch ( fieldType )
    {
    case ENGINEFIELDTYPE::FIELD_FLOAT: return &FieldTypeToSerializerMapper<FIELD_FLOAT>::Serializer;
    case ENGINEFIELDTYPE::FIELD_STRING: return &FieldTypeToSerializerMapper<FIELD_STRING>::Serializer;
    case ENGINEFIELDTYPE::FIELD_EDICT: return &FieldTypeToSerializerMapper<FIELD_EDICT>::Serializer;
    case ENGINEFIELDTYPE::FIELD_VECTOR: return &FieldTypeToSerializerMapper<FIELD_VECTOR>::Serializer;
    case ENGINEFIELDTYPE::FIELD_INTEGER: return &FieldTypeToSerializerMapper<FIELD_INTEGER>::Serializer;
    case ENGINEFIELDTYPE::FIELD_CHARACTER: return &FieldTypeToSerializerMapper<FIELD_CHARACTER>::Serializer;
    case ENGINEFIELDTYPE::FIELD_TIME: return &FieldTypeToSerializerMapper<FIELD_TIME>::Serializer;

    default: return nullptr;
    }
}

struct EngineDataMap
{
    std::unique_ptr<const DataMember[]> TypeDescriptions;
    DataMap Map;
};

std::unordered_map<const TYPEDESCRIPTION*, std::unique_ptr<const EngineDataMap>> g_EngineTypeDescriptionsToGame;

static const DataMap* GetOrCreateDataMap( const char* className, const TYPEDESCRIPTION* fields, int fieldCount )
{
    auto it = g_EngineTypeDescriptionsToGame.find( fields );

    if( it == g_EngineTypeDescriptionsToGame.end() )
    {
        auto typeDescriptions = std::make_unique<DataMember[]>( fieldCount );

        for( int i = 0; i < fieldCount; ++i )
        {
            const auto& src = fields[i];

            DataFieldDescription dest{
                .fieldType = RemapEngineFieldType( src.fieldType ),
                .Serializer = RemapEngineFieldTypeToSerializer( src.fieldType ),
                .fieldName = src.fieldName,
                .fieldOffset = src.fieldOffset,
                .fieldSize = src.fieldSize,
                .flags = src.flags};

            typeDescriptions[i] = dest;
        }

        auto engineDataMap = std::make_unique<EngineDataMap>();

        engineDataMap->Map.ClassName = className;
        engineDataMap->Map.Members = {typeDescriptions.get(), static_cast<std::size_t>( fieldCount )};
        engineDataMap->TypeDescriptions = std::move( typeDescriptions );

        it = g_EngineTypeDescriptionsToGame.emplace( fields, std::move( engineDataMap ) ).first;
    }

    return &it->second->Map;
}

void SaveWriteFields( SAVERESTOREDATA* pSaveData, const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount )
{
    if( !CSaveRestoreBuffer::IsValidSaveRestoreData( pSaveData ) )
    {
        return;
    }

    auto dataMap = GetOrCreateDataMap( pname, pFields, fieldCount );

    CSave saveHelper( *pSaveData );
    saveHelper.WriteFields( pBaseData, *dataMap, *dataMap );
}

void SaveReadFields( SAVERESTOREDATA* pSaveData, const char* pname, void* pBaseData, TYPEDESCRIPTION* pFields, int fieldCount )
{
    if( !CSaveRestoreBuffer::IsValidSaveRestoreData( pSaveData ) )
    {
        return;
    }

    // Will happen here if we're loading a saved game
    // ETABLE is the first chunk of data read after the engine has set up some global variables that we need
    if( 0 == strcmp( pname, "ETABLE" ) )
    {
        g_Server.CheckForNewMapStart( true );
    }

    // Always check if the player is stuck when loading a save game.
    g_CheckForPlayerStuck = true;

    auto dataMap = GetOrCreateDataMap( pname, pFields, fieldCount );

    CRestore restoreHelper( *pSaveData );
    restoreHelper.ReadFields( pBaseData, *dataMap, *dataMap );
}

static void CheckForBackwardsBounds( CBaseEntity* entity )
{
    if( UTIL_FixBoundsVectors( entity->m_CustomHullMin, entity->m_CustomHullMax ) )
    {
        // Can't log the targetname because it may not have been set yet.
        CBaseEntity::Logger->warn( "Backwards mins/maxs fixed in custom hull (entity index {})", entity->entindex() );
    }
}

static void LoadReplacementMap( const ReplacementMap*& destination, string_t fileName, const ReplacementMapOptions& options )
{
    const char* fileNameString = STRING( fileName );

    if( FStrEq( fileNameString, "" ) )
    {
        return;
    }

    auto result = g_ReplacementMaps.Load( fileNameString, options );

    // Only overwrite destination if we successfully loaded something.
    if( result )
    {
        destination = result;
    }
}

static void LoadFileNameReplacementMap( const ReplacementMap*& destination, string_t fileName )
{
    return LoadReplacementMap( destination, fileName, {.CaseSensitive = false, .LoadFromAllPaths = true} );
}

static void LoadSentenceReplacementMap( const ReplacementMap*& destination, string_t fileName )
{
    return LoadReplacementMap( destination, fileName, {.CaseSensitive = true, .LoadFromAllPaths = true} );
}

bool CBaseEntity::RequiredKeyValue( KeyValueData* pkvd )
{
    // Theorically should only catch supported ones but it's fine for now
    if( pkvd->szKeyName[0] == '$' )
    {
        m_CustomKeyValues.SetValue( pkvd->szKeyName, pkvd->szValue );
    }
    // Replacement maps can be changed at runtime using trigger_changekeyvalue.
    // Note that this may cause host_error or sys_error if files aren't precached.
    else if( FStrEq( pkvd->szKeyName, "configlist" ) )
    {
        m_config = g_cfg.CustomConfigurationFile( pkvd->szValue );
        return ( m_config >= 0 );
    }
    else if( FStrEq( pkvd->szKeyName, "modellist" ) )
    {
        m_ModelReplacementFileName = ALLOC_STRING( pkvd->szValue );
        LoadFileNameReplacementMap( m_ModelReplacement, m_ModelReplacementFileName );
    }
    else if( FStrEq( pkvd->szKeyName, "soundlist" ) )
    {
        m_SoundReplacementFileName = ALLOC_STRING( pkvd->szValue );
        LoadFileNameReplacementMap( m_SoundReplacement, m_SoundReplacementFileName );
    }
    else if( FStrEq( pkvd->szKeyName, "sentencelist" ) )
    {
        m_SentenceReplacementFileName = ALLOC_STRING( pkvd->szValue );
        LoadSentenceReplacementMap( m_SentenceReplacement, m_SentenceReplacementFileName );
    }
    // Note: while this code does fix backwards bounds here it will not apply to partial hulls mixing with hard-coded ones.
    else if( FStrEq( pkvd->szKeyName, "minhullsize" ) )
    {
        UTIL_StringToVector( m_CustomHullMin, pkvd->szValue );
        m_HasCustomHullMin = true;

        if( m_HasCustomHullMax )
        {
            CheckForBackwardsBounds( this );
        }
    }
    else if( FStrEq( pkvd->szKeyName, "maxhullsize" ) )
    {
        UTIL_StringToVector( m_CustomHullMax, pkvd->szValue );
        m_HasCustomHullMax = true;

        if( m_HasCustomHullMin )
        {
            CheckForBackwardsBounds( this );
        }
    }
    else if( FStrEq( pkvd->szKeyName, "m_Activator" ) )
    {
        m_sNewActivator = ALLOC_STRING( pkvd->szValue );
    }
    else if( strstr( pkvd->szKeyName, "appearflag_" ) != nullptr )
    {
        return AppearFlags(pkvd->szKeyName, atoi(pkvd->szValue));
    }
    else if( strstr( pkvd->szKeyName, "cfg_" ) != nullptr )
    {
        std::string name = std::string( pkvd->szKeyName ).substr(4);

        auto it = std::find_if( m_ConfigVariables.begin(), m_ConfigVariables.end(), [&]( const auto& variable )
            { return variable.Name == name; } );

        if( it == m_ConfigVariables.end() )
        {
            ConfigurationSystem::ConfigVariable variable{
                .Name = std::string( name ),
                .CurrentValue = 0,
                .InitialValue = 0
            };
            m_ConfigVariables.emplace_back( std::move( variable ) );
            it = m_ConfigVariables.end() - 1;
        }

        char *endptr;
        strtof( pkvd->szValue, &endptr );
        if( *endptr != '\0' )
            it->CurrentValue = atof( pkvd->szValue );
        else
            it->StringValue = std::string( pkvd->szValue );

        CBaseEntity::Logger->debug( "Config value \"{}\" changed to \"{}\" for {}::{}",
            pkvd->szKeyName, pkvd->szValue, STRING( pev->classname ), STRING( pev->targetname ) );

        return false;
    }
    else if( FStrEq( pkvd->szKeyName, "m_UseType" ) )
    {
        int value = atoi( pkvd->szValue );

        if( value >= USE_UNKNOWN || value <= USE_UNSET )
            return false;

        m_UseType = static_cast<USE_TYPE>( value );
    }
    else if( FStrEq( pkvd->szKeyName, "m_UseLockType" ) )
    {
        m_UseLockType = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_uselos" ) )
    {
        m_uselos = atoi( pkvd->szValue ) == 1;
    }
    else return false;
    return true;
}

void CBaseEntity::SetOrigin( const Vector& origin )
{
    g_engfuncs.pfnSetOrigin( edict(), origin );
}

void CBaseEntity::LoadReplacementFiles()
{
    LoadFileNameReplacementMap( m_ModelReplacement, m_ModelReplacementFileName );
    LoadFileNameReplacementMap( m_SoundReplacement, m_SoundReplacementFileName );
    LoadSentenceReplacementMap( m_SentenceReplacement, m_SentenceReplacementFileName );
}

int CBaseEntity::PrecacheModel( const char* s )
{
    if( m_ModelReplacement )
    {
        s = m_ModelReplacement->Lookup( s );
    }

    return UTIL_PrecacheModel( s );
}

void CBaseEntity::SetModel( const char* s )
{
    if( m_ModelReplacement )
    {
        s = m_ModelReplacement->Lookup( s );
    }

    s = UTIL_CheckForGlobalModelReplacement( s );

    g_engfuncs.pfnSetModel( edict(), s );

    if( m_HasCustomHullMin || m_HasCustomHullMax )
    {
        SetSize( pev->mins, pev->maxs );
    }
}

int CBaseEntity::PrecacheSound( const char* s )
{
    if( s[0] == '*' )
    {
        ++s;
    }

    if( m_SoundReplacement )
    {
        s = m_SoundReplacement->Lookup( s );
    }

    return UTIL_PrecacheSound( s );
}

void CBaseEntity::SetSize( const Vector& min, const Vector& max )
{
    if( m_HasZeroOrigin ) // Set g_vecZero to mark as absolute size in the world
        SetOrigin( g_vecZero );

    // If it's a studiomodel and has a scale let's scale the bbox too
    if( strstr( STRING( pev->model ), ".mdl" ) != nullptr && pev->scale != 0.0 && pev->scale != 1.0 )
    {
        g_engfuncs.pfnSetSize( edict(),
            m_HasCustomHullMin ? m_CustomHullMin * pev->scale : min * pev->scale,
            m_HasCustomHullMax ? m_CustomHullMax * pev->scale : max * pev->scale
    ); }
    else {
        g_engfuncs.pfnSetSize( edict(),
            m_HasCustomHullMin ? m_CustomHullMin : min,
            m_HasCustomHullMax ? m_CustomHullMax : max
    ); }
}

bool CBaseEntity::GiveHealth( float flHealth, int bitsDamageType )
{
    if( 0 == pev->takedamage )
        return false;

    // heal
    if( pev->health >= pev->max_health )
        return false;

    pev->health += flHealth;

    if( pev->health > pev->max_health )
        pev->health = pev->max_health;

    return true;
}

bool CBaseEntity::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType )
{
    Vector vecTemp;

    if( 0 == pev->takedamage )
        return false;

    // UNDONE: some entity types may be immune or resistant to some bitsDamageType

    // if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
    // (that is, no actual entity projectile was involved in the attack so use the shooter's origin).
    if( attacker == inflictor )
    {
        vecTemp = inflictor->pev->origin - ( VecBModelOrigin( this ) );
    }
    else
    // an actual missile was involved.
    {
        vecTemp = inflictor->pev->origin - ( VecBModelOrigin( this ) );
    }

    // this global is still used for glass and other non-monster killables, along with decals.
    g_vecAttackDir = vecTemp.Normalize();

    // save damage based on the target's armor level

    // figure momentum add (don't let hurt brushes or other triggers move player)
    if( ( !FNullEnt( inflictor ) ) && ( pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP ) && ( attacker->pev->solid != SOLID_TRIGGER ) )
    {
        Vector vecDir = pev->origin - ( inflictor->pev->absmin + inflictor->pev->absmax ) * 0.5;
        vecDir = vecDir.Normalize();

        float flForce = flDamage * ( ( 32 * 32 * 72.0 ) / ( pev->size.x * pev->size.y * pev->size.z ) ) * 5;

        if( flForce > 1000.0 )
            flForce = 1000.0;
        pev->velocity = pev->velocity + vecDir * flForce;
    }

    // do the damage
    pev->health -= flDamage;
    if( pev->health <= 0 )
    {
        Killed( attacker, GIB_NORMAL );
        return false;
    }

    return true;
}

void CBaseEntity::Killed( CBaseEntity* attacker, int iGib )
{
    pev->takedamage = DAMAGE_NO;
    pev->deadflag = DEAD_DEAD;
    UTIL_Remove( this );
}

CBaseEntity* CBaseEntity::GetNextTarget()
{
    if( FStringNull( pev->target ) )
        return nullptr;
    return UTIL_FindEntityByTargetname( nullptr, STRING( pev->target ) );
}

// Initialize absmin & absmax to the appropriate box
void SetObjectCollisionBox( entvars_t* pev )
{
    if( ( pev->solid == SOLID_BSP ) && pev->angles != g_vecZero )
    { // expand for rotation
        float max, v;
        int i;

        max = 0;
        for( i = 0; i < 3; i++ )
        {
            v = fabs( pev->mins[i] );
            if( v > max )
                max = v;
            v = fabs( pev->maxs[i] );
            if( v > max )
                max = v;
        }
        for( i = 0; i < 3; i++ )
        {
            pev->absmin[i] = pev->origin[i] - max;
            pev->absmax[i] = pev->origin[i] + max;
        }
    }
    else
    {
        pev->absmin = pev->origin + pev->mins;
        pev->absmax = pev->origin + pev->maxs;
    }

    pev->absmin.x -= 1;
    pev->absmin.y -= 1;
    pev->absmin.z -= 1;
    pev->absmax.x += 1;
    pev->absmax.y += 1;
    pev->absmax.z += 1;
}

void CBaseEntity::SetObjectCollisionBox()
{
    ::SetObjectCollisionBox( pev );
}

bool CBaseEntity::Intersects( CBaseEntity* pOther )
{
    for( int i = 0; i < 3; i++ ) {
        if( pOther->pev->absmin[i] > pev->absmax[i] || pOther->pev->absmax[i] < pev->absmin[i] ) {
            return false;
        }
    }
    return true;
}

void CBaseEntity::MakeDormant()
{
    SetBits( pev->flags, FL_DORMANT );

    // Don't touch
    pev->solid = SOLID_NOT;
    // Don't move
    pev->movetype = MOVETYPE_NONE;
    // Don't draw
    SetBits( pev->effects, EF_NODRAW );
    // Don't think
    pev->nextthink = 0;
    // Relink
    SetOrigin( pev->origin );
}

bool CBaseEntity::IsDormant()
{
    return FBitSet( pev->flags, FL_DORMANT );
}

bool CBaseEntity::IsLockedByMaster( CBaseEntity* pActivator )
{
    if( FBitSet( m_UseLocked, USE_VALUE_MASTER ) )
        return true;

    if( FStringNull( m_sMaster ) )
        return false;

    auto master = UTIL_FindEntityByTargetname( nullptr, STRING( m_sMaster ) );

    if( FNullEnt( master ) )
        return false;

    if( ( master->ObjectCaps() & FCAP_MASTER ) == 0 )
        return false;

    if( master->IsTriggered( ( pActivator != nullptr ? pActivator : m_hActivator.Get() ) ) )
        return false;

    return true;
}

bool CBaseEntity::IsInWorld()
{
    // position
    for( int i = 0; i < 3; i++ ) {
        if( pev->origin[i] >= World->m_CustomHullMax[i] || pev->origin[i] <= World->m_CustomHullMin[i] ) {
            return false;
        }
    }

    // speed
    for( int i = 0; i < 3; i++ ) {
        if( pev->velocity[i] >= 2000 || pev->velocity[i] <= -2000 ) {
            return false;
        }
    }

    return true;
}

bool CBaseEntity::ShouldToggle( USE_TYPE useType, bool currentState )
{
    switch( useType )
    {
        case USE_TOGGLE:
            return true;
        case USE_ON:
            return !currentState; // True if it's disabled
        case USE_OFF:
            return currentState; // True if it's enabled
        case USE_SET:
            return !currentState; // -TODO Return the original state. i.e if it spawned being active or inactive.
    }
    return false;
}

int CBaseEntity::DamageDecal( int bitsDamageType )
{
    if( pev->rendermode == kRenderTransAlpha )
        return -1;

    if( pev->rendermode != kRenderNormal )
        return DECAL_BPROOF1;

    return DECAL_GUNSHOT1 + RANDOM_LONG( 0, 4 );
}

CBaseEntity* CBaseEntity::Create( const char* szName, const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* owner, bool callSpawn )
{
    auto entity = g_EntityDictionary->Create( szName );

    if( FNullEnt( entity ) )
    {
        CBaseEntity::Logger->debug( "NULL Ent in Create!" );
        return nullptr;
    }

    entity->SetOwner( owner );
    entity->pev->origin = vecOrigin;
    entity->pev->angles = vecAngles;

    if( owner != nullptr && owner->m_config >= 0 )
        entity->m_config = owner->m_config;

    if( callSpawn )
    {
        DispatchSpawn( entity->edict() );
    }

    return entity;
}

void CBaseEntity::EmitSound( int channel, const char* sample, float volume, float attenuation )
{
    sound::g_ServerSound.EmitSound( this, channel, sample, volume, attenuation, 0, PITCH_NORM );
}

void CBaseEntity::EmitSoundDyn( int channel, const char* sample, float volume, float attenuation, int flags, int pitch )
{
    sound::g_ServerSound.EmitSound( this, channel, sample, volume, attenuation, flags, pitch );
}

void CBaseEntity::EmitAmbientSound( const Vector& vecOrigin, const char* samp, float vol, float attenuation, int fFlags, int pitch )
{
    sound::g_ServerSound.EmitAmbientSound( this, vecOrigin, samp, vol, attenuation, fFlags, pitch );
}

void CBaseEntity::StopSound( int channel, const char* sample )
{
    sound::g_ServerSound.EmitSound( this, channel, sample, 0, 0, SND_STOP, PITCH_NORM );
}

CBaseEntity* CBaseEntity::AllocNewActivator( CBaseEntity* pActivator, CBaseEntity* pCaller, string_t szNewTarget, CBaseEntity* failback )
{
    if( !FStringNull( szNewTarget ) )
    {
        if( FStrEq( STRING( szNewTarget ), "!activator" ) )
        {
            return pActivator;
        }
        else if( FStrEq( STRING( szNewTarget ), "!caller" ) )
        {
            return pCaller;
        }
        else if( FStrEq( STRING( szNewTarget ), "!this" ) || FStrEq( STRING( szNewTarget ), "!self" ) )
        {
            return this;
        }
        else if( FStrEq( STRING( szNewTarget ), "!player" ) )
        {
            return static_cast<CBaseEntity*>( UTIL_FindNearestPlayer( pev->origin ) );
        }
        else
        {
            return UTIL_FindEntityByTargetname( nullptr, STRING( szNewTarget ) );
        }
    }

    return failback;
}

SpawnAction CBaseEntity::GlobalState()
{
    // Handle global stuff here
    if( !FStringNull( pev->globalname ) )
    {
        const globalentity_t* pGlobal = gGlobalState.EntityFromTable( pev->globalname );

        if( pGlobal )
        {
            // Already dead? delete
            if( pGlobal->state == GLOBAL_DEAD )
            {
                return SpawnAction::RemoveNow;
            }
            else if( !FStrEq( STRING( gpGlobals->mapname ), pGlobal->levelName ) )
            {
                MakeDormant(); // Hasn't been moved to this level yet, wait but stay alive
                                        // In this level & not dead, continue on as normal
            }
        }
        else
        {
            // Spawned entities default to 'On'
            gGlobalState.EntityAdd( pev->globalname, gpGlobals->mapname, GLOBAL_ON );
            // CBaseEntity::Logger->trace("Added global entity {} ({})", STRING(entity->pev->classname), STRING(entity->pev->globalname));
        }
    }

    return SpawnAction::Spawn;
}

bool CBaseEntity::AppearFlags( const char* keyname, int value )
{
    value = std::max( -1, std::min( 1, value ) );

    auto ShouldAppear = [&]( bool condition ) -> bool
    {
        switch( value )
        {
            case -1: // Does not appears when condition is true
            {
                if( condition ) {
                    m_appearflags = SpawnAction::RemoveNow;
                    CBaseEntity::Logger->debug( "Entity got appearflags \"Not in\" for key \"{}\"", keyname );
                    return false;
                }
                break;
            }
            case 1: // Only appears when condition is true
            {
                if( !condition ) {
                    m_appearflags = SpawnAction::RemoveNow;
                    CBaseEntity::Logger->debug( "Entity got appearflags \"Only in\" for key \"{}\"", keyname );
                    return false;
                }
                break;
            }
        }

        return true;
    };

    // You can add more appearance conditions here
    if( FStrEq( keyname, "appearflag_multiplayer" ) ) {
        ShouldAppear( g_GameMode->IsMultiplayer() );
    }
    else if( FStrEq( keyname, "appearflag_cooperative" ) ) {
        ShouldAppear( g_GameMode->IsGamemode( "coop"sv ) );
    }
    else if( FStrEq( keyname, "appearflag_skilleasy" ) ) {
        ShouldAppear( g_cfg.GetSkillLevel() == SkillLevel::Easy );
    }
    else if( FStrEq( keyname, "appearflag_skillmedium" ) ) {
        ShouldAppear( g_cfg.GetSkillLevel() == SkillLevel::Medium );
    }
    else if( FStrEq( keyname, "appearflag_skillhard" ) ) {
        ShouldAppear( g_cfg.GetSkillLevel() == SkillLevel::Hard );
    }
    else if( FStrEq( keyname, "appearflag_deathmatch" ) ) {
        ShouldAppear( g_GameMode->IsGamemode( "deathmatch"sv ) );
    }
    else if( FStrEq( keyname, "appearflag_teamplay" ) ) {
        ShouldAppear( g_GameMode->IsGamemode( "teamplay"sv ) );
    }
    else if( FStrEq( keyname, "appearflag_ctf" ) ) {
        ShouldAppear( g_GameMode->IsGamemode( "ctf"sv ) );
    }
    else if( FStrEq( keyname, "appearflag_dedicated" ) ) {
        ShouldAppear( IS_DEDICATED_SERVER() );
    }
    else {
        return false;
    }
    return true;
}

std::string& CBaseEntity::KeyValueManager::GetValue( std::string_view key )
{
    std::string keyname = std::string( key );

    if( KeyValues.find( keyname ) != KeyValues.end() )
    {
        return KeyValues.at( keyname );
    }

    static std::string empty;
    return empty;
}

void CBaseEntity::KeyValueManager::SetValue( std::string_view key, std::string_view value )
{
    // If we're in multiplayer SaveRestore shouldn't be important, maybe skip this condition?
    if( auto tsize = KeyValues.size(); tsize >= MAX_KEYVALUEMANAGER_ENTRIES ) {
        CBaseEntity::Logger->warn( "Couldn't add key-value pair \"{}:{}\" list on size limit {}!", key, value, tsize );
        return;
    }

    KeyValues[ std::string( key ) ] = std::string( value );
}

void CBaseEntity::KeyValueManager::Restore()
{
    for( int i = 0; i < size; ++i )
    {
        SetValue( STRING( m_Keys[i] ), STRING( m_Values[i] ) );
    }
}

void CBaseEntity::KeyValueManager::Save()
{
    // Custom keyvalues are always saved
    if( custom || g_Server.GetMapState()->KeyValueManager )
    {
        for( const auto& [ key, value ] : KeyValues )
        {
            m_Keys[size] = ALLOC_STRING( key.c_str() );
            m_Values[size] = ALLOC_STRING( value.c_str() );

            ++size;
        }
    }
}
