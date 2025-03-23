/***
 *
 *  Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *  This product contains software technology licensed from Id
 *  Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *  All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include "cbase.h"

class CTriggerZone : public CBaseEntity
{
    DECLARE_CLASS( CTriggerZone, CBaseEntity );
    DECLARE_DATAMAP();

public:
    bool Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

private:
    string_t m_iszEntity;
    string_t m_iszOutCount;
    string_t m_iszInCount;
    USE_TYPE m_InUse = USE_TOGGLE;
    int m_InValue;
    USE_TYPE m_OutUse = USE_TOGGLE;
    int m_OutValue;
};

BEGIN_DATAMAP( CTriggerZone )
    DEFINE_FIELD( m_InUse, FIELD_INTEGER ),
    DEFINE_FIELD( m_InValue, FIELD_INTEGER ),
    DEFINE_FIELD( m_OutUse, FIELD_INTEGER ),
    DEFINE_FIELD( m_OutValue, FIELD_INTEGER ),
    DEFINE_FIELD( m_iszEntity, FIELD_STRING ),
    DEFINE_FIELD( m_iszOutCount, FIELD_STRING ),
    DEFINE_FIELD( m_iszInCount, FIELD_STRING ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( trigger_zone, CTriggerZone );

bool CTriggerZone::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_iszEntity" ) )
    {
        m_iszEntity = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_InUse" ) )
    {
        m_InUse = static_cast<USE_TYPE>( atoi( pkvd->szValue ) );
    }
    else if( FStrEq( pkvd->szKeyName, "m_OutUse" ) )
    {
        m_OutUse = static_cast<USE_TYPE>( atoi( pkvd->szValue ) );
    }
    else if( FStrEq( pkvd->szKeyName, "m_InValue" ) )
    {
        m_InValue = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_OutValue" ) )
    {
        m_OutValue = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iszInCount" ) )
    {
        m_iszInCount = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_iszOutCount" ) )
    {
        m_iszOutCount = ALLOC_STRING( pkvd->szValue );
    }
    else
    {
        return CBaseEntity::KeyValue(pkvd);
    }
    return true;
}

bool CTriggerZone::Spawn()
{
    pev->solid = SOLID_NOT;
    pev->effects |= EF_NODRAW;
    pev->movetype = MOVETYPE_NONE;
    SetModel( STRING( pev->model ) );
    return true;
}

void CTriggerZone::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( IsLockedByMaster( pActivator ) )
        return;

    int in_entities = 0;
    int out_entities = 0;

    if( !FStringNull( m_iszEntity ) )
    {
        CBaseEntity* pEntity = NULL;

        while( ( pEntity = UTIL_FindEntityByClassname( pEntity, STRING( m_iszEntity ) ) ) )
        {
            if( Intersects( pEntity ) )
            {
                in_entities++;
                FireTargets( STRING(pev->message), pEntity, this, m_InUse, UseValue(m_InValue) );
            }
            else
            {
                out_entities++;
                FireTargets( STRING(pev->netname), pEntity, this, m_OutUse, UseValue(m_OutValue) );
            }
        }
    }

    if( !FStringNull( m_iszInCount ) )
    {
        FireTargets( STRING( m_iszInCount ), pActivator, this, USE_SET, UseValue(in_entities) );
    }

    if( !FStringNull( m_iszOutCount ) )
    {
        FireTargets( STRING( m_iszOutCount ), pActivator, this, USE_SET, UseValue(out_entities) );
    }

    FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE );
}
