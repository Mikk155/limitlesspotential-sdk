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
#include "ctf_items.h"
#include "CHalfLifeCTFplay.h"
#include "UserMessages.h"

const auto SF_ITEMCTF_RANDOM_SPAWN = 1 << 2;
const auto SF_ITEMCTF_IGNORE_TEAM = 1 << 3;

BEGIN_DATAMAP( CItemCTF )
    DEFINE_FUNCTION( DropPreThink ),
    DEFINE_FUNCTION( DropThink ),
    DEFINE_FUNCTION( CarryThink ),
    DEFINE_FUNCTION( ItemTouch ),
END_DATAMAP();

CItemSpawnCTF* CItemCTF::m_pLastSpawn = nullptr;

bool CItemCTF::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( "team_no", pkvd->szKeyName ) )
    {
        team_no = static_cast<CTFTeam>( atoi( pkvd->szValue ) );
        return true;
    }

    // TODO: should invoke base class KeyValue here
    return false;
}

void CItemCTF::Precache()
{
    if( !FStringNull( pev->model ) )
        PrecacheModel( STRING( pev->model ) );

    PrecacheSound( "ctf/itemthrow.wav" );
    PrecacheSound( "items/ammopickup1.wav" );
}

SpawnAction CItemCTF::Spawn()
{
    pev->movetype = MOVETYPE_TOSS;
    pev->solid = SOLID_TRIGGER;

    SetOrigin( pev->origin );

    SetSize( {-16, -16, 0}, {16, 16, 48} );

    SetTouch( &CItemCTF::ItemTouch );

    if( DROP_TO_FLOOR( edict() ) == 0 )
    {
        CBaseEntity::Logger->error( "Item {} fell out of level at {}", STRING( pev->classname ), pev->origin.MakeString() );
        return SpawnAction::DelayRemove;
    }

    if( !FStringNull( pev->model ) )
    {
        SetModel( STRING( pev->model ) );

        pev->sequence = LookupSequence( "idle" );

        if( pev->sequence != -1 )
        {
            ResetSequenceInfo();
            pev->frame = 0;
        }
    }

    if( ( pev->spawnflags & SF_ITEMCTF_RANDOM_SPAWN ) != 0 )
    {
        SetThink( &CItemCTF::DropThink );
        pev->nextthink = gpGlobals->time + 0.1;
    }

    m_iLastTouched = 0;
    m_flPickupTime = 0;

    // TODO: already done above
    SetTouch( &CItemCTF::ItemTouch );

    m_flNextTouchTime = 0;
    m_pszItemName = "";

    return SpawnAction::Spawn;
}

void CItemCTF::DropPreThink()
{
    const auto contents = UTIL_PointContents( pev->origin );

    if( contents == CONTENTS_SLIME || contents == CONTENTS_LAVA || contents == CONTENTS_SOLID || contents == CONTENTS_SKY )
    {
        DropThink();
    }
    else
    {
        SetThink( &CItemCTF::DropThink );

        pev->nextthink = std::max( 0.f, g_flPowerupRespawnTime - 5 ) + gpGlobals->time;
    }
}

void CItemCTF::DropThink()
{
    SetThink( nullptr );

    pev->origin = pev->oldorigin;

    int nTested = 0;
    CItemSpawnCTF* spawn = nullptr;

    if( ( pev->spawnflags & SF_ITEMCTF_IGNORE_TEAM ) == 0 )
    {
        int iLosingTeam, iScoreDiff;
        GetLosingTeam( iLosingTeam, iScoreDiff );

        for( int i = 0; i < 2; ++i )
        {
            if( i == 0 )
                spawn = m_pLastSpawn;
            else
                spawn = nullptr;

            while( ( spawn = ( CItemSpawnCTF* )UTIL_FindEntityByClassname( spawn, "info_ctfspawn_powerup" ) ) != nullptr )
            {
                // Assume occupied until proven otherwise.
                bool nOccupied = true;

                if( iScoreDiff == 0 ||
                    ( iScoreDiff == 1 && ( g_engfuncs.pfnRandomLong( 0, 1 ) != 0 && spawn->team_no == CTFTeam::None ||
                                            g_engfuncs.pfnRandomLong( 0, 1 ) != 0 && spawn->team_no == CTFTeam( iLosingTeam + 1 ) ) ) ||
                    ( iScoreDiff > 1 && spawn->team_no == CTFTeam( iLosingTeam + 1 ) ) )
                {
                    nOccupied = false;
                    ++nTested;

                    for( CBaseEntity* test = nullptr; ( test = UTIL_FindEntityInSphere( test, spawn->pev->origin, 128 ) ) != nullptr; )
                    {
                        if( test->MyItemCTFPointer() && this != test )
                        {
                            nOccupied = true;
                            break;
                        }
                    }
                }

                if( !nOccupied )
                {
                    pev->origin = spawn->pev->origin;
                    if( g_engfuncs.pfnRandomLong( 0, 1 ) != 0 )
                    {
                        break;
                    }
                }
            }

            if( spawn == m_pLastSpawn )
                break;

            if( spawn )
                break;
        }
    }

    if( !spawn && pev->origin == pev->oldorigin )
    {
        for( spawn = nullptr; ( spawn = ( CItemSpawnCTF* )UTIL_FindEntityByClassname( spawn, "info_ctfspawn_powerup" ) ) != nullptr; )
        {
            bool nOccupied = false;

            for( CBaseEntity* test = nullptr; ( test = UTIL_FindEntityInSphere( test, spawn->pev->origin, 128 ) ) != nullptr; )
            {
                if( test->MyItemCTFPointer() && this != test )
                {
                    nOccupied = true;
                    break;
                }
            }

            if( !nOccupied )
            {
                pev->origin = spawn->pev->origin;
                if( g_engfuncs.pfnRandomLong( 0, 1 ) != 0 )
                {
                    break;
                }
            }
        }
    }

    m_pLastSpawn = spawn;

    if( !spawn && nTested > 0 )
        Logger->error( "Warning: No available spawn points found.  Powerup returned to original coordinates." );

    SetOrigin( pev->origin );

    if( g_engfuncs.pfnDropToFloor( edict() ) == 0 )
    {
        Logger->error( "Item {} fell out of level at {}", STRING( pev->classname ), pev->origin.MakeString() );
        UTIL_Remove( this );
    }
}

void CItemCTF::CarryThink()
{
    auto pOwner = ToBasePlayer( GetOwner() );

    if( pOwner )
    {
        if( ( m_iItemFlag & pOwner->m_iItems ) != 0 )
        {
            pev->nextthink = gpGlobals->time + 20;
        }
        else
        {
            pOwner->m_iClientItems = m_iItemFlag;
            DropItem( pOwner, true );
        }
    }
    else
    {
        DropItem( nullptr, true );
    }
}

void CItemCTF::ItemTouch( CBaseEntity* pOther )
{
    auto player = ToBasePlayer( pOther );

    // TODO: really shouldn't be using the index here tbh
    if( !player || !player->IsAlive() || ( m_iLastTouched == player->entindex() && m_flNextTouchTime > gpGlobals->time ) )
    {
        return;
    }

    m_iLastTouched = 0;

    if( MyTouch( player ) )
    {
        SUB_UseTargets( player, USE_TOGGLE );
        SetTouch( nullptr );

        pev->movetype = MOVETYPE_NONE;
        pev->solid = SOLID_NOT;
        pev->effects |= EF_NODRAW;

        SetOrigin( pev->origin );

        SetOwner( player );

        SetThink( &CItemCTF::CarryThink );
        pev->nextthink = gpGlobals->time + 20.0;

        m_flPickupTime = gpGlobals->time;

        CGameRules::Logger->trace( "{} triggered \"take_{}_Powerup\"", PlayerLogInfo{*player}, m_pszItemName );
    }
}

void CItemCTF::DropItem( CBasePlayer* pPlayer, bool bForceRespawn )
{
    if( pPlayer )
    {
        RemoveEffect( pPlayer );

        CGameRules::Logger->trace( "{} triggered \"drop_{}_Powerup\"", PlayerLogInfo{*pPlayer}, m_pszItemName );

        pev->origin = pPlayer->pev->origin + Vector( 0, 0, ( pPlayer->pev->flags & FL_DUCKING ) != 0 ? 34 : 16 );
    }

    SetOrigin( pev->origin );

    pev->flags &= ~FL_ONGROUND;
    pev->movetype = MOVETYPE_TOSS;
    pev->angles = g_vecZero;
    pev->solid = SOLID_TRIGGER;
    pev->effects &= ~EF_NODRAW;
    pev->velocity = g_vecZero;

    pev->velocity.z = -100;

    if( pev->owner )
    {
        auto pOwner = ToBasePlayer( pev->owner );

        if( pOwner )
        {
            pOwner->m_iItems = static_cast<CTFItem::CTFItem>( pOwner->m_iItems & ~m_iItemFlag );
        }

        pev->owner = nullptr;
    }

    SetTouch( &CItemCTF::ItemTouch );

    if( bForceRespawn )
    {
        DropThink();
    }
    else
    {
        SetThink( &CItemCTF::DropPreThink );
        pev->nextthink = gpGlobals->time + 5.0;
    }
}

void CItemCTF::ScatterItem( CBasePlayer* pPlayer )
{
    if( pPlayer )
    {
        RemoveEffect( pPlayer );

        pev->origin = pPlayer->pev->origin + Vector( 0, 0, ( pPlayer->pev->flags & FL_DUCKING ) != 0 ? 34 : 16 );
    }

    SetOrigin( pev->origin );

    pev->flags &= ~FL_ONGROUND;
    pev->movetype = MOVETYPE_TOSS;
    pev->angles = g_vecZero;
    pev->solid = SOLID_TRIGGER;
    pev->effects &= ~EF_NODRAW;
    pev->velocity = g_vecZero;

    pev->velocity = pPlayer->pev->velocity + Vector( RANDOM_FLOAT( 0, 1 ) * 274, RANDOM_FLOAT( 0, 1 ) * 274, 0 );

    pev->avelocity.y = 400;

    if( pev->owner )
    {
        auto pOwner = ToBasePlayer( pev->owner );

        if( pOwner )
        {
            pOwner->m_iItems = static_cast<CTFItem::CTFItem>( pOwner->m_iItems & ~m_iItemFlag );
        }

        pev->owner = nullptr;
    }

    SetTouch( &CItemCTF::ItemTouch );
    SetThink( &CItemCTF::DropPreThink );
    pev->nextthink = gpGlobals->time + 5.0;

    m_iLastTouched = pPlayer->entindex();
    m_flNextTouchTime = 5.0 + gpGlobals->time;

    CGameRules::Logger->trace( "{} triggered \"drop_{}_Powerup\"", PlayerLogInfo{*pPlayer}, m_pszItemName );
}

void CItemCTF::ThrowItem( CBasePlayer* pPlayer )
{
    if( pPlayer )
    {
        RemoveEffect( pPlayer );

        pev->origin = pPlayer->pev->origin + Vector( 0, 0, ( pPlayer->pev->flags & FL_DUCKING ) != 0 ? 34 : 16 );
    }

    SetOrigin( pev->origin );

    EmitSound( CHAN_VOICE, "ctf/itemthrow.wav", VOL_NORM, ATTN_NORM );

    pev->flags &= ~FL_ONGROUND;
    pev->movetype = MOVETYPE_TOSS;
    pev->angles = g_vecZero;
    pev->solid = SOLID_TRIGGER;
    pev->effects &= ~EF_NODRAW;
    pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * 274;

    pev->avelocity.y = 400;

    if( pev->owner )
    {
        auto pOwner = ToBasePlayer( pev->owner );

        if( pOwner )
        {
            pOwner->m_iItems = static_cast<CTFItem::CTFItem>( pOwner->m_iItems & ~m_iItemFlag );
        }

        pev->owner = nullptr;
    }

    SetTouch( &CItemCTF::ItemTouch );
    SetThink( &CItemCTF::DropPreThink );
    pev->nextthink = gpGlobals->time + 5.0;

    m_iLastTouched = pPlayer->entindex();
    m_flNextTouchTime = 5.0 + gpGlobals->time;

    CGameRules::Logger->trace( "{} triggered \"drop_{}_Powerup\"", PlayerLogInfo{*pPlayer}, m_pszItemName );
}

LINK_ENTITY_TO_CLASS( info_ctfspawn_powerup, CItemSpawnCTF );

bool CItemSpawnCTF::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( "team_no", pkvd->szKeyName ) )
    {
        team_no = static_cast<CTFTeam>( atoi( pkvd->szValue ) );
        return true;
    }

    return CPointEntity::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( item_ctfaccelerator, CItemAcceleratorCTF );

void CItemAcceleratorCTF::OnCreate()
{
    CItemCTF::OnCreate();

    pev->model = MAKE_STRING( "models/w_accelerator.mdl" );
}

void CItemAcceleratorCTF::Precache()
{
    CItemCTF::Precache();

    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "turret/tu_ping.wav" );
}

SpawnAction CItemAcceleratorCTF::Spawn()
{
    Precache();

    SetModel( STRING( pev->model ) );

    pev->oldorigin = pev->origin;

    CItemCTF::Spawn();

    m_iItemFlag = CTFItem::Acceleration;
    m_pszItemName = "Damage";

    return SpawnAction::Spawn;
}

void CItemAcceleratorCTF::RemoveEffect( CBasePlayer* pPlayer )
{
    pPlayer->m_flAccelTime += gpGlobals->time - m_flPickupTime;
}

bool CItemAcceleratorCTF::MyTouch( CBasePlayer* pPlayer )
{
    if( ( pPlayer->m_iItems & CTFItem::Acceleration ) == 0 )
    {
        if( 0 == multipower.value )
        {
            if( ( pPlayer->m_iItems & CTFItem::ItemsMask ) != 0 )
                return false;
        }

        if( team_no == CTFTeam::None || team_no == pPlayer->m_iTeamNum )
        {
            if( pPlayer->HasSuit() )
            {
                pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::Acceleration );
                MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, nullptr, pPlayer );
                WRITE_STRING( STRING( pev->classname ) );
                MESSAGE_END();
                EmitSound( CHAN_VOICE, "items/ammopickup1.wav", VOL_NORM, ATTN_NORM );
                return true;
            }
        }
    }

    return false;
}

LINK_ENTITY_TO_CLASS( item_ctfbackpack, CItemBackpackCTF );

void CItemBackpackCTF::OnCreate()
{
    CItemCTF::OnCreate();

    pev->model = MAKE_STRING( "models/w_backpack.mdl" );
}

void CItemBackpackCTF::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "ctf/pow_backpack.wav" );
}

void CItemBackpackCTF::RemoveEffect( CBasePlayer* pPlayer )
{
    pPlayer->m_flBackpackTime += gpGlobals->time - m_flPickupTime;
}

bool CItemBackpackCTF::MyTouch( CBasePlayer* pPlayer )
{
    if( ( pPlayer->m_iItems & CTFItem::Backpack ) == 0 )
    {
        if( 0 == multipower.value )
        {
            if( ( pPlayer->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) != 0 )
                return false;
        }

        if( static_cast<int>( team_no ) <= 0 || team_no == pPlayer->m_iTeamNum )
        {
            if( pPlayer->HasSuit() )
            {
                pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::Backpack );
                MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, nullptr, pPlayer );
                g_engfuncs.pfnWriteString( STRING( pev->classname ) );
                g_engfuncs.pfnMessageEnd();

                EmitSound( CHAN_VOICE, "items/ammopickup1.wav", VOL_NORM, ATTN_NORM );

                pPlayer->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium" );
                pPlayer->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm" );
                pPlayer->GiveAmmo( AMMO_357BOX_GIVE, "357" );
                pPlayer->GiveAmmo( AMMO_BUCKSHOTBOX_GIVE, "buckshot" );
                pPlayer->GiveAmmo( CROSSBOW_DEFAULT_GIVE, "bolts" );
                pPlayer->GiveAmmo( 1, "rockets" );
                pPlayer->GiveAmmo( HANDGRENADE_DEFAULT_GIVE, "Hand Grenade" );
                pPlayer->GiveAmmo( SNARK_DEFAULT_GIVE, "Snarks" );
                pPlayer->GiveAmmo( SPORELAUNCHER_DEFAULT_GIVE, "spores" );
                pPlayer->GiveAmmo( SNIPERRIFLE_DEFAULT_GIVE, "762" );
                pPlayer->GiveAmmo( M249_MAX_CARRY, "556" );

                return true;
            }
        }
    }

    return false;
}

SpawnAction CItemBackpackCTF::Spawn()
{
    PrecacheSound( "ctf/itemthrow.wav" );
    PrecacheSound( "items/ammopickup1.wav" );

    Precache();

    SetModel( STRING( pev->model ) );
    pev->oldorigin = pev->origin;

    CItemCTF::Spawn();

    m_iItemFlag = CTFItem::Backpack;
    m_pszItemName = "Ammo";

    return SpawnAction::Spawn;
}

LINK_ENTITY_TO_CLASS( item_ctflongjump, CItemLongJumpCTF );

void CItemLongJumpCTF::OnCreate()
{
    CItemCTF::OnCreate();

    pev->model = MAKE_STRING( "models/w_jumppack.mdl" );
}

void CItemLongJumpCTF::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "ctf/pow_big_jump.wav" );
}

void CItemLongJumpCTF::RemoveEffect( CBasePlayer* pPlayer )
{
    pPlayer->m_fLongJump = false;
    g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "jpj", "0" );
    pPlayer->m_flJumpTime += gpGlobals->time - m_flPickupTime;
}

bool CItemLongJumpCTF::MyTouch( CBasePlayer* pPlayer )
{
    if( ( pPlayer->m_iItems & CTFItem::LongJump ) == 0 )
    {
        if( 0 == multipower.value )
        {
            if( ( pPlayer->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) != 0 )
                return false;
        }

        if( static_cast<int>( team_no ) <= 0 || team_no == pPlayer->m_iTeamNum )
        {
            if( pPlayer->HasSuit() )
            {
                pPlayer->m_fLongJump = true;
                g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "jpj", "1" );

                pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::LongJump );

                MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, nullptr, pPlayer );
                g_engfuncs.pfnWriteString( STRING( pev->classname ) );
                g_engfuncs.pfnMessageEnd();

                EmitSound( CHAN_VOICE, "items/ammopickup1.wav", VOL_NORM, ATTN_NORM );

                return true;
            }
        }
    }

    return false;
}

SpawnAction CItemLongJumpCTF::Spawn()
{
    PrecacheSound( "ctf/itemthrow.wav" );
    PrecacheSound( "items/ammopickup1.wav" );

    Precache();

    SetModel( STRING( pev->model ) );

    pev->oldorigin = pev->origin;
    CItemCTF::Spawn();

    m_iItemFlag = CTFItem::LongJump;
    m_pszItemName = "Jump";

    return SpawnAction::Spawn;
}

LINK_ENTITY_TO_CLASS( item_ctfportablehev, CItemPortableHEVCTF );

void CItemPortableHEVCTF::OnCreate()
{
    CItemCTF::OnCreate();

    pev->model = MAKE_STRING( "models/w_porthev.mdl" );
}

void CItemPortableHEVCTF::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "ctf/pow_armor_charge.wav" );
}

void CItemPortableHEVCTF::RemoveEffect( CBasePlayer* pPlayer )
{
    pPlayer->m_flShieldTime += gpGlobals->time - m_flPickupTime;
}

bool CItemPortableHEVCTF::MyTouch( CBasePlayer* pPlayer )
{
    if( ( pPlayer->m_iItems & CTFItem::PortableHEV ) == 0 )
    {
        if( 0 == multipower.value )
        {
            if( ( pPlayer->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) != 0 )
                return false;
        }

        if( static_cast<int>( team_no ) <= 0 || team_no == pPlayer->m_iTeamNum )
        {
            if( pPlayer->HasSuit() )
            {
                pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::PortableHEV );
                pPlayer->m_fPlayingAChargeSound = false;

                MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, nullptr, pPlayer );
                g_engfuncs.pfnWriteString( STRING( pev->classname ) );
                g_engfuncs.pfnMessageEnd();

                EmitSound( CHAN_VOICE, "items/ammopickup1.wav", VOL_NORM, ATTN_NORM );

                return true;
            }
        }
    }
    return false;
}

SpawnAction CItemPortableHEVCTF::Spawn()
{
    PrecacheSound( "ctf/itemthrow.wav" );
    PrecacheSound( "items/ammopickup1.wav" );

    Precache();

    SetModel( STRING( pev->model ) );

    pev->oldorigin = pev->origin;

    CItemCTF::Spawn();

    m_iItemFlag = CTFItem::PortableHEV;
    m_pszItemName = "Shield";

    return SpawnAction::Spawn;
}

LINK_ENTITY_TO_CLASS( item_ctfregeneration, CItemRegenerationCTF );

void CItemRegenerationCTF::OnCreate()
{
    CItemCTF::OnCreate();

    pev->model = MAKE_STRING( "models/w_health.mdl" );
}

void CItemRegenerationCTF::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheSound( "ctf/pow_health_charge.wav" );
}

void CItemRegenerationCTF::RemoveEffect( CBasePlayer* pPlayer )
{
    pPlayer->m_flHealthTime += gpGlobals->time - m_flPickupTime;
}

bool CItemRegenerationCTF::MyTouch( CBasePlayer* pPlayer )
{
    if( ( pPlayer->m_iItems & CTFItem::Regeneration ) == 0 )
    {
        if( 0 == multipower.value )
        {
            if( ( pPlayer->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) != 0 )
                return false;
        }

        if( static_cast<int>( team_no ) <= 0 || team_no == pPlayer->m_iTeamNum )
        {
            if( pPlayer->HasSuit() )
            {
                pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::Regeneration );
                pPlayer->m_fPlayingHChargeSound = false;

                MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, nullptr, pPlayer );
                g_engfuncs.pfnWriteString( STRING( pev->classname ) );
                g_engfuncs.pfnMessageEnd();

                EmitSound( CHAN_VOICE, "items/ammopickup1.wav", VOL_NORM, ATTN_NORM );

                // TODO: check player's max health.
                if( pPlayer->pev->health < 100.0 )
                {
                    pPlayer->GiveHealth( g_cfg.GetValue<float>( "healthkit"sv, 10, this ), DMG_GENERIC );
                }

                return true;
            }
        }
    }

    return false;
}

SpawnAction CItemRegenerationCTF::Spawn()
{
    PrecacheSound( "ctf/itemthrow.wav" );
    PrecacheSound( "items/ammopickup1.wav" );

    Precache();

    SetModel( STRING( pev->model ) );

    pev->oldorigin = pev->origin;

    CItemCTF::Spawn();

    m_iItemFlag = CTFItem::Regeneration;
    m_pszItemName = "Health";

    return SpawnAction::Spawn;
}
