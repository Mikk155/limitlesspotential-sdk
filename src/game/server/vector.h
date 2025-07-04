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

#pragma once

#include <string.h>
#include <fmt/core.h>

//=========================================================
// 2DVector - used for many pathfinding and many other
// operations that are treated as planar rather than 3d.
//=========================================================
class Vector2D
{
public:
    constexpr Vector2D() = default;
    constexpr Vector2D(const Vector2D&) = default;
    constexpr Vector2D& operator=(const Vector2D&) = default;

    constexpr Vector2D(float X, float Y)
        : x( X ),
          y( Y )
    {
    }

    [[nodiscard]] constexpr Vector2D operator+( const Vector2D& v ) const { return Vector2D( x + v.x, y + v.y ); }
    [[nodiscard]] constexpr Vector2D operator-( const Vector2D& v ) const { return Vector2D( x - v.x, y - v.y ); }
    [[nodiscard]] constexpr Vector2D operator*( float fl ) const { return Vector2D( x * fl, y * fl ); }
    [[nodiscard]] constexpr Vector2D operator/( float fl ) const { return Vector2D( x / fl, y / fl ); }

    [[nodiscard]] float Length() const { return static_cast<float>( sqrt( x * x + y * y ) ); }

    [[nodiscard]] Vector2D Normalize() const
    {
        float flLen = Length();
        if( flLen == 0 )
        {
            return Vector2D( 0, 0 );
        }
        else
        {
            flLen = 1 / flLen;
            return Vector2D( x * flLen, y * flLen );
        }
    }

    vec_t x = 0, y = 0;
};

[[nodiscard]] constexpr float DotProduct( const Vector2D& a, const Vector2D& b )
{
    return ( a.x * b.x + a.y * b.y );
}

[[nodiscard]] constexpr Vector2D operator*( float fl, const Vector2D& v )
{
    return v * fl;
}

//=========================================================
// 3D Vector
//=========================================================
class Vector // same data-layout as engine's vec3_t,
{             //        which is a vec_t[3]
public:
    // Construction/destruction
    constexpr Vector() = default;
    constexpr Vector(const Vector&) = default;
    constexpr Vector& operator=(const Vector&) = default;

    constexpr Vector(float X, float Y, float Z)
        : x( X ),
          y( Y ),
          z( Z )
    {
    }

    // Operators
    [[nodiscard]] constexpr Vector operator-() const { return Vector( -x, -y, -z ); }
    [[nodiscard]] constexpr bool operator==( const Vector& v ) const { return x == v.x && y == v.y && z == v.z; }
    [[nodiscard]] constexpr bool operator!=( const Vector& v ) const { return !( *this == v ); }
    [[nodiscard]] constexpr Vector operator+( const Vector& v ) const { return Vector( x + v.x, y + v.y, z + v.z ); }
    [[nodiscard]] constexpr Vector operator-( const Vector& v ) const { return Vector( x - v.x, y - v.y, z - v.z ); }
    [[nodiscard]] constexpr Vector operator*( float fl ) const { return Vector( x * fl, y * fl, z * fl ); }
    [[nodiscard]] constexpr Vector operator/( float fl ) const { return Vector( x / fl, y / fl, z / fl ); }

    // Methods
    constexpr void CopyToArray(float* rgfl) const { rgfl[0] = x, rgfl[1] = y, rgfl[2] = z; }

    [[nodiscard]] constexpr float LengthSquared() const { return x * x + y * y + z * z; }
    [[nodiscard]] float Length() const { return static_cast<float>( sqrt( LengthSquared() ) ); }
    [[nodiscard]] constexpr operator float*() { return &x; }             // Vectors will now automatically convert to float * when needed
    [[nodiscard]] constexpr operator const float*() const { return &x; } // Vectors will now automatically convert to float * when needed

    [[nodiscard]] Vector Normalize() const
    {
        float flLen = Length();
        if( flLen == 0 )
            return Vector( 0, 0, 1 ); // ????
        flLen = 1 / flLen;
        return Vector( x * flLen, y * flLen, z * flLen );
    }

    [[nodiscard]] constexpr Vector2D Make2D() const
    {
        return {x, y};
    }

    [[nodiscard]] float Length2D() const { return static_cast<float>( sqrt( x * x + y * y ) ); }

    /**
     *    @brief Creates a string from the Vector members
     *    @param Digits How many digits to print. 0 = to integer, -1 = whole float
     */
    [[nodiscard]] std::string MakeString( int Digits = -1 ) const
    {
        switch( Digits )
        {
            case -1:
                return fmt::format( "{} {} {}", x, y, z );
            case 0:
                return fmt::format( "{} {} {}", static_cast<int>(x), static_cast<int>(y), static_cast<int>(z) );
            default:
                return fmt::format( fmt::format( "{{:.{}f}} {{:.{}f}} {{:.{}f}}", Digits, Digits, Digits ), x, y, z );
        }
    }

    /**
     * @brief Update this vector with the given @c str string
     * @param delimiter Delimiter on the string to split numbers
     * @return number of occurences (X, Y, Z) 3 or 0 if completely failed.
     */
    int FromString( std::string_view str, std::string_view delimiter = " " )
    {
        x = y = z = 0;

        int parsed = 0;
        size_t start = 0, end;
        std::vector<std::string_view> tokens;

        while( ( end = str.find( delimiter, start ) ) != std::string_view::npos )
        {
            if( end != start )
            {
                tokens.emplace_back( str.substr( start, end - start ) );
            }

            start = end + delimiter.length();
        }

        if( start < str.size() )
        {
            tokens.emplace_back( str.substr( start ) );
        }

        vec_t* components[3] = { &x, &y, &z };

        for( size_t i = 0; i < tokens.size() && i < 3; ++i )
        {
            try
            {
                *components[i] = std::stof( std::string( tokens[i] ) );
                ++parsed;
            }
            catch ( ... ) { }
        }

        return parsed;
    }

    // Members
    vec_t x = 0, y = 0, z = 0;
};

[[nodiscard]] constexpr Vector operator*( float fl, const Vector& v )
{
    return v * fl;
}

[[nodiscard]] constexpr float DotProduct( const Vector& a, const Vector& b )
{
    return ( a.x * b.x + a.y * b.y + a.z * b.z );
}

[[nodiscard]] constexpr Vector CrossProduct( const Vector& a, const Vector& b )
{
    return Vector( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
}
