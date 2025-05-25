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
#include "CHalfLifeRules.h"
#include "items.h"
#include "UserMessages.h"
#include "items/CBaseItem.h"

void CHalfLifeRules::Think()
{
}

bool CHalfLifeRules::GetNextBestWeapon( CBasePlayer* pPlayer, CBasePlayerWeapon* pCurrentWeapon, bool alwaysSearch )
{
    // If this is an exhaustible weapon and it's out of ammo, always try to switch even in singleplayer.
    if( alwaysSearch || ( ( pCurrentWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE ) != 0 &&
                            pCurrentWeapon->PrimaryAmmoIndex() != -1 &&
                            pPlayer->GetAmmoCountByIndex( pCurrentWeapon->PrimaryAmmoIndex() ) == 0 ) )
    {
        return CGameRules::GetNextBestWeapon( pPlayer, pCurrentWeapon );
    }

    return false;
}

bool CHalfLifeRules::ClientConnected( edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128] )
{
    return true;
}

void CHalfLifeRules::InitHUD( CBasePlayer* pl )
{
}

void CHalfLifeRules::ClientDisconnected( edict_t* pClient )
{
}

bool CHalfLifeRules::AllowAutoTargetCrosshair()
{
    return ( g_cfg.GetSkillLevel() == SkillLevel::Easy );
}

bool CHalfLifeRules::FPlayerCanRespawn( CBasePlayer* pPlayer )
{
    return true;
}

float CHalfLifeRules::FlPlayerSpawnTime( CBasePlayer* pPlayer )
{
    return gpGlobals->time; // now!
}

int CHalfLifeRules::IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled )
{
    return 1;
}

void CHalfLifeRules::PlayerKilled( CBasePlayer* pVictim, CBaseEntity* pKiller, CBaseEntity* inflictor )
{
}

void CHalfLifeRules::DeathNotice( CBasePlayer* pVictim, CBaseEntity* pKiller, CBaseEntity* inflictor )
{
}

void CHalfLifeRules::PlayerGotItem( CBasePlayer* player, CBaseItem* item )
{
}

bool CHalfLifeRules::IsAllowedToSpawn( CBaseEntity* pEntity )
{
    return true;
}

int CHalfLifeRules::DeadPlayerWeapons( CBasePlayer* pPlayer )
{
    return GR_PLR_DROP_GUN_NO;
}

int CHalfLifeRules::DeadPlayerAmmo( CBasePlayer* pPlayer )
{
    return GR_PLR_DROP_AMMO_NO;
}

int CHalfLifeRules::PlayerRelationship( CBasePlayer* pPlayer, CBaseEntity* pTarget )
{
    // why would a single player in half life need this?
    return GR_NOTTEAMMATE;
}
