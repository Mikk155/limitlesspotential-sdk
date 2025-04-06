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

bool CHudSpeedometer::Init()
{
    m_iFlags = HUD_ACTIVE;

    m_pSpeedometerHeight = CVAR_CREATE( "hud_speedometer_height", "1.0", FCVAR_ARCHIVE );
    m_pSpeedometerWidth = CVAR_CREATE( "hud_speedometer_width", "0.50", FCVAR_ARCHIVE );

    gHUD.AddHudElem( this );

    return true;
};

bool CHudSpeedometer::VidInit()
{
    return true;
};

void CHudSpeedometer::SetSpeed( float SpeedLength )
{
    speed = std::min( static_cast<int>( std::round( std::floor( SpeedLength + 0.5 ) ) ), 999 );
}

bool CHudSpeedometer::Draw( float flTime )
{
    if( !gHUD.HasSuit() || speed <= 0 )
        return false;

    RGB24 color;

    // -TODO How can we get the current player's max speed?
    uint16_t maxspeed = 270;

    if( speed < maxspeed )
    {
        int Alpha = static_cast<int>( ( ( speed - maxspeed ) * 255 ) / maxspeed );
        color = gHUD.m_HudColor.Scale( Alpha );
    }
    else
    {
        // -TODO this should be the maximun velocity a player can get bhoping
        int maximun_velocity = static_cast<int>( maxspeed * 1.5 );

        if( speed > maximun_velocity )
            maximun_velocity = speed;

        int Alpha = static_cast<int>( ( ( speed - maxspeed ) * 255 ) / ( maximun_velocity - maxspeed ) );
        color = gHUD.m_HudColor.ScaleColor( { 255, 0, 0 }, Alpha );
    }

    // Clamp values to a valid size
    m_pSpeedometerHeight->value = std::max( 0.0f, std::min( 1.0f, m_pSpeedometerHeight->value ) );
    m_pSpeedometerWidth->value = std::max( 0.0f, std::min( 1.0f, m_pSpeedometerWidth->value ) );

    int Height = static_cast<int>( ( ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight ) * m_pSpeedometerHeight->value );
    int Width = static_cast<int>( ( ScreenWidth ) * m_pSpeedometerWidth->value );

    int ScreenMiddle = static_cast<int>( ScreenWidth / 2 );

    // If it's more to the right then move the numbers to the left direction
    if( Width > ScreenMiddle )
    {
        Width -= gHUD.GetHudNumberWidth( speed, 3, 0 );
    }
    // Medium size of the numbers as we are centered.
    else if( Width == ScreenMiddle )
    {
        Width -= gHUD.GetHudNumberWidth( speed, 3, 0 ) / 2;
    }

    gHUD.DrawHudNumber( Width, Height, DHN_3DIGITS, speed, color );

    return true;
}
