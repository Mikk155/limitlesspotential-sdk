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
#include "pm_shared.h"
#include "UserMessages.h"
#include "CTFSpawn.h"

const char* const sTeamSpawnNames[] =
{
    "ctfs0",
    "ctfs1",
    "ctfs2",
};

LINK_ENTITY_TO_CLASS( info_ctfspawn, CTFSpawn );

bool CTFSpawn::CanPlayerSpawn( CBasePlayer* player )
{
    if( !BaseClass::CanPlayerSpawn( player ) )
        return false;

    if( !FStringNull( pev->targetname ) && STRING( pev->targetname ) )
        return m_fState;

        return true;
}

void CTFSpawn::SpawnPlayer( CBasePlayer* player )
{
    BaseClass::SpawnPlayer( player );

    if( player->m_iTeamNum == CTFTeam::None )
    {
        player->pev->effects |= EF_NODRAW;
        player->pev->iuser1 = OBS_ROAMING;
        player->pev->solid = SOLID_NOT;
        player->pev->movetype = MOVETYPE_NOCLIP;
        player->pev->takedamage = DAMAGE_NO;
        player->m_iHideHUD = HIDEHUD_WEAPONS | HIDEHUD_HEALTH;
        player->m_afPhysicsFlags |= PFLAG_OBSERVER;
        player->pev->flags |= FL_SPECTATOR;

        MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
        WRITE_BYTE( player->entindex() );
        WRITE_BYTE( 1 );
        MESSAGE_END();

        MESSAGE_BEGIN( MSG_ONE, gmsgTeamFull, nullptr, player );
        WRITE_BYTE( 0 );
        MESSAGE_END();

        player->m_pGoalEnt = nullptr;

        player->m_iCurrentMenu = player->m_iNewTeamNum > CTFTeam::None ? MENU_DEFAULT : MENU_CLASS;

        player->Player_Menu();
    }

}

bool CTFSpawn::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( "team_no", pkvd->szKeyName ) )
    {
        team_no = static_cast<CTFTeam>( atoi( pkvd->szValue ) );
        return true;
    }

    return BaseClass::KeyValue( pkvd );
}

bool CTFSpawn::Spawn()
{
    if( team_no < CTFTeam::None || team_no > CTFTeam::OpposingForce )
    {
        Logger->debug( "Teamspawnpoint with an invalid team_no of {}", static_cast<int>( team_no ) );
        return false;
    }

    m_sMaster = pev->classname;

    // Change the classname to the owning team's spawn name
    pev->classname = MAKE_STRING( sTeamSpawnNames[static_cast<int>( team_no )] );
    m_fState = true;

    return true;
}
