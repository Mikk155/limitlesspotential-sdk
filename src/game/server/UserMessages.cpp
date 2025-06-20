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

#include "cbase.h"
#include "shake.h"
#include "UserMessages.h"

void LinkUserMessages()
{
    // Already taken care of?
    if( 0 != gmsgCurWeapon )
    {
        return;
    }

    gmsgCurWeapon = REG_USER_MSG( "CurWeapon", 3 );
    gmsgGeigerRange = REG_USER_MSG( "Geiger", 1 );
    gmsgFlashlight = REG_USER_MSG( "Flashlight", 3 );
    gmsgFlashBattery = REG_USER_MSG( "FlashBat", 1 );
    gmsgHealth = REG_USER_MSG( "Health", 2 );
    gmsgDamage = REG_USER_MSG( "Damage", 12 );
    gmsgBattery = REG_USER_MSG( "Battery", 2 );
    gmsgTrain = REG_USER_MSG( "Train", 1 );
    // gmsgHudText = REG_USER_MSG( "HudTextPro", -1 );
    gmsgHudText = REG_USER_MSG( "HudText", -1 ); // we don't use the message but 3rd party addons may!
    gmsgSayText = REG_USER_MSG( "SayText", -1 );
    gmsgTextMsg = REG_USER_MSG( "TextMsg", -1 );
    gmsgResetHUD = REG_USER_MSG( "ResetHUD", 1 ); // called every respawn
    gmsgInitHUD = REG_USER_MSG( "InitHUD", 0 );    // called every time a new player joins the server
    gmsgShowGameTitle = REG_USER_MSG( "GameTitle", -1 );
    gmsgDeathMsg = REG_USER_MSG( "DeathMsg", -1 );
    gmsgScoreInfo = REG_USER_MSG( "ScoreInfo", 9 );
    gmsgTeamInfo = REG_USER_MSG( "TeamInfo", -1 );   // sets the name of a player's team
    gmsgTeamScore = REG_USER_MSG( "TeamScore", -1 ); // sets the score of a team on the scoreboard
    gmsgMOTD = REG_USER_MSG( "MOTD", -1 );
    gmsgServerName = REG_USER_MSG( "ServerName", -1 );
    gmsgAmmoPickup = REG_USER_MSG( "AmmoPickup", 2 );
    gmsgWeapPickup = REG_USER_MSG( "WeapPickup", 1 );
    gmsgItemPickup = REG_USER_MSG( "ItemPickup", -1 );
    gmsgHideWeapon = REG_USER_MSG( "HideWeapon", 1 );
    gmsgSetFOV = REG_USER_MSG( "SetFOV", 1 );
    gmsgShowMenu = REG_USER_MSG( "ShowMenu", -1 );
    gmsgShake = REG_USER_MSG( "ScreenShake", sizeof( ScreenShake ) );
    gmsgFade = REG_USER_MSG( "ScreenFade", sizeof( ScreenFade ) );
    gmsgAmmoX = REG_USER_MSG( "AmmoX", 2 );
    gmsgTeamNames = REG_USER_MSG( "TeamNames", -1 );

    gmsgStatusText = REG_USER_MSG( "StatusText", -1 );
    gmsgStatusValue = REG_USER_MSG( "StatusValue", 3 );

    gmsgSpectator = g_engfuncs.pfnRegUserMsg( "Spectator", 2 );
    gmsgStatusIcon = g_engfuncs.pfnRegUserMsg( "StatusIcon", -1 );
    gmsgPlayerBrowse = g_engfuncs.pfnRegUserMsg( "PlyrBrowse", -1 );
    gmsgFlagIcon = g_engfuncs.pfnRegUserMsg( "FlagIcon", -1 );
    gmsgFlagTimer = g_engfuncs.pfnRegUserMsg( "FlagTimer", -1 );
    gmsgPlayerIcon = g_engfuncs.pfnRegUserMsg( "PlayerIcon", -1 );
    gmsgVGUIMenu = g_engfuncs.pfnRegUserMsg( "VGUIMenu", -1 );
    gmsgAllowSpec = g_engfuncs.pfnRegUserMsg( "AllowSpec", 1 );
    gmsgSetMenuTeam = g_engfuncs.pfnRegUserMsg( "SetMenuTeam", 1 );
    gmsgCTFScore = g_engfuncs.pfnRegUserMsg( "CTFScore", 2 );
    gmsgStatsInfo = g_engfuncs.pfnRegUserMsg( "StatsInfo", -1 );
    gmsgStatsPlayer = g_engfuncs.pfnRegUserMsg( "StatsPlayer", 31 );
    gmsgTeamFull = g_engfuncs.pfnRegUserMsg( "TeamFull", 1 );
    gmsgCustomIcon = g_engfuncs.pfnRegUserMsg( "CustomIcon", -1 );
    gmsgWeapons = REG_USER_MSG( "Weapons", 8 );

    gmsgEntityInfo = REG_USER_MSG( "EntityInfo", -1 );
    gmsgEmitSound = REG_USER_MSG( "EmitSound", -1 );
    gmsgTempEntity = REG_USER_MSG( "TempEntity", -1 );
    gmsgConfigVars = REG_USER_MSG( "ConfigVars", -1 );
    gmsgGameMode = REG_USER_MSG( "GameMode", -1 );

    gmsgTgtLaser = REG_USER_MSG( "TgtLaser", -1 );

    gmsgFog = REG_USER_MSG( "Fog", 14 );

    gmsgClientGibs = REG_USER_MSG( "ClientGibs", 18 );

    gmsgDLight = REG_USER_MSG( "TempLight", 18 );

    gmsgSendSteamID = REG_USER_MSG( "SteamID", 8 );
}
