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
#include "UserMessages.h"

class CLogicCampaignSelect : public CPointEntity
{
    DECLARE_CLASS( CLogicCampaignSelect, CPointEntity );

public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
};

LINK_ENTITY_TO_CLASS( logic_campaignselect, CLogicCampaignSelect );

void CLogicCampaignSelect::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( auto player = ToBasePlayer( pActivator ); player )
    {
        MESSAGE_BEGIN( MSG_ONE, gmsgCampaignSelect, nullptr, player );
        MESSAGE_END();
    }
}

constexpr int MaxRandomTargets = 16;
constexpr std::string_view TargetKeyValuePrefix{"target"sv};

class CLogicRandom : public CBaseDelay
{
    DECLARE_CLASS( CLogicRandom, CBaseDelay );
    DECLARE_DATAMAP();

public:
    int ObjectCaps() override { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

    bool KeyValue( KeyValueData* pkvd ) override;

    bool Spawn() override;

    void RandomUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );

private:
    eastl::fixed_vector<std::uint8_t, MaxRandomTargets, false> BuildMap() const;

private:
    string_t m_iszTargets[MaxRandomTargets];
};

BEGIN_DATAMAP( CLogicRandom )
    DEFINE_ARRAY( m_iszTargets, FIELD_STRING, MaxRandomTargets ),
    DEFINE_FUNCTION( RandomUse ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( logic_random, CLogicRandom );

bool CLogicRandom::KeyValue( KeyValueData* pkvd )
{
    if( std::string_view{pkvd->szKeyName}.starts_with( TargetKeyValuePrefix ) )
    {
        const char* indexString = pkvd->szKeyName + TargetKeyValuePrefix.size();

        char* end;

        int index = strtol( indexString, &end, 10 );

        // Must be a number and end after the number, e.g. "target1"
        // index is 1 based [1, MaxRandomTargets]
        if( end != indexString && *end == '\0' )
        {
            --index;

            if( index >= 0 && index < MaxRandomTargets )
            {
                if( FStringNull( m_iszTargets[index] ) )
                {
                    m_iszTargets[index] = ALLOC_STRING( pkvd->szValue );
                }
                else
                {
                    Logger->error( "{}: target \"{}\" already set to \"{}\"",
                        GetClassname(), pkvd->szKeyName, STRING( m_iszTargets[index] ) );
                }
            }
            else
            {
                Logger->error( "{}: invalid target index \"{}\", must be in range [1, {}]",
                    GetClassname(), pkvd->szKeyName, MaxRandomTargets );
            }
        }
        else
        {
            Logger->error( "{}: invalid target format \"{}\": must be {}[1, {}]",
                GetClassname(), pkvd->szKeyName, TargetKeyValuePrefix, MaxRandomTargets );
        }

        return true;
    }

    return BaseClass::KeyValue( pkvd );
}

bool CLogicRandom::Spawn()
{
    SetUse( &CLogicRandom::RandomUse );

    return true;
}

void CLogicRandom::RandomUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( const auto map = BuildMap(); !map.empty() )
    {
        const int index = RANDOM_LONG( 0, map.size() - 1 );

        const int targetIndex = map[index];

        const char* target = STRING( m_iszTargets[targetIndex] );

        FireTargets( target, pActivator, this, useType );
    }
}

eastl::fixed_vector<std::uint8_t, MaxRandomTargets, false> CLogicRandom::BuildMap() const
{
    eastl::fixed_vector<std::uint8_t, MaxRandomTargets, false> map;

    for( int i = 0; i < MaxRandomTargets; ++i )
    {
        if( !FStringNull( m_iszTargets[i] ) )
        {
            map.push_back( i );
        }
    }

    return map;
}
