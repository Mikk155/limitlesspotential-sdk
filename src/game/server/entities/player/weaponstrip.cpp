/***
*
* Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
* This product contains software technology licensed from Id
* Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
* All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "cbase.h"

#define SF_WEAPONSTRIP_REMOVEALL ( 0 << 1 )

class CStripWeapons : public CPointEntity
{
    DECLARE_CLASS( CStripWeapons, CPointEntity );
    DECLARE_DATAMAP();

    public:
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
        void FireItsTarget( CBasePlayer* pActivator, int index, bool bShouldFire );

    private:
        int m_cTargets;
        string_t m_iKey[MAX_WEAPONS];
        string_t m_iValue[MAX_WEAPONS];
};

LINK_ENTITY_TO_CLASS( player_weaponstrip, CStripWeapons );

BEGIN_DATAMAP( CStripWeapons )
    DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
    DEFINE_ARRAY( m_iKey, FIELD_STRING, MAX_WEAPONS ),
    DEFINE_ARRAY( m_iValue, FIELD_STRING, MAX_WEAPONS ),
END_DATAMAP();

bool CStripWeapons::KeyValue(KeyValueData* pkvd)
{
    if( strncmp( pkvd->szKeyName, "weapon_", 7 ) == 0
    ||   strncmp( pkvd->szKeyName, "item_", 5 ) == 0
    ||   strncmp( pkvd->szKeyName, "ammo_", 5 ) == 0 )
    {
        char temp[256];

        UTIL_StripToken( pkvd->szKeyName, temp );
        m_iKey[m_cTargets] = ALLOC_STRING(temp );

        UTIL_StripToken( pkvd->szValue, temp );
        m_iValue[m_cTargets] = ALLOC_STRING( temp );

        ++m_cTargets;
    }
    else
    {
        return CPointEntity::KeyValue( pkvd );
    }
    return true;
}

void CStripWeapons::FireItsTarget( CBasePlayer* pActivator, int index, bool bShouldFire )
{
    if( bShouldFire && !FStringNull( m_iValue[index] ) )
    {
        FireTargets( STRING( m_iValue[index] ), pActivator, this, USE_TOGGLE );
    }
}

void CStripWeapons::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( IsLockedByMaster(pActivator) )
        return;

    auto player = ToBasePlayer(pActivator);

    if( player == nullptr )
    {
        if( !g_pGameRules->IsMultiplayer() )
            player = UTIL_GetLocalPlayer();
        else
            return;
    }

    if( !FBitSet( pev->spawnflags, SF_WEAPONSTRIP_REMOVEALL ) )
    {
        player->RemoveAllItems( false );
        player->SetHasLongJump( false );
        player->SetHasSuit( false );
        return;
    }

    for( int i = 0; i < m_cTargets; ++i )
    {
        if( FStringNull( m_iKey[i] ) )
            continue;

        // Doing this after strips in case the mapper wants to do some chains
        bool condition = false;

        if( FStrEq( STRING( m_iKey[i] ), "weapon_*" ) )
        {
            condition = player->HasWeapons();
            player->RemoveAllItems( false );
            FireItsTarget( player, i, condition );
        }
        else if( FStrEq( STRING( m_iKey[i] ), "ammo_*" ) )
        {
            for( int count = 0; count <= 32; count++ )
            {
                condition = ( player->GetAmmoCountByIndex( count ) > 0 );
                player->SetAmmoCountByIndex( count, 0 );
            }
            FireItsTarget( player, i, condition );
        }
        else if( FStrEq( STRING( m_iKey[i] ), "item_longjump" ) )
        {
            condition = player->HasLongJump();
            player->SetHasLongJump( false );
            FireItsTarget( player, i, condition );
        }
        else if( FStrEq( STRING( m_iKey[i] ), "item_suit" ) )
        {
            condition = player->HasSuit();
            player->SetHasSuit( false );
            FireItsTarget( player, i, condition );
        }
        else if( strncmp( STRING( m_iKey[i] ), "weapon_", 7 ) == 0 )
        {
            condition = player->RemovePlayerWeapon( player->HasNamedPlayerWeaponPtr( STRING( m_iKey[i] ) ) );
            FireItsTarget( player, i, condition );
        }
        else if( strncmp( STRING( m_iKey[i] ), "ammo_", 5 ) == 0 )
        {
            condition = ( player->GetAmmoCount( STRING( m_iKey[i] ) ) > 0 );
            player->AdjustAmmoByIndex( player->GetAmmoIndex( STRING( m_iKey[i] ) ), 0 );
            FireItsTarget( player, i, condition );
        }
    }
}