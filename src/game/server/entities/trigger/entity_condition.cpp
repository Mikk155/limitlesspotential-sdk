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

class CTriggerEntityCondition : public CPointEntity
{
    DECLARE_CLASS( CTriggerEntityCondition, CPointEntity );
    DECLARE_DATAMAP();

    public:
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    private:
        string_t m_PassTarget;
        string_t m_FailTarget;
        int m_Condition;
        string_t m_Arg1;
};

LINK_ENTITY_TO_CLASS( trigger_entity_condition, CTriggerEntityCondition );

BEGIN_DATAMAP( CTriggerEntityCondition )
    DEFINE_FIELD( m_PassTarget, FIELD_STRING ),
    DEFINE_FIELD( m_FailTarget, FIELD_STRING ),
    DEFINE_FIELD( m_Condition, FIELD_INTEGER ),
    DEFINE_FIELD( m_Arg1, FIELD_STRING ),
END_DATAMAP();

bool CTriggerEntityCondition :: KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "pass_target" ) )
    {
        m_PassTarget = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "fail_target" ) )
    {
        m_FailTarget = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "condition" ) )
    {
        m_Condition = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "argument_1" ) )
    {
        m_Arg1 = ALLOC_STRING( pkvd->szValue );
    }
    else
    {
        return CPointEntity::KeyValue( pkvd );
    }
    return true;
}

void CTriggerEntityCondition :: Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    bool bCondition = false;

    if( pActivator != nullptr )
    {
        switch( m_Condition )
        {
            case 0:
            {
                bCondition = pActivator->IsPlayer();
                break;
            }
            case 1:
            {
                bCondition = pActivator->IsPlayer() && ToBasePlayer( pActivator )->HasSuit();
                break;
            }
            case 2:
            {
                if( CBasePlayer* player = ToBasePlayer( pActivator ); player != nullptr )
                {
                    if( !FStringNull( m_Arg1 ) )
                    {
                        if( FStrEq( STRING( m_Arg1 ), "item_longjump" ) ) {
                            bCondition = player->HasLongJump();
                        }
                        else {
                            bCondition =  player->HasNamedPlayerWeapon( STRING( m_Arg1 ) );
                        }
                    }
                    else
                    {
                        bCondition = player->HasWeapons();
                    }
                }
                break;
            }
            case 3:
            {
                bCondition = pActivator->IsMonster();
                break;
            }
            case 4:
            {
                bCondition = pActivator->IsAlive();
                break;
            }
        }
    }

    FireTargets( ( bCondition ? STRING( m_PassTarget ) : STRING( m_FailTarget ) ), pActivator, this, USE_TOGGLE, value );

    FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE, value );
}
