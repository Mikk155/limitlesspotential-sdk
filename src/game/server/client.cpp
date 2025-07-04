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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//                   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <algorithm>
#include <optional>

#include "cbase.h"
#include "changelevel.h"
#include "CCorpse.h"
#include "com_model.h"
#include "client.h"
#include "customentity.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "UserMessages.h"
#include "ClientCommandRegistry.h"
#include "ServerLibrary.h"
#include "AdminInterface.h"

#include "ctf/ctf_goals.h"

unsigned int g_ulFrameCount;

/**
 *    @brief called when a player connects to a server
 */
qboolean ClientConnect( edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128] )
{
    return static_cast<qboolean>( g_Server.CanPlayerConnect( pEntity, pszName, pszAddress, szRejectReason ) );

    // a client connecting during an intermission can cause problems
    //    if (intermission_running)
    //        ExitIntermission ();
}


/**
 *    @brief called when a player disconnects from a server
 */
void ClientDisconnect( edict_t* pEntity )
{
    if( g_fGameOver )
        return;

    char text[256] = "";
    if( !FStringNull( pEntity->v.netname ) )
        snprintf( text, sizeof( text ), "- %s has left the game\n", STRING( pEntity->v.netname ) );
    text[sizeof( text ) - 1] = 0;
    MESSAGE_BEGIN( MSG_ALL, gmsgSayText );
    WRITE_BYTE( ENTINDEX( pEntity ) );
    WRITE_STRING( text );
    MESSAGE_END();

    CSound* pSound;
    pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
    {
        // since this client isn't around to think anymore, reset their sound.
        if( pSound )
        {
            pSound->Reset();
        }
    }

    // since the edict doesn't get deleted, fix it so it doesn't interfere.
    pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
    pEntity->v.solid = SOLID_NOT;       // nonsolid

    auto pPlayer = ToBasePlayer( pEntity );

    TriggerEvent( TriggerEventType::PlayerLeftServer, pPlayer, pPlayer );

    if( pPlayer )
    {
        pPlayer->m_Connected = false;

        pPlayer->SetOrigin( pPlayer->pev->origin );

        if( pPlayer->m_pTank != nullptr )
        {
            pPlayer->m_pTank->Use( pPlayer, pPlayer, USE_OFF );
            pPlayer->m_pTank = nullptr;
        }
    }

    g_pGameRules->ClientDisconnected( pEntity );
}

/**
 *    @brief called by ClientKill and DeadThink
 */
void respawn( CBasePlayer* player, bool fCopyCorpse )
{
    if( g_GameMode->IsMultiplayer() )
    {
        if( fCopyCorpse )
        {
            // make a copy of the dead body for appearances sake
            CopyToBodyQue( player );
        }

        // respawn player
        player->Spawn();
    }
    else
    { // restart the entire server
        SERVER_COMMAND( "reload\n" );
    }
}

/**
 *    @brief Player entered the suicide command
 */
void ClientKill( edict_t* pEntity )
{
    CBasePlayer* pl = ToBasePlayer( pEntity );

    assert( pl );

    // Only check for teams in CTF gamemode
    if( ( pl->pev->flags & FL_SPECTATOR ) != 0 || ( g_GameMode->IsGamemode( "ctf"sv ) && pl->m_iTeamNum == CTFTeam::None ) )
    {
        return;
    }

    if( pl->m_fNextSuicideTime > gpGlobals->time )
        return; // prevent suiciding too ofter

    pl->m_fNextSuicideTime = gpGlobals->time + 1; // don't let them suicide for 5 seconds after suiciding

    // have the player kill themself
    pl->pev->health = 0;
    pl->Killed( pl, GIB_NEVER );

    //    pev->frags -= 2;        // extra penalty
    //    respawn( pev );
}

/**
 *    @brief called each time a player is spawned
 */
void ClientPutInServer( edict_t* pEntity )
{
    auto pPlayer = g_EntityDictionary->Create<CBasePlayer>( "player", &pEntity->v );
    pPlayer->SetCustomDecalFrames( -1 ); // Assume none;

    // Allocate a CBasePlayer for pev, and call spawn
    pPlayer->Spawn();

    // Reset interpolation during first frame
    pPlayer->pev->effects |= EF_NOINTERP;

    // Player can be made spectator on spawn, so don't do this
    /*
    pPlayer->pev->iuser1 = 0;    // disable any spec modes
    pPlayer->pev->iuser2 = 0;
    */

    pPlayer->m_Connected = true;
    pPlayer->m_ConnectTime = gpGlobals->time;

    g_LastPlayerJoinTime = gpGlobals->time;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

#if defined( _MSC_VER ) || defined( WIN32 )
typedef wchar_t uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

/**
 *    @brief determine if a uchar32 represents a valid Unicode code point
 */
bool Q_IsValidUChar32( uchar32 uVal )
{
    // Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
    // values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
    return ( uVal < 0x110000u ) && ( ( uVal - 0x00D800u ) > 0x7FFu ) && ( ( uVal & 0xFFFFu ) < 0xFFFEu ) && ( ( uVal - 0x00FDD0u ) > 0x1Fu );
}

/*
 *    @brief Decode one character from a UTF-8 encoded string.
 *    @details Treats 6-byte CESU-8 sequences as a single character,
 *    as if they were a correctly-encoded 4-byte UTF-8 sequence.
 */
int Q_UTF8ToUChar32( const char* pUTF8_, uchar32& uValueOut, bool& bErrorOut )
{
    const uint8* pUTF8 = ( const uint8* )pUTF8_;

    int nBytes = 1;
    uint32 uValue = pUTF8[0];
    uint32 uMinValue = 0;

    // 0....... single byte
    if( uValue < 0x80 )
        goto decodeFinishedNoCheck;

    // Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
    if( ( uValue - 0xC0u ) > 0x37u || ( pUTF8[1] & 0xC0 ) != 0x80 )
        goto decodeError;

    uValue = ( uValue << 6 ) - ( 0xC0 << 6 ) + pUTF8[1] - 0x80;
    nBytes = 2;
    uMinValue = 0x80;

    // 110..... two-byte lead byte
    if( 0 == ( uValue & ( 0x20 << 6 ) ) )
        goto decodeFinished;

    // Expecting at least a three-byte sequence
    if( ( pUTF8[2] & 0xC0 ) != 0x80 )
        goto decodeError;

    uValue = ( uValue << 6 ) - ( 0x20 << 12 ) + pUTF8[2] - 0x80;
    nBytes = 3;
    uMinValue = 0x800;

    // 1110.... three-byte lead byte
    if( 0 == ( uValue & ( 0x10 << 12 ) ) )
        goto decodeFinishedMaybeCESU8;

    // Expecting a four-byte sequence, longest permissible in UTF-8
    if( ( pUTF8[3] & 0xC0 ) != 0x80 )
        goto decodeError;

    uValue = ( uValue << 6 ) - ( 0x10 << 18 ) + pUTF8[3] - 0x80;
    nBytes = 4;
    uMinValue = 0x10000;

    // 11110... four-byte lead byte. fall through to finished.

decodeFinished:
    if( uValue >= uMinValue && Q_IsValidUChar32( uValue ) )
    {
    decodeFinishedNoCheck:
        uValueOut = uValue;
        bErrorOut = false;
        return nBytes;
    }
decodeError:
    uValueOut = '?';
    bErrorOut = true;
    return nBytes;

decodeFinishedMaybeCESU8:
    // Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
    // That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
    if( ( uValue - 0xD800u ) < 0x400u && pUTF8[3] == 0xED && ( uint8 )( pUTF8[4] - 0xB0 ) < 0x10 && ( pUTF8[5] & 0xC0 ) == 0x80 )
    {
        uValue = 0x10000 + ( ( uValue - 0xD800u ) << 10 ) + ( ( uint8 )( pUTF8[4] - 0xB0 ) << 6 ) + pUTF8[5] - 0x80;
        nBytes = 6;
        uMinValue = 0x10000;
    }
    goto decodeFinished;
}

/**
 *    @brief Returns true if UTF-8 string contains invalid sequences.
 */
bool Q_UnicodeValidate( const char* pUTF8 )
{
    bool bError = false;
    while( '\0' != *pUTF8 )
    {
        uchar32 uVal;
        // Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
        // However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
        int nCharSize = Q_UTF8ToUChar32( pUTF8, uVal, bError );
        if( bError || nCharSize == 6 )
            return false;
        pUTF8 += nCharSize;
    }
    return true;
}

/**
 *    @brief Handles incoming @c say and @c say_team commands and sends them to other players.
 *    <pre>
 *    String comes in as
 *    say blah blah blah
 *    or as
 *    blah blah blah
 *    </pre>
 */
void Host_Say( CBasePlayer* player, bool teamonly )
{
    int j;
    char* p;
    char text[128];
    char szTemp[256];
    const char* cpSay = "say";
    const char* cpSayTeam = "say_team";
    const char* pcmd = CMD_ARGV( 0 );

    // We can get a raw string now, without the "say " prepended
    if( CMD_ARGC() == 0 )
        return;

    // Only limit chat in multiplayer.
    // Not yet.
    if( g_GameMode->IsMultiplayer() && player->m_flNextChatTime > gpGlobals->time )
        return;

    if( !stricmp( pcmd, cpSay ) || !stricmp( pcmd, cpSayTeam ) )
    {
        if( CMD_ARGC() >= 2 )
        {
            p = (char*)CMD_ARGS();
        }
        else
        {
            // say with a blank message, nothing to do
            return;
        }
    }
    else // Raw text, need to prepend argv[0]
    {
        if( CMD_ARGC() >= 2 )
        {
            sprintf( szTemp, "%s %s", pcmd, CMD_ARGS() );
        }
        else
        {
            // Just a one word command, use the first word...sigh
            sprintf( szTemp, "%s", pcmd );
        }
        p = szTemp;
    }

    // remove quotes if present
    if( *p == '"' )
    {
        p++;
        p[strlen( p ) - 1] = 0;
    }

    // make sure the text has content

    if( !p || '\0' == p[0] || !Q_UnicodeValidate( p ) )
        return; // no character found, so say nothing

    // turn on color set 2  (color on,  no sound)
    // turn on color set 2  (color on,  no sound)
    if( player->IsObserver() && ( teamonly ) )
        sprintf( text, "%c(SPEC) %s: ", 2, STRING( player->pev->netname ) );
    else if( teamonly )
        sprintf( text, "%c(TEAM) %s: ", 2, STRING( player->pev->netname ) );
    else
        sprintf( text, "%c%s: ", 2, STRING( player->pev->netname ) );

    j = sizeof( text ) - 2 - strlen( text ); // -2 for /n and null terminator
    if( (int)strlen( p ) > j )
        p[j] = 0;

    strcat( text, p );
    strcat( text, "\n" );


    player->m_flNextChatTime = gpGlobals->time + std::min( 0.1f, spamdelay.value );

    // loop through all players
    // Start with the first player.
    // This may return the world in single player if the client types something between levels or during spawn
    // so check it, or it will infinite loop

    for( auto client : UTIL_FindPlayers() )
    {
        if( !client->pev )
            continue;

        if( client == player )
            continue;

        if( !( client->IsNetClient() ) ) // Not a client ? (should never be true)
            continue;

        // can the receiver hear the sender? or has he muted him?
        if( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, player ) )
            continue; // -TODO Mode to mute only chat, only voice or both

        if( !player->IsObserver() && teamonly && g_pGameRules->PlayerRelationship( client, player ) != GR_TEAMMATE )
            continue;

        // Spectators can only talk to other specs
        if( player->IsObserver() && teamonly )
            if( !client->IsObserver() )
                continue;

        MESSAGE_BEGIN( MSG_ONE, gmsgSayText, nullptr, client );
        WRITE_BYTE( player->entindex() );
        WRITE_STRING( text );
        MESSAGE_END();
    }

    // print to the sending client
    MESSAGE_BEGIN( MSG_ONE, gmsgSayText, nullptr, player );
    WRITE_BYTE( player->entindex() );
    WRITE_STRING( text );
    MESSAGE_END();

    // echo to server console
    g_engfuncs.pfnServerPrint( text );

    const char* temp;
    if( teamonly )
        temp = "say_team";
    else
        temp = "say";

    CGameRules::Logger->trace( "{} {} \"{}\"", PlayerLogInfo{*player}, temp, p );
}

const int NumberOfEntitiesPerPage = 10;

struct PageSearchResult
{
    int TotalEntityCount{0};
    int DesiredPage{0};
    int PageCount{0};
};

/**
 *    @brief Searches for entities using a page based search.
 *    @param desiredPage Which page to display, or 0 to display all.
 *    @param filter Callback to filter entities.
 */
template <typename Function>
PageSearchResult PageBasedEntitySearch( CBasePlayer* player, int desiredPage, Function&& filter )
{
    desiredPage = std::max( desiredPage, 1 );

    int pageCount = 0;
    int totalCount = 0;
    int currentPage = 1;

    UTIL_ConsolePrint( player, "entindex - classname - targetname - origin\n" );

    for( auto entity : UTIL_FindEntities() )
    {
        if( !filter( entity ) )
        {
            continue;
        }

        ++totalCount;

        if( currentPage == desiredPage )
        {
            UTIL_ConsolePrint( player, "{} - {} - {} - {{{}}}\n",
                entity->entindex(), entity->GetClassname(), entity->GetTargetname(), entity->pev->origin );
        }

        ++pageCount;
        if( pageCount >= NumberOfEntitiesPerPage )
        {
            ++currentPage;
            pageCount = 0;
        }
    }

    return {.TotalEntityCount = totalCount, .DesiredPage = desiredPage, .PageCount = currentPage};
}

void PrintPageSearchResult( CBasePlayer* player, const PageSearchResult& result, const char* filterName, const char* filterContents )
{
    UTIL_ConsolePrint( player, "{} entities having the {}: \"{}\" (page {} / {})\n",
        result.TotalEntityCount, filterName, filterContents, result.DesiredPage, result.PageCount );
}

void PrintPageSearchResult( CBasePlayer* player, const PageSearchResult& result )
{
    UTIL_ConsolePrint( player, "Total {} / {} entities (page {} / {})\n",
        result.TotalEntityCount, gpGlobals->maxEntities, result.DesiredPage, result.PageCount );
}

static CBaseEntity* TryCreateEntity( CBasePlayer* player, const char* className, const Vector& angles,
    const CommandArgs& args, int firstKeyValue )
{
    Vector forward;
    AngleVectors( player->pev->v_angle, &forward, nullptr, nullptr );

    Vector origin = player->pev->origin + forward * MAX_EXTENT;

    TraceResult tr;
    UTIL_TraceLine( player->pev->origin, origin, dont_ignore_monsters, player->edict(), &tr );

    if( tr.fStartSolid != 0 )
    {
        UTIL_ConsolePrint( player, "Cannot create entity in solid object\n" );
        return nullptr;
    }

    auto entity = CBaseEntity::Create( className, tr.vecEndPos, Vector{0, angles.y, 0}, nullptr, false );

    if( !entity )
    {
        return nullptr;
    }

    const int keyValues = args.Count();

    KeyValueData kvd{.szClassName = className};

    for( int i = firstKeyValue; i < keyValues; i += 2 )
    {
        kvd.szKeyName = args.Argument( i );
        kvd.szValue = args.Argument( i + 1 );
        kvd.fHandled = 0;

        // Skip the classname the same way the engine does.
        if( FStrEq( kvd.szValue, className ) )
        {
            continue;
        }

        DispatchKeyValue( entity->edict(), &kvd );
    }

    if( DispatchSpawn( entity->edict() ) == -1 )
    {
        UTIL_ConsolePrint( player, "Entity was removed during spawn\n" );
    }

    return entity;
}

void SV_CreateClientCommands()
{
    g_ClientCommands.Create( "set_hud_color"sv, []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() == 3 && atoi( args.Argument( 2 ) ) == -1 )
        {
            g_GameMode->SetClientHUDColor( atoi( args.Argument( 1 ) ), player->entindex() );
        }
        else if( args.Count() > 2 )
        {
            Vector color{255, 255, 255};

            for( int i = 0; i < std::min( 3, args.Count() - 2 ); i ++ ) {
                color[i] = atoi( args.Argument( i + 2 ) );
            }
            g_GameMode->SetClientHUDColor( atoi( args.Argument( 1 ) ), player->entindex(), RGB24::FromVector( color ) );
        }
        else
        {
            UTIL_ConsolePrint( player, "Usage: set_hud_color <elements> <r> <g> <b> (values in range 0-255)\n" );
            UTIL_ConsolePrint( player, "or set_hud_color <elements> to restore the default color\n" );
            UTIL_ConsolePrint( player, "elements:\nAll = 0\nUncategorized = 1\nCrosshair = 2\n" );
        }
    } );

    g_ClientCommands.Create( "spectate", []( CBasePlayer* player, const auto& args )
    {
        // clients wants to become a spectator
        if( g_GameMode->IsGamemode( "ctf"sv ) )
        {
            // CTF game mode: make sure player has gamemode settings applied properly.
            player->Menu_Team_Input( -1 );
        }
        // always allow proxies to become a spectator
        else if( ( player->pev->flags & FL_PROXY ) != 0 || allow_spectators.value != 0 )
        {
            player->StartObserver( player->pev->origin, player->pev->angles );

            // notify other clients of player switching to spectator mode
            UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s switched to spectator mode\n",
            ( !FStringNull( player->pev->netname ) && STRING( player->pev->netname )[0] != 0 ) ? STRING( player->pev->netname ) : "unconnected" ) );
        }
        else
        {
            ClientPrint( player, HUD_PRINTCONSOLE, "Spectator mode is disabled.\n" );
        }
    } );

    g_ClientCommands.Create( "specmode", []( CBasePlayer* player, const auto& args )
    {
        // new spectator mode
        if( player->IsObserver() )
        {
            player->Observer_SetMode( atoi( CMD_ARGV( 1 ) ) );
        }
    } );

    g_ClientCommands.Create( "say", []( CBasePlayer* player, const auto& args )
    {
        Host_Say( player, false );
    } );

    g_ClientCommands.Create( "say_team", []( CBasePlayer* player, const auto& args )
    {
        Host_Say( player, true );
    } );

    g_ClientCommands.Create( "gibme", []( CBasePlayer* player, const auto& args )
    {
        player->pev->health = 0;
        player->Killed( player, GIB_ALWAYS );
    } );

    g_ClientCommands.Create( "unstuck", []( CBasePlayer* player, const auto& args )
    {
        if( player->m_VecLastFreePosition != g_vecZero )
        {
            player->pev->origin = player->m_VecLastFreePosition;
        }
        else
        {
            // -TODO What's about storing a vector of last known safe hull?
            TraceResult tr;
            UTIL_TraceHull( player->pev->origin, player->pev->origin, dont_ignore_monsters, human_hull, nullptr, &tr );

            if( tr.fStartSolid != 0 || tr.fAllSolid != 0 )
            {
                UTIL_GetNearestHull( player->pev->origin, player->pev->origin, human_hull, 128, 128 );
            }
        }
    } );

    g_ClientCommands.Create( "fullupdate", []( CBasePlayer* player, const auto& args )
    {
        player->ForceClientDllUpdate();
    } );

    g_ClientCommands.Create( "drop", []( CBasePlayer* player, const auto& args )
    {
        // player is dropping an item.
        player->DropPlayerWeapon( args.Argument( 1 ) );
    } );

    g_ClientCommands.Create( "fov", []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() > 1 )
        {
            player->m_iFOV = atoi( args.Argument( 1 ) );
        }
        else
        {
            UTIL_ConsolePrint( player, "\"fov\" is \"{}\"\n", player->m_iFOV );
        }
    } );

    // -TODO To GameModes
    g_ClientCommands.Create( "set_suit_light_type", []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() > 1 )
        {
            const auto type = SuitLightTypeFromString( args.Argument( 1 ) );

            if( type.has_value() )
            {
                player->SetSuitLightType( type.value() );
            }
            else
            {
                UTIL_ConsolePrint( player, "Unknown suit light type \"{}\"\n", args.Argument( 1 ) );
            }
        } },
    {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "use", []( CBasePlayer* player, const auto& args )
    {
            player->SelectItem( args.Argument( 1 ) );
    } );

    g_ClientCommands.Create( "selectweapon", []( CBasePlayer* player, const auto& args )
    {
        if( args.Count() > 1 )
        {
            player->SelectItem( args.Argument( 1 ) );
        }
        else
        {
            UTIL_ConsolePrint( player, "usage: selectweapon <weapon name>\n" );
        }
    } );

    g_ClientCommands.Create( "lastinv", []( CBasePlayer* player, const auto& args )
    {
        player->SelectLastItem();
    } );

    g_ClientCommands.Create( "closemenus", []( CBasePlayer* player, const auto& args )
    {
        /* just ignore it*/
    } );

    g_ClientCommands.Create( "follownext", []( CBasePlayer* player, const auto& args )
    {
        // follow next player
        if( player->IsObserver() )
        {
            player->Observer_FindNextPlayer( atoi( args.Argument( 1 ) ) != 0 );
        }
    } );

    g_ClientCommands.Create( "cheat_god", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::Godmode ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_unkillable", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::Unkillable ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_notarget", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::Notarget ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_noclip", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::Noclip ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_respawn", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( player->IsAlive() ) {
                g_pGameRules->GetPlayerSpawnSpot( player, true ); }
            else {
                player->Spawn(); }
        },{.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_infiniteair", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::InfiniteAir ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_infinitearmor", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::InfiniteArmor ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_jetpack", []( CBasePlayer* player, const CommandArgs& args )
        { player->ToggleCheat( Cheat::Jetpack ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "cheat_givemagazine", []( CBasePlayer* player, const CommandArgs& args )
        {
            int attackMode = 0;

            if( args.Count() >= 2 )
            {
                attackMode = atoi( args.Argument( 1 ) );

                if( attackMode < 0 || attackMode >= MAX_WEAPON_ATTACK_MODES )
                {
                    UTIL_ConsolePrint( player, "Invalid weapon attack mode\n" );
                    return;
                }
            }

            if( player->GiveMagazine( player->m_pActiveWeapon, attackMode ) != -1 )
            {
                player->EmitSound( CHAN_ITEM, DefaultItemPickupSound, VOL_NORM, ATTN_NORM );
            } },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "ent_find_by_classname", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( args.Count() > 1 )
            {
                const auto result = PageBasedEntitySearch( player, args.Count() > 2 ? atoi( args.Argument( 2 ) ) : 1, [&]( auto entity )
                    { return FStrEq( args.Argument( 1 ), entity->GetClassname() ); } );

                PrintPageSearchResult( player, result, "classname", args.Argument( 1 ) );
            }
            else
            {
                UTIL_ConsolePrint( player, "usage: ent_find_by_classname <classname> [page]\n" );
            } },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "ent_find_by_targetname", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( args.Count() > 1 )
            {
                const auto result = PageBasedEntitySearch( player, args.Count() > 2 ? atoi( args.Argument( 2 ) ) : 1, [&]( auto entity )
                    { return FStrEq( args.Argument( 1 ), entity->GetTargetname() ); } );

                PrintPageSearchResult( player, result, "targetname", args.Argument( 1 ) );
            }
            else
            {
                UTIL_ConsolePrint( player, "usage: ent_find_by_targetname <targetname> [page]\n" );
            } },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "ent_list", []( CBasePlayer* player, const CommandArgs& args )
        {
            const auto result = PageBasedEntitySearch( player, args.Count() > 1 ? atoi( args.Argument( 1 ) ) : 1, [&]( auto entity )
                { return true; } );

            PrintPageSearchResult( player, result ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "ent_show_bbox", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( args.Count() < 2 )
            {
                UTIL_ConsolePrint( player, "Usage: ent_show_bbox <targetname>\n" );
                return;
            }

            CBaseEntity* target = UTIL_FindEntityByTargetname( nullptr, args.Argument( 1 ) );

            if( target )
            {
                MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, nullptr, player );
                WRITE_BYTE( TE_BOX );
                WRITE_COORD_VECTOR( target->pev->absmin );
                WRITE_COORD_VECTOR( target->pev->absmax );
                WRITE_SHORT( 100 ); // Stick around for 10 seconds.
                WRITE_BYTE( 0 );
                WRITE_BYTE( 0 );
                WRITE_BYTE( 255 );
                MESSAGE_END();

                UTIL_ConsolePrint( player, "BBox: min {} max {}\n", target->pev->absmin, target->pev->absmax );
            }
            else
            {
                UTIL_ConsolePrint( player, "No entity found\n" );
            } },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "ent_create", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( args.Count() < 2 )
            {
                UTIL_ConsolePrint( player, "Usage: ent_create <classname> [<key> <value> <key2> <value2> ...]\n" );
                return;
            }

            const char* className = args.Argument( 1 );

            TryCreateEntity( player, className, vec3_origin, args, 2 ); },
        {.Flags = ClientCommandFlag::Cheat} );

    g_ClientCommands.Create( "npc_create", []( CBasePlayer* player, const CommandArgs& args )
        {
            if( args.Count() < 2 )
            {
                UTIL_ConsolePrint( player, "Usage: npc_create <classname> [<key> <value> <key2> <value2> ...]\n" );
                return;
            }

            const char* className = args.Argument( 1 );

            auto entity = TryCreateEntity( player, className, player->pev->v_angle, args, 2 );

            if( !entity )
            {
                return;
            }

            // Move NPC up based on its bounding box.
            if( entity->pev->mins.z < 0 )
            {
                entity->pev->origin.z -= entity->pev->mins.z;
            }

            if( ( entity->pev->flags & FL_FLY ) == 0 && entity->pev->movetype != MOVETYPE_FLY )
            {
                // See if the NPC is stuck in something.
                if( DROP_TO_FLOOR( entity->edict() ) == 0 )
                {
                    UTIL_ConsolePrint( player, "NPC fell out of level\n" );
                    UTIL_Remove( entity );
                }
            } },
        {.Flags = ClientCommandFlag::Cheat} );
}

/**
 *    @brief called each time a player uses a @c "cmd" command
 *    @details Use CMD_ARGV, CMD_ARGV, and CMD_ARGC to get pointers the character string command.
 */
void ExecuteClientCommand( edict_t* pEntity )
{
    // Is the client spawned yet?
    if( !pEntity->pvPrivateData )
        return;

    const char* pcmd = CMD_ARGV( 0 );
    auto player = ToBasePlayer( pEntity );

    if( !player )
        return;

    if( auto clientCommand = g_ClientCommands.Find( pcmd ); clientCommand )
    {
        if( ( clientCommand->Flags & ClientCommandFlag::AdminInterface ) != 0 && !g_AdminInterface.HasAccess( player, clientCommand->Name ) )
        {
            UTIL_ConsolePrint( player, "The command \"{}\" can only be used by explicit access\n", clientCommand->Name );
        }
        else if( ( clientCommand->Flags & ClientCommandFlag::Cheat ) != 0 && g_psv_cheats->value != 1 )
        {
            UTIL_ConsolePrint( player, "The command \"{}\" can only be used when cheats are enabled\n", clientCommand->Name );
        }
        else
        {
            clientCommand->Function( player, CommandArgs{} );
        }
    }
    else
    {
        // tell the user they entered an unknown command
        char command[128];

        // check the length of the command (prevents crash)
        // max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
        strncpy( command, pcmd, 127 );
        command[127] = '\0';

        // tell the user they entered an unknown command
        ClientPrint( player, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
    }
}


/**
*    @brief called after the player changes userinfo.
*    gives dll a chance to modify it before it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t* pEntity, char* infobuffer )
{
    // Is the client spawned yet?
    if( !pEntity->pvPrivateData )
        return;

    auto player = ToBasePlayer( pEntity );

    // msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
    if( !FStringNull( pEntity->v.netname ) && STRING( pEntity->v.netname )[0] != 0 && !FStrEq( STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) ) )
    {
        char sName[256];
        char* pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
        strncpy( sName, pName, sizeof( sName ) - 1 );
        sName[sizeof( sName ) - 1] = '\0';

        // First parse the name and remove any %'s
        for( char* pApersand = sName; pApersand != nullptr && *pApersand != 0; pApersand++ )
        {
            // Replace it with a space
            if( *pApersand == '%' )
                *pApersand = ' ';
        }

        // Set the name
        g_engfuncs.pfnSetClientKeyValue( player->entindex(), infobuffer, "name", sName );

        if( g_GameMode->IsMultiplayer() )
        {
            char text[256];
            sprintf( text, "* %s changed name to %s\n", STRING( pEntity->v.netname ), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
            MESSAGE_BEGIN( MSG_ALL, gmsgSayText );
            WRITE_BYTE( player->entindex() );
            WRITE_STRING( text );
            MESSAGE_END();
        }

        CGameRules::Logger->trace( "{} changed name to \"{}\"", PlayerLogInfo{*player}, g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
    }

    g_pGameRules->ClientUserInfoChanged( player, infobuffer );
}

void ServerDeactivate()
{
    // It's possible that the engine will call this function more times than is necessary
    //  Therefore, only run it one time for each call to ServerActivate
    if( !g_serveractive )
    {
        return;
    }

    g_serveractive = false;

    // Peform any shutdown operations here...
    //
}

void ServerActivate( edict_t* pEdictList, int edictCount, int clientMax )
{
    int i;
    CBaseEntity* pClass;

    // Every call to ServerActivate should be matched by a call to ServerDeactivate
    g_serveractive = true;

    g_Server.PreMapActivate();

    // Clients have not been initialized yet
    for( i = 0; i < edictCount; i++ )
    {
        if( 0 != pEdictList[i].free )
            continue;

        // Clients aren't necessarily initialized until ClientPutInServer()
        if( ( i > 0 && i <= clientMax ) || !pEdictList[i].pvPrivateData )
            continue;

        pClass = CBaseEntity::Instance( &pEdictList[i] );
        // Activate this entity if it's got a class & isn't dormant
        if( pClass && ( pClass->pev->flags & FL_DORMANT ) == 0 )
        {
            //-TODO Should we call Precache in here?
            pClass->Activate();
        }
        else
        {
            CBaseEntity::Logger->debug( "Can't instance {}", STRING( pEdictList[i].v.classname ) );
        }
    }

    g_Server.PostMapActivate();
}


/**
 *    @brief Called every frame before physics are run
 */
void PlayerPreThink( edict_t* pEntity )
{
    CBasePlayer* pPlayer = ToBasePlayer( pEntity );

    if( pPlayer )
        pPlayer->PreThink();
}

/**
 *    @brief Called every frame after physics are run
 */
void PlayerPostThink( edict_t* pEntity )
{
    CBasePlayer* pPlayer = ToBasePlayer( pEntity );

    if( pPlayer )
        pPlayer->PostThink();
}

void ParmsNewLevel()
{
}

void ParmsChangeLevel()
{
    // retrieve the pointer to the save data
    SAVERESTOREDATA* pSaveData = ( SAVERESTOREDATA* )gpGlobals->pSaveData;

    if( pSaveData )
        pSaveData->connectionCount = CChangeLevel::ChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame()
{
    g_Server.RunFrame();

    if( g_pGameRules )
        g_pGameRules->Think();

    g_GameMode->OnThink();

    g_Achievement.Save();

    if( g_fGameOver )
        return;

    g_ulFrameCount++;
}

void ClientPrecache()
{
    g_GameLogger->trace( "Precaching player assets" );

    // setup precaches always needed
    UTIL_PrecacheSound( "player/sprayer.wav" ); // spray paint sound for PreAlpha

    // UTIL_PrecacheSound("player/pl_jumpland2.wav");        // UNDONE: play 2x step sound

    UTIL_PrecacheSound( "player/pl_fallpain2.wav" );
    UTIL_PrecacheSound( "player/pl_fallpain3.wav" );

    UTIL_PrecacheSound( "player/pl_step1.wav" ); // walk on concrete
    UTIL_PrecacheSound( "player/pl_step2.wav" );
    UTIL_PrecacheSound( "player/pl_step3.wav" );
    UTIL_PrecacheSound( "player/pl_step4.wav" );

    UTIL_PrecacheSound( "common/npc_step1.wav" ); // NPC walk on concrete
    UTIL_PrecacheSound( "common/npc_step2.wav" );
    UTIL_PrecacheSound( "common/npc_step3.wav" );
    UTIL_PrecacheSound( "common/npc_step4.wav" );

    UTIL_PrecacheSound( "player/pl_metal1.wav" ); // walk on metal
    UTIL_PrecacheSound( "player/pl_metal2.wav" );
    UTIL_PrecacheSound( "player/pl_metal3.wav" );
    UTIL_PrecacheSound( "player/pl_metal4.wav" );

    UTIL_PrecacheSound( "player/pl_dirt1.wav" ); // walk on dirt
    UTIL_PrecacheSound( "player/pl_dirt2.wav" );
    UTIL_PrecacheSound( "player/pl_dirt3.wav" );
    UTIL_PrecacheSound( "player/pl_dirt4.wav" );

    UTIL_PrecacheSound( "player/pl_duct1.wav" ); // walk in duct
    UTIL_PrecacheSound( "player/pl_duct2.wav" );
    UTIL_PrecacheSound( "player/pl_duct3.wav" );
    UTIL_PrecacheSound( "player/pl_duct4.wav" );

    UTIL_PrecacheSound( "player/pl_grate1.wav" ); // walk on grate
    UTIL_PrecacheSound( "player/pl_grate2.wav" );
    UTIL_PrecacheSound( "player/pl_grate3.wav" );
    UTIL_PrecacheSound( "player/pl_grate4.wav" );

    UTIL_PrecacheSound( "player/pl_slosh1.wav" ); // walk in shallow water
    UTIL_PrecacheSound( "player/pl_slosh2.wav" );
    UTIL_PrecacheSound( "player/pl_slosh3.wav" );
    UTIL_PrecacheSound( "player/pl_slosh4.wav" );

    UTIL_PrecacheSound( "player/pl_tile1.wav" ); // walk on tile
    UTIL_PrecacheSound( "player/pl_tile2.wav" );
    UTIL_PrecacheSound( "player/pl_tile3.wav" );
    UTIL_PrecacheSound( "player/pl_tile4.wav" );
    UTIL_PrecacheSound( "player/pl_tile5.wav" );

    UTIL_PrecacheSound( "player/pl_swim1.wav" ); // breathe bubbles
    UTIL_PrecacheSound( "player/pl_swim2.wav" );
    UTIL_PrecacheSound( "player/pl_swim3.wav" );
    UTIL_PrecacheSound( "player/pl_swim4.wav" );

    UTIL_PrecacheSound( "player/pl_ladder1.wav" ); // climb ladder rung
    UTIL_PrecacheSound( "player/pl_ladder2.wav" );
    UTIL_PrecacheSound( "player/pl_ladder3.wav" );
    UTIL_PrecacheSound( "player/pl_ladder4.wav" );

    UTIL_PrecacheSound( "player/pl_wade1.wav" ); // wade in water
    UTIL_PrecacheSound( "player/pl_wade2.wav" );
    UTIL_PrecacheSound( "player/pl_wade3.wav" );
    UTIL_PrecacheSound( "player/pl_wade4.wav" );

    UTIL_PrecacheSound( "player/pl_snow1.wav" ); // walk on snow
    UTIL_PrecacheSound( "player/pl_snow2.wav" );
    UTIL_PrecacheSound( "player/pl_snow3.wav" );
    UTIL_PrecacheSound( "player/pl_snow4.wav" );

    UTIL_PrecacheSound( "debris/wood1.wav" ); // hit wood texture
    UTIL_PrecacheSound( "debris/wood2.wav" );
    UTIL_PrecacheSound( "debris/wood3.wav" );

    UTIL_PrecacheSound( "plats/train_use1.wav" ); // use a train

    UTIL_PrecacheSound( "buttons/spark5.wav" ); // hit computer texture
    UTIL_PrecacheSound( "buttons/spark6.wav" );
    UTIL_PrecacheSound( "debris/glass1.wav" );
    UTIL_PrecacheSound( "debris/glass2.wav" );
    UTIL_PrecacheSound( "debris/glass3.wav" );

    UTIL_PrecacheSound( SOUND_FLASHLIGHT_ON );
    UTIL_PrecacheSound( SOUND_FLASHLIGHT_OFF );

    UTIL_PrecacheSound( SOUND_NIGHTVISION_ON );
    UTIL_PrecacheSound( SOUND_NIGHTVISION_OFF );

    // player gib sounds
    UTIL_PrecacheSound( "common/bodysplat.wav" );

    // player pain sounds
    UTIL_PrecacheSound( "player/pl_pain2.wav" );
    UTIL_PrecacheSound( "player/pl_pain4.wav" );
    UTIL_PrecacheSound( "player/pl_pain5.wav" );
    UTIL_PrecacheSound( "player/pl_pain6.wav" );
    UTIL_PrecacheSound( "player/pl_pain7.wav" );

    UTIL_PrecacheModel( "models/player.mdl" );

    // hud sounds

    UTIL_PrecacheSound( "common/wpn_hudoff.wav" );
    UTIL_PrecacheSound( "common/wpn_hudon.wav" );
    UTIL_PrecacheSound( "common/wpn_moveselect.wav" );
    UTIL_PrecacheSound( "common/wpn_select.wav" );
    UTIL_PrecacheSound( "common/wpn_denyselect.wav" );


    // geiger sounds

    UTIL_PrecacheSound( "player/geiger6.wav" );
    UTIL_PrecacheSound( "player/geiger5.wav" );
    UTIL_PrecacheSound( "player/geiger4.wav" );
    UTIL_PrecacheSound( "player/geiger3.wav" );
    UTIL_PrecacheSound( "player/geiger2.wav" );
    UTIL_PrecacheSound( "player/geiger1.wav" );

    UTIL_PrecacheSound( "ctf/pow_big_jump.wav" );

    // for cheat_givemagazine
    UTIL_PrecacheSound( DefaultItemPickupSound );

    if( giPrecacheGrunt )
        UTIL_PrecacheOther( "monster_human_grunt" );

    UTIL_PrecacheModelDirect( "sprites/voiceicon.spr" );
}

/**
 *    @brief Returns the descriptive name of this .dll. E.g., <tt>Half-Life</tt>, or <tt>Team Fortress 2</tt>
 */
const char* GetGameDescription()
{
    if( g_GameMode.gamemode ) // this function may be called before the world has spawned, and the game rules initialized
        return g_GameMode->GetGameDescription();
    else
        return "Half-Life: Limitless Potential";
}

/**
 *    @brief Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
 */
void Sys_Error( const char* error_string )
{
    // Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/**
 *    @brief A new player customization has been registered on the server
 *    @details UNDONE: This only sets the # of frames of the spray can logo animation right now.
 */
void PlayerCustomization( edict_t* pEntity, customization_t* pCust )
{
    CBasePlayer* pPlayer = ToBasePlayer( pEntity );

    if( !pPlayer )
    {
        CBaseEntity::Logger->debug( "PlayerCustomization:  Couldn't get player!" );
        return;
    }

    if( !pCust )
    {
        CBaseEntity::Logger->debug( "PlayerCustomization:  nullptr customization!" );
        return;
    }

    switch ( pCust->resource.type )
    {
    case t_decal:
        pPlayer->SetCustomDecalFrames( pCust->nUserData2 ); // Second int is max # of frames.
        break;
    case t_sound:
    case t_skin:
    case t_model:
        // Ignore for now.
        break;
    default:
        CBaseEntity::Logger->debug( "PlayerCustomization:  Unknown customization type!" );
        break;
    }
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/**
 *    @brief A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
 *    view entity.
 *    If that's the case, then pViewEntity will be non-nullptr and will be used.
 *    Otherwise, the current entity's origin is used.
 *    Either is offset by the view_ofs to get the eye position.
 *
 *    From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.
 *    At this point, we could override the actual PAS or PVS values, or use a different origin.
 *
 *    NOTE: Do not cache the values of pas and pvs, as they depend on reusable memory in the engine,
 *    they are only good for this one frame
 */
void SetupVisibility( edict_t* pViewEntity, edict_t* pClient, unsigned char** pvs, unsigned char** pas )
{
    Vector org;
    edict_t* pView = pClient;

    // Find the client's PVS
    if( pViewEntity )
    {
        pView = pViewEntity;
    }

    if( ( pClient->v.flags & FL_PROXY ) != 0 )
    {
        *pvs = nullptr; // the spectator proxy sees
        *pas = nullptr; // and hears everything
        return;
    }

    org = pView->v.origin + pView->v.view_ofs;
    if( ( pView->v.flags & FL_DUCKING ) != 0 )
    {
        org = org + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
    }

    *pvs = ENGINE_SET_PVS( org );
    *pas = ENGINE_SET_PAS( org );
}

#include "entity_state.h"

/**
 *    @brief Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client,
 *    0 otherwise
 *    @param state the server maintained copy of the state info that is transmitted to the client
 *        a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
 *    @param e index of the entity that is being added to the update, if 1 is returned
 *    @param ent the entity that is being added to the update, if 1 is returned
 *    @param host is the player's edict of the player whom we are sending the update to
 *    @param hostflags 1 if the host has <tt>cl_lw</tt> enabled, 0 otherwise
 *    @param player 1 if the ent/e is a player and 0 otherwise
 *    @param pSet either the PAS or PVS that we previous set up.
 *        We can use it to ask the engine to filter the entity against the PAS or PVS.
 *        we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.
 *        Caching the value is valid in that case, but still only for the current frame
 */
int AddToFullPack( entity_state_t* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet )
{
    // Entities with an index greater than this will corrupt the client's heap because
    // the index is sent with only 11 bits of precision (2^11 == 2048).
    // So we don't send them, just like having too many entities would result
    // in the entity not being sent.
    if( e >= MAX_EDICTS )
    {
        return 0;
    }

    int i;

    auto entity = reinterpret_cast<CBaseEntity*>( GET_PRIVATE( ent ) );

    // don't send if flagged for NODRAW and it's not the host getting the message
    if( ( ent->v.effects & EF_NODRAW ) != 0 &&
        ( ent != host ) )
        return 0;

    // Ignore ents without valid / visible models
    if( 0 == ent->v.modelindex || !STRING( ent->v.model ) )
        return 0;

    // Don't send spectators to other players
    if( ( ent->v.flags & FL_SPECTATOR ) != 0 && ( ent != host ) )
    {
        return 0;
    }

    // Ignore if not the host and not touching a PVS/PAS leaf
    // If pSet is nullptr, then the test will always succeed and the entity will be added to the update
    if( ent != host )
    {
        if( !ENGINE_CHECK_VISIBILITY( ( const edict_t* )ent, pSet ) )
        {
            return 0;
        }
    }


    // Don't send entity to local client if the client says it's predicting the entity itself.
    if( ( ent->v.flags & FL_SKIPLOCALHOST ) != 0 )
    {
        if( ( hostflags & 1 ) != 0 && ( ent->v.owner == host ) )
            return 0;
    }

    if( 0 != host->v.groupinfo )
    {
        UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

        // Should always be set, of course
        if( 0 != ent->v.groupinfo )
        {
            if( g_groupop == GROUP_OP_AND )
            {
                if( ( ent->v.groupinfo & host->v.groupinfo ) == 0 )
                    return 0;
            }
            else if( g_groupop == GROUP_OP_NAND )
            {
                if( ( ent->v.groupinfo & host->v.groupinfo ) != 0 )
                    return 0;
            }
        }

        UTIL_UnsetGroupTrace();
    }

    memset( state, 0, sizeof( *state ) );

    // Assign index so we can track this entity from frame to frame and
    //  delta from it.
    state->number = e;
    state->entityType = ENTITY_NORMAL;

    // Flag custom entities.
    if( ( ent->v.flags & FL_CUSTOMENTITY ) != 0 )
    {
        state->entityType = ENTITY_BEAM;
    }

    //
    // Copy state data
    //

    // Round animtime to nearest millisecond
    state->animtime = (int)( 1000.0 * ent->v.animtime ) / 1000.0;

    memcpy( state->origin, ent->v.origin, 3 * sizeof(float) );
    memcpy( state->angles, ent->v.angles, 3 * sizeof(float) );
    memcpy( state->mins, ent->v.mins, 3 * sizeof(float) );
    memcpy( state->maxs, ent->v.maxs, 3 * sizeof(float) );

    memcpy( state->startpos, ent->v.startpos, 3 * sizeof(float) );
    memcpy( state->endpos, ent->v.endpos, 3 * sizeof(float) );

    state->impacttime = ent->v.impacttime;
    state->starttime = ent->v.starttime;

    state->modelindex = ent->v.modelindex;

    state->frame = ent->v.frame;

    state->skin = ent->v.skin;
    state->effects = ent->v.effects;

    // This non-player entity is being moved by the game .dll and not the physics simulation system
    //  make sure that we interpolate it's position on the client if it moves
    /*
    if (0 == player &&
        0 != ent->v.animtime &&
        ent->v.velocity[0] == 0 &&
        ent->v.velocity[1] == 0 &&
        ent->v.velocity[2] == 0)
    {
        state->eflags |= EFLAG_SLERP;
    }
    */

    if( ( ent->v.flags & FL_FLY ) != 0 )
    {
        state->eflags |= EFLAG_SLERP;
    }
    else
    {
        state->eflags &= ~EFLAG_SLERP;
    }

    state->eflags |= entity->m_EFlags;

    state->scale = ent->v.scale;
    state->solid = ent->v.solid;
    state->colormap = ent->v.colormap;

    state->movetype = ent->v.movetype;
    state->sequence = ent->v.sequence;
    state->framerate = ent->v.framerate;
    state->body = ent->v.body;

    for( i = 0; i < 4; i++ )
    {
        state->controller[i] = ent->v.controller[i];
    }

    for( i = 0; i < 2; i++ )
    {
        state->blending[i] = ent->v.blending[i];
    }

    state->rendermode = ent->v.rendermode;
    state->renderamt = ent->v.renderamt;
    state->renderfx = ent->v.renderfx;
    state->rendercolor.r = ent->v.rendercolor.x;
    state->rendercolor.g = ent->v.rendercolor.y;
    state->rendercolor.b = ent->v.rendercolor.z;

    state->aiment = 0;
    if( ent->v.aiment )
    {
        state->aiment = ENTINDEX( ent->v.aiment );
    }

    state->owner = 0;
    if( ent->v.owner )
    {
        int owner = ENTINDEX( ent->v.owner );

        // Only care if owned by a player
        if( owner >= 1 && owner <= gpGlobals->maxClients )
        {
            state->owner = owner;
        }
    }

    // HACK:  Somewhat...
    // Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
    if( 0 == player )
    {
        state->playerclass = ent->v.playerclass;
    }

    // Special stuff for players only
    if( 0 != player )
    {
        memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof(float) );

        state->weaponmodel = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
        state->gaitsequence = ent->v.gaitsequence;
        state->spectator = ent->v.flags & FL_SPECTATOR;
        state->friction = ent->v.friction;

        state->gravity = ent->v.gravity;
        //        state->team            = ent->v.team;
        //
        state->usehull = ( ent->v.flags & FL_DUCKING ) != 0 ? 1 : 0;
        state->health = ent->v.health;

        auto pHost = static_cast<CBasePlayer*>( CBaseEntity::Instance( host ) );

        if( host != ent )
        {
            // Remove the night vision illumination effect so other players don't see it
            state->effects &= ~EF_BRIGHTLIGHT;

            // Set nocliping if players touching each others
            if( !g_cfg.GetValue<bool>( "player_collision"sv, true )
            && entity->IsPlayer()
            && pHost->IsAlive()
            && pHost->Intersects( entity )
            && host->v.absmin.z < ent->v.absmax.z /* Allow to "Boost" */
            ) {
                state->solid = SOLID_NOT;
                // -TODO Check for relationship and perphabs allow monsters too?
            }
        }
    }

    return 1;
}

/**
 *    @brief Creates baselines used for network encoding,
 *    especially for player data since players are not spawned until connect time.
 */
void CreateBaseline( int player, int eindex, entity_state_t* baseline, edict_t* entity, int playermodelindex, Vector* player_mins, Vector* player_maxs )
{
    baseline->origin = entity->v.origin;
    baseline->angles = entity->v.angles;
    baseline->frame = entity->v.frame;
    baseline->skin = (short)entity->v.skin;

    // render information
    baseline->rendermode = ( byte )entity->v.rendermode;
    baseline->renderamt = ( byte )entity->v.renderamt;
    baseline->rendercolor.r = ( byte )entity->v.rendercolor.x;
    baseline->rendercolor.g = ( byte )entity->v.rendercolor.y;
    baseline->rendercolor.b = ( byte )entity->v.rendercolor.z;
    baseline->renderfx = ( byte )entity->v.renderfx;

    if( 0 != player )
    {
        baseline->mins = *player_mins;
        baseline->maxs = *player_maxs;

        baseline->colormap = eindex;
        baseline->modelindex = playermodelindex;
        baseline->friction = 1.0;
        baseline->movetype = MOVETYPE_WALK;

        baseline->scale = entity->v.scale;
        baseline->solid = SOLID_SLIDEBOX;
        baseline->framerate = 1.0;
        baseline->gravity = 1.0;
    }
    else
    {
        baseline->mins = entity->v.mins;
        baseline->maxs = entity->v.maxs;

        baseline->colormap = 0;
        baseline->modelindex = entity->v.modelindex; // SV_ModelIndex(pr_strings + entity->v.model);
        baseline->movetype = entity->v.movetype;

        baseline->scale = entity->v.scale;
        baseline->solid = entity->v.solid;
        baseline->framerate = entity->v.framerate;
        baseline->gravity = entity->v.gravity;
    }
}

struct entity_field_alias_t
{
    char name[32];
    int field;
};

#define FIELD_ORIGIN0 0
#define FIELD_ORIGIN1 1
#define FIELD_ORIGIN2 2
#define FIELD_ANGLES0 3
#define FIELD_ANGLES1 4
#define FIELD_ANGLES2 5

static entity_field_alias_t entity_field_alias[] =
    {
        {"origin[0]", 0},
        {"origin[1]", 0},
        {"origin[2]", 0},
        {"angles[0]", 0},
        {"angles[1]", 0},
        {"angles[2]", 0},
};

void Entity_FieldInit( delta_t* pFields )
{
    entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN0].name );
    entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN1].name );
    entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ORIGIN2].name );
    entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES0].name );
    entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES1].name );
    entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD( pFields, entity_field_alias[FIELD_ANGLES2].name );
}

/**
 *    @brief Callback for sending entity_state_t info over network.
 *    FIXME: Move to script
 */
void Entity_Encode( delta_t* pFields, const unsigned char* from, const unsigned char* to )
{
    entity_state_t *f, *t;
    static bool initialized = false;

    if( !initialized )
    {
        Entity_FieldInit( pFields );
        initialized = true;
    }

    f = ( entity_state_t* )from;
    t = ( entity_state_t* )to;

    // Never send origin to local player, it's sent with more resolution in clientdata_t structure
    const bool localplayer = ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
    if( localplayer )
    {
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }

    if( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
    {
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );

        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ANGLES2].field );
    }

    if( ( t->movetype == MOVETYPE_FOLLOW ) &&
        ( t->aiment != 0 ) )
    {
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }
    else if( t->aiment != f->aiment )
    {
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }
}

static entity_field_alias_t player_field_alias[] =
    {
        {"origin[0]", 0},
        {"origin[1]", 0},
        {"origin[2]", 0},
};

void Player_FieldInit( delta_t* pFields )
{
    player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN0].name );
    player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN1].name );
    player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, player_field_alias[FIELD_ORIGIN2].name );
}

/**
 *    @brief Callback for sending entity_state_t for players info over network.
 */
void Player_Encode( delta_t* pFields, const unsigned char* from, const unsigned char* to )
{
    entity_state_t *f, *t;
    static bool initialized = false;

    if( !initialized )
    {
        Player_FieldInit( pFields );
        initialized = true;
    }

    f = ( entity_state_t* )from;
    t = ( entity_state_t* )to;

    // Never send origin to local player, it's sent with more resolution in clientdata_t structure
    const bool localplayer = ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
    if( localplayer )
    {
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }

    if( ( t->movetype == MOVETYPE_FOLLOW ) &&
        ( t->aiment != 0 ) )
    {
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }
    else if( t->aiment != f->aiment )
    {
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN0].field );
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN1].field );
        DELTA_SETBYINDEX( pFields, entity_field_alias[FIELD_ORIGIN2].field );
    }
}

#define CUSTOMFIELD_ORIGIN0 0
#define CUSTOMFIELD_ORIGIN1 1
#define CUSTOMFIELD_ORIGIN2 2
#define CUSTOMFIELD_ANGLES0 3
#define CUSTOMFIELD_ANGLES1 4
#define CUSTOMFIELD_ANGLES2 5
#define CUSTOMFIELD_SKIN 6
#define CUSTOMFIELD_SEQUENCE 7
#define CUSTOMFIELD_ANIMTIME 8

entity_field_alias_t custom_entity_field_alias[] =
    {
        {"origin[0]", 0},
        {"origin[1]", 0},
        {"origin[2]", 0},
        {"angles[0]", 0},
        {"angles[1]", 0},
        {"angles[2]", 0},
        {"skin", 0},
        {"sequence", 0},
        {"animtime", 0},
};

void Custom_Entity_FieldInit( delta_t* pFields )
{
    custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name );
    custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name );
    custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name );
    custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name );
    custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name );
    custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name );
    custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name );
    custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name );
    custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD( pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name );
}

/**
 *    @brief Callback for sending entity_state_t info ( for custom entities ) over network.
 *    FIXME: Move to script
 */
void Custom_Encode( delta_t* pFields, const unsigned char* from, const unsigned char* to )
{
    entity_state_t *f, *t;
    int beamType;
    static bool initialized = false;

    if( !initialized )
    {
        Custom_Entity_FieldInit( pFields );
        initialized = true;
    }

    f = ( entity_state_t* )from;
    t = ( entity_state_t* )to;

    beamType = t->rendermode & 0x0f;

    if( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
    {
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field );
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field );
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field );
    }

    if( beamType != BEAM_POINTS )
    {
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field );
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field );
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field );
    }

    if( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
    {
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field );
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field );
    }

    // animtime is compared by rounding first
    // see if we really shouldn't actually send it
    if( (int)f->animtime == (int)t->animtime )
    {
        DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field );
    }
}

/**
 *    @brief Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
 *    @details See the mod's @c delta.lst configuration file for relevant encoding settings.
 */
void RegisterEncoders()
{
    DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
    DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
    DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

int GetWeaponData( edict_t* player, weapon_data_t* info )
{
    memset( info, 0, MAX_WEAPONS * sizeof( weapon_data_t ) );

#if defined( CLIENT_WEAPONS )
    int i;
    weapon_data_t* item;
    auto pl = ToBasePlayer( player );

    if( !pl )
        return 1;

    pl->m_LastWeaponDataUpdateTime = gpGlobals->time;

    // go through all of the weapons and make a list of the ones to pack
    for( i = 0; i < MAX_WEAPON_SLOTS; i++ )
    {
        if( pl->m_rgpPlayerWeapons[i] )
        {
            // there's a weapon here. Should I pack it?
            CBasePlayerWeapon* gun = pl->m_rgpPlayerWeapons[i];

            while( gun )
            {
                if( gun->UseDecrement() )
                {
                    // Get The ID.
                    WeaponInfo weaponInfo;
                    gun->GetWeaponInfo( weaponInfo );

                    if( weaponInfo.Id >= 0 && weaponInfo.Id < MAX_WEAPONS )
                    {
                        item = &info[weaponInfo.Id];

                        item->m_iId = weaponInfo.Id;
                        item->m_iClip = gun->m_iClip;

                        item->m_flTimeWeaponIdle = std::max( gun->m_flTimeWeaponIdle, -0.001f );
                        item->m_flNextPrimaryAttack = std::max( gun->m_flNextPrimaryAttack, -0.001f );
                        item->m_flNextSecondaryAttack = std::max( gun->m_flNextSecondaryAttack, -0.001f );
                        item->m_fInReload = static_cast<int>( gun->m_fInReload );
                        item->m_fInSpecialReload = gun->m_fInSpecialReload;
                        item->fuser1 = std::max( gun->pev->fuser1, -0.001f );

                        gun->GetWeaponData( *item );

                        // item->m_flPumpTime = std::max(gun->m_flPumpTime, -0.001f);
                    }
                }
                gun = gun->m_pNext;
            }
        }
    }
#endif
    return 1;
}

/**
 *    @brief Data sent to current client only
 *    @param cd zeroed out by engine before call.
 */
void UpdateClientData( const edict_t* ent, int sendweapons, clientdata_t* cd )
{
    g_Server.OnUpdateClientData();

    if( !ent )
        return;

    auto pl = ToBasePlayer( const_cast<edict_t*>( ent ) );

    if( !pl )
    {
        return;
    }

    CBaseEntity* viewEntity = pl;
    CBasePlayer* plOrg = pl;

    // if user is spectating different player in First person, override some vars
    if( pl && pl->pev->iuser1 == OBS_IN_EYE )
    {
        if( pl->m_hObserverTarget )
        {
            viewEntity = pl->m_hObserverTarget;
            pl = ToBasePlayer( viewEntity );
        }
    }

    cd->flags = viewEntity->pev->flags;
    cd->health = viewEntity->pev->health;

    cd->viewmodel = MODEL_INDEX( STRING( viewEntity->pev->viewmodel ) );

    cd->waterlevel = viewEntity->pev->waterlevel;
    cd->watertype = viewEntity->pev->watertype;

    // Vectors
    cd->origin = viewEntity->pev->origin;
    cd->velocity = viewEntity->pev->velocity;
    cd->view_ofs = viewEntity->pev->view_ofs;
    cd->punchangle = viewEntity->pev->punchangle;

    cd->bInDuck = viewEntity->pev->bInDuck;
    cd->flTimeStepSound = viewEntity->pev->flTimeStepSound;
    cd->flDuckTime = viewEntity->pev->flDuckTime;
    cd->flSwimTime = viewEntity->pev->flSwimTime;
    cd->waterjumptime = viewEntity->pev->teleport_time;

    strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

    cd->maxspeed = viewEntity->pev->maxspeed;
    cd->weaponanim = viewEntity->pev->weaponanim;

    cd->pushmsec = viewEntity->pev->pushmsec;

    // Spectator mode
    // don't use spec vars from chased player
    cd->iuser1 = plOrg->pev->iuser1;
    cd->iuser2 = plOrg->pev->iuser2;

    if( pl )
    {
        cd->weapons = pl->m_HudFlags;
        cd->fov = pl->m_iFOV;
        cd->iuser4 = pl->m_iItems;
    }
    else
    {
        cd->weapons = 0;            // Non-players don't have hud flags.
        cd->fov = plOrg->m_iFOV;    // Use actual player FOV if target is not a player.
        cd->iuser4 = CTFItem::None; // Non-players don't have items.
    }

#if defined( CLIENT_WEAPONS )
    if( 0 != sendweapons )
    {
        if( pl )
        {
            cd->m_flNextAttack = pl->m_flNextAttack;
            cd->fuser2 = pl->m_flNextAmmoBurn;
            cd->fuser3 = pl->m_flAmmoStartCharge;
            cd->vuser1.x = pl->GetAmmoCount( "9mm" );
            cd->vuser1.y = pl->GetAmmoCount( "357" );
            cd->vuser1.z = pl->GetAmmoCount( "ARgrenades" );
            cd->ammo_nails = pl->GetAmmoCount( "bolts" );
            cd->ammo_shells = pl->GetAmmoCount( "buckshot" );
            cd->ammo_rockets = pl->GetAmmoCount( "rockets" );
            cd->ammo_cells = pl->GetAmmoCount( "uranium" );
            cd->vuser2.x = pl->GetAmmoCount( "Hornets" );
            cd->vuser2.y = pl->GetAmmoCount( "spores" );
            cd->vuser2.z = pl->GetAmmoCount( "762" );

            if( pl->m_pActiveWeapon )
            {
                CBasePlayerWeapon* gun = pl->m_pActiveWeapon;
                if( gun->UseDecrement() )
                {
                    WeaponInfo weaponInfo;
                    gun->GetWeaponInfo( weaponInfo );

                    cd->m_iId = weaponInfo.Id;

                    cd->vuser3.z = gun->m_iSecondaryAmmoType;
                    cd->vuser4.x = gun->m_iPrimaryAmmoType;
                    cd->vuser4.y = pl->GetAmmoCountByIndex( gun->m_iPrimaryAmmoType );
                    cd->vuser4.z = pl->GetAmmoCountByIndex( gun->m_iSecondaryAmmoType );
                }
            }
        }
    }
#endif
}

/**
 *    @brief We're about to run this usercmd for the specified player. We can set up groupinfo and masking here, etc.
 *    This is the time to examine the usercmd for anything extra. This call happens even if think does not.
 */
void CmdStart( const edict_t* player, const usercmd_t* cmd, unsigned int random_seed )
{
    auto pl = ToBasePlayer( const_cast<edict_t*>( player ) );

    if( !pl )
        return;

    if( pl->pev->groupinfo != 0 )
    {
        UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
    }

    pl->random_seed = random_seed;
}

/**
 *    @brief Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
 */
void CmdEnd( const edict_t* player )
{
    auto pl = ToBasePlayer( const_cast<edict_t*>( player ) );

    if( !pl )
        return;
    if( pl->pev->groupinfo != 0 )
    {
        UTIL_UnsetGroupTrace();
    }
}

/**
 *    @brief Return 1 if the packet is valid.
 *    @param net_from Address of the sender.
 *    @param args Argument string.
 *    @param response_buffer Buffer to write a response message to.
 *    @param[in,out] response_buffer_size Initially holds the maximum size of @p response_buffer.
 *        Set to size of response message or 0 if sending no response.
 */
int ConnectionlessPacket( const netadr_t* net_from, const char* args, char* response_buffer, int* response_buffer_size )
{
    // Parse stuff from args
    // int max_buffer_size = *response_buffer_size;

    // Zero it out since we aren't going to respond.
    // If we wanted to response, we'd write data into response_buffer
    *response_buffer_size = 0;

    // Since we don't listen for anything here, just respond that it's a bogus message
    // If we didn't reject the message, we'd return 1 for success instead.
    return 0;
}

/**
 *    @brief Engine calls this to enumerate player collision hulls, for prediction.
 *    @return 0 if the hullnumber doesn't exist, 1 otherwise.
 */
int GetHullBounds( int hullnumber, float* mins, float* maxs )
{
    return static_cast<int>( PM_GetHullBounds( hullnumber, mins, maxs ) );
}

/**
 *    @brief Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
 *    to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
 */
void CreateInstancedBaselines()
{
    // int iret = 0;
    // entity_state_t state;

    // memset(&state, 0, sizeof(state));

    // Create any additional baselines here for things like grendates, etc.
    // iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

    // Destroy objects.
    // UTIL_Remove( pc );
}

/**
 *    @brief One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 *    @return 0 to allow the client to continue, 1 to force immediate disconnection
 *        ( with an optional disconnect message of up to 256 characters )
 */
int InconsistentFile( const edict_t* player, const char* filename, char* disconnect_message )
{
    // Server doesn't care?
    if( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
        return 0;

    // Default behavior is to kick the player
    sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

    // Kick now with specified disconnect message.
    return 1;
}

/**
 *    @brief The game .dll should return 1 if lag compensation should be allowed ( must also set the sv_unlag cvar ).
 *    @details Most games right now should return 0,
 *    until client-side weapon prediction code is written and tested for them
 *    ( note you can predict weapons, but not do lag compensation, too, if you want ).
 */
int AllowLagCompensation()
{
    return 1;
}

/*
================================
ShouldCollide

  Called when the engine believes two entities are about to collide. Return 0 if you
  want the two entities to just pass through each other without colliding or calling the
  touch function.
================================
*/
int ShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
#if 0
    if( !FNullEnt( pentTouched ) && !FNullEnt( pentOther ) )
    {
        auto touched = CBaseEntity::Instance( pentTouched );
        auto toucher = CBaseEntity::Instance( pentOther );

        if( touched != nullptr && toucher != nullptr )
        {
//-TODO Need to check both players are efectivelly allies
            if( !g_cfg.GetValue<bool>( "player_collision"sv, true ) )
            {
                auto IsPlayerWithProjectile = []( CBaseEntity* a, CBaseEntity* b ) -> bool
                {
                    if( a->IsPlayer() && !FNullEnt( b->pev->owner ) )
                    {
                        if( auto c = CBaseEntity::Instance( b->pev->owner ); c != nullptr )
                        {
                            return ( c->IsPlayer() );
                        }
                    }
                    return false;
                };
    
                if( IsPlayerWithProjectile( touched, toucher ) || IsPlayerWithProjectile( toucher, touched ) )
                    return 0;
            }
        }
    }
#endif

    return 1;
}
