/***
 *
 *    Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#include "hud.h"

bool CHudDlight::Init()
{
    m_iFlags = HUD_ACTIVE;

    g_ClientUserMessages.RegisterHandler( "TempLight", &CHudDlight::MsgFunc_DrawLight, this );

    m_Muzzleflash = gEngfuncs.pfnRegisterVariable( "cl_dlight_weapons", "1", FCVAR_ARCHIVE );
    m_StudioModelRendering = gEngfuncs.pfnRegisterVariable( "cl_dlight_studiomodels", "1", FCVAR_ARCHIVE );
    m_Explosions = gEngfuncs.pfnRegisterVariable( "cl_dlight_explosions", "1", FCVAR_ARCHIVE );

    gHUD.AddHudElem( this );

    return true;
};


bool CHudDlight::VidInit()
{
    return true;
};

bool CHudDlight::ShouldDisplay( const DynamicLightType type )
{
    switch( type )
    {
        case DynamicLightType::Muzzleflash:
            return ( m_Muzzleflash->value != 0 );
        case DynamicLightType::StudioModelRendering:
            return ( m_StudioModelRendering->value != 0 );
        case DynamicLightType::Explosions:
            return ( m_Explosions->value != 0 );
        default:
            return false;
    }
}

dlight_t* CHudDlight::AllocDLight( const DynamicLightType type )
{
    if( ShouldDisplay( type ) )
    {
        if( dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight(0); light != nullptr )
        {
            light->die = gEngfuncs.GetClientTime();
            return light;
        }
    }

    return nullptr;
}

void CHudDlight::MsgFunc_DrawLight( const char* pszName, BufferReader& reader )
{
    DynamicLightType type = static_cast<DynamicLightType>( reader.ReadByte() );

    if( dlight_t* light = AllocDLight( type ); light != nullptr )
    {
        light->origin = reader.ReadCoordVector();
        gEngfuncs.Con_DPrintf( "DLight at origin %s.", light->origin.MakeString().c_str() );
        light->radius = reader.ReadByte();
        gEngfuncs.Con_DPrintf( " radius %f.", light->radius );
        light->color.r = reader.ReadByte();
        gEngfuncs.Con_DPrintf( " r %i.", light->color.r );
        light->color.g = reader.ReadByte();
        gEngfuncs.Con_DPrintf( " g %i.", light->color.g );
        light->color.b = reader.ReadByte();
        gEngfuncs.Con_DPrintf( " b %i.", light->color.b );
        light->die += reader.ReadCoord();
        gEngfuncs.Con_DPrintf( " die %f.", light->die );
        light->decay = reader.ReadLong();
        gEngfuncs.Con_DPrintf( " decay %f.", light->decay );
        light->dark = reader.ReadByte();
        gEngfuncs.Con_DPrintf( " dark %i.\n", light->dark );
    }
}

bool CHudDlight::Draw( float flTime )
{
    return true;
}
