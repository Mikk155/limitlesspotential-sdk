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
#include "CLaserSpot.h"

BEGIN_DATAMAP( CLaserSpot )
    DEFINE_FUNCTION( Revive ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( laser_spot, CLaserSpot );

CLaserSpot* CLaserSpot::CreateSpot()
{
    CLaserSpot* pSpot = g_EntityDictionary->Create<CLaserSpot>( "laser_spot" );
    pSpot->Spawn();

    return pSpot;
}

SpawnAction CLaserSpot::Spawn()
{
    Precache();
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_NOT;

    pev->rendermode = kRenderGlow;
    pev->renderfx = kRenderFxNoDissipation;
    pev->renderamt = 255;

    SetModel( "sprites/laserdot.spr" );
    SetOrigin( pev->origin );

    return SpawnAction::Spawn;
}

void CLaserSpot::Suspend( float flSuspendTime )
{
    pev->effects |= EF_NODRAW;

    SetThink( &CLaserSpot::Revive );
    pev->nextthink = gpGlobals->time + flSuspendTime;
}

void CLaserSpot::Revive()
{
    pev->effects &= ~EF_NODRAW;

    SetThink( nullptr );
}

void CLaserSpot::Precache()
{
    PrecacheModel( "sprites/laserdot.spr" );
}
