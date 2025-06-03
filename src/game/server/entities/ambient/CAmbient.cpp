/***
*
*  Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*   This product contains software technology licensed from Id
*   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*   All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "CAmbient.h"

BEGIN_DATAMAP( CAmbient )
    DEFINE_FIELD( m_sPlaySound, FIELD_STRING ),
    DEFINE_FIELD( m_sPlayFrom, FIELD_STRING ),
END_DATAMAP();

bool CAmbient :: KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_sPlayFrom" ) )
    {
        m_sPlayFrom = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_sPlaySound" ) )
    {
        m_sPlaySound = ALLOC_STRING( pkvd->szValue );
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CAmbient::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( ( pev->spawnflags & AMBIENT_DONT_REMOVE ) == 0 )
    {
        UTIL_Remove(this);
    }
}

Vector CAmbient::PlayFromEntity( CBaseEntity* activator, CBaseEntity* caller )
{
    if( !FStringNull( m_sPlayFrom ) )
    {
        if( CBaseEntity* ent = AllocNewActivator( activator, caller, m_sPlayFrom ); ent != nullptr )
        {
            if( ent->IsMonster() )
            {
                return ent->EyePosition();
            }
            else if( ent->IsBSPModel() && ent->pev->origin == g_vecZero ) // Allow for origin texture
            {
                return ent->Center();
            }

            return ent->pev->origin;
        }
        else
        {
            CBaseEntity::Logger->warn( "{} Failed to get \"{}\", Playing at origin {}",
                STRING(pev->classname), STRING(m_sPlayFrom), STRING(pev->targetname), pev->origin.MakeString(0) );
        }
    }

    return pev->origin;
}

void CAmbient::Precache()
{
    if( !FStringNull( m_sPlaySound ) )
    {
        const char* szSoundFile = STRING( m_sPlaySound );

        if( strlen( szSoundFile ) > 1 )
        {
            if( *szSoundFile != '!' )
            {
                PrecacheSound( szSoundFile );
            }
        }
    }
}
