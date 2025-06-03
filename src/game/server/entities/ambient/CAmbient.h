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

#include "cbase.h"

#define AMBIENT_ACTIVATOR_ONLY ( 1 << 0 )
#define AMBIENT_DONT_REMOVE ( 1 << 1 )

class CAmbient : public CPointEntity
{
    DECLARE_CLASS( CAmbient, CPointEntity );
    DECLARE_DATAMAP();

    public:

        void Precache() override;
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

        [[nodiscard]] Vector PlayFromEntity( CBaseEntity* activator, CBaseEntity* caller );

        bool Spawn() override
        {
            pev->solid = SOLID_NOT;
            pev->movetype = MOVETYPE_NONE;

            return true;
        }

    protected:
        string_t m_sPlaySound;
        string_t m_sPlayFrom;
};
