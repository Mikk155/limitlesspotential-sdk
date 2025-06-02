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

#include "PlayerList.h"

void PlayerList::add( CBasePlayer* player, CBaseEntity* target )
{
    if( player != nullptr && target != nullptr )
    {
        auto& list = player->m_listed_entities;
        auto index = target->entindex();

        if( std::find( list.begin(), list.end(), index ) == list.end() )
        {
            list.push_back( index );
        }
    }
}

void PlayerList::remove( CBasePlayer* player, CBaseEntity* target )
{
    if( player != nullptr && target != nullptr )
    {
        auto& list = player->m_listed_entities;
        auto index = target->entindex();

        if( std::find( list.begin(), list.end(), index ) != list.end() )
        {
            list.erase( std::remove( list.begin(), list.end(), index ), list.end() );
        }
    }
}

bool PlayerList::toggle( CBasePlayer* player, CBaseEntity* target )
{
    if( player != nullptr && target != nullptr )
    {
        auto& list = player->m_listed_entities;
        auto index = target->entindex();

        if( std::find( list.begin(), list.end(), index ) == list.end() )
        {
            list.push_back( index );
            return true;
        }
        else
        {
            list.erase( std::remove( list.begin(), list.end(), index ), list.end() );
        }
    }

    return false;
}

bool PlayerList::exists( CBasePlayer* player, CBaseEntity* target )
{
    if( player != nullptr && target != nullptr )
    {
        auto& list = player->m_listed_entities;

        return ( std::find( list.begin(), list.end(), target->entindex() ) != list.end() );
    }

    return false;
}

void PlayerList::clear( CBasePlayer* player )
{
    if( player != nullptr )
    {
        player->m_listed_entities.clear();
    }
}
