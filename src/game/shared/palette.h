/***
 *
 *    Copyright (c) 1999, Valve LLC. All rights reserved.
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

#pragma once

#include "Platform.h"

struct RGB24
{
    std::uint8_t Red = 255;
    std::uint8_t Green = 255;
    std::uint8_t Blue = 255;

    /**
     *    @brief Returns a copy of this color scaled by the given amount
     *    @param amount Scale, expressed as a value in the range [0, 255]
     */
    constexpr RGB24 Scale(std::uint8_t amount) const noexcept
    {
        const float x = amount / 255.f;
        const auto r = static_cast<std::uint8_t>( Red * x );
        const auto g = static_cast<std::uint8_t>( Green * x );
        const auto b = static_cast<std::uint8_t>( Blue * x );

        return {r, g, b};
    }

    /**
     *    @brief Returns a copy of this color scaled to the given RGB24 and amount
     *    @param amount Scale, expressed as a value in the range [0, 255]
     *    @param target Target color
     */
    constexpr RGB24 ScaleColor( const RGB24& target, std::uint8_t amount ) const noexcept
    {
        const float t = amount / 255.f;
    
        const auto r = static_cast<std::uint8_t>( Red + ( target.Red - Red ) * t );
        const auto g = static_cast<std::uint8_t>( Green + ( target.Green - Green ) * t );
        const auto b = static_cast<std::uint8_t>( Blue + ( target.Blue - Blue ) * t );

        return {r, g, b};
    }

    constexpr int ToInteger() const
    {
        return Red | ( Green << 8 ) | ( Blue << 16 );
    }

    static constexpr RGB24 FromInteger( int color )
    {
        return {
            static_cast<std::uint8_t>( color & 0xFF ),
            static_cast<std::uint8_t>( ( color & 0xFF00 ) >> 8 ),
            static_cast<std::uint8_t>( ( color & 0XFF0000 ) >> 16 )};
    }
};
