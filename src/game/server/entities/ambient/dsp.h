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

class CAmbientDSP : public CAmbient
{
    DECLARE_CLASS( CAmbientDSP, CAmbient );
    DECLARE_DATAMAP();

public:
    bool Spawn() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    void UpdateRoomType( CBasePlayer* player, USE_TYPE useType );

    int m_Roomtype;

    void DSPTouch( CBaseEntity* pOther );

protected:
    bool m_bState = true;

private:
    bool m_bIsTrigger;
};
