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

// -TODO Track m_hActivator if using AMBIENT_ACTIVATOR_ONLY and send SND_ONLYHOST
// -TODO Get rid of dynpitchvol_t and MapUpgrade their presets. perphabs leave a entity block for JACK in the docs.

enum class LFO
{
    Square = 1,
    Triangle,
    Random
};

/**
 *    @brief runtime pitch shift and volume fadein/out structure
 *    @details NOTE: IF YOU CHANGE THIS STRUCT YOU MUST CHANGE THE SAVE/RESTORE VERSION NUMBER
 *    SEE BELOW (in the typedescription for the class)
 */
struct dynpitchvol_t
{
    // NOTE: do not change the order of these parameters
    // NOTE: unless you also change order of rgdpvpreset array elements!
    int preset;

    int pitchrun;    // pitch shift % when sound is running 0 - 255
    int pitchstart; // pitch shift % when sound stops or starts 0 - 255
    int spinup;        // spinup time 0 - 100
    int spindown;    // spindown time 0 - 100

    int volrun;      // volume change % when sound is running 0 - 10
    int volstart; // volume change % when sound stops or starts 0 - 10
    int fadein;      // volume fade in time 0 - 100
    int fadeout;  // volume fade out time 0 - 100

    // Low Frequency Oscillator
    int lfotype; // 0) off 1) square 2) triangle 3) random
    int lforate; // 0 - 1000, how fast lfo osciallates

    int lfomodpitch; // 0-100 mod of current pitch. 0 is off.
    int lfomodvol;     // 0-100 mod of current volume. 0 is off.

    int cspinup; // each trigger hit increments counter and spinup pitch


    int cspincount;

    int pitch;
    int spinupsav;
    int spindownsav;
    int pitchfrac;

    int vol;
    int fadeinsav;
    int fadeoutsav;
    int volfrac;

    int lfofrac;
    int lfomult;
};

#define CDPVPRESETMAX 27

/**
 *    @brief presets for runtime pitch and vol modulation of ambient sounds
 */
dynpitchvol_t rgdpvpreset[CDPVPRESETMAX] =
    {
        // pitch    pstart    spinup    spindwn    volrun    volstrt    fadein    fadeout    lfotype    lforate    modptch modvol    cspnup
        {1, 255, 75, 95, 95, 10, 1, 50, 95, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {2, 255, 85, 70, 88, 10, 1, 20, 88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 255, 100, 50, 75, 10, 1, 10, 75, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {4, 100, 100, 0, 0, 10, 1, 90, 90, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {5, 100, 100, 0, 0, 10, 1, 80, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {6, 100, 100, 0, 0, 10, 1, 50, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {7, 100, 100, 0, 0, 5, 1, 40, 50, 1, 50, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {8, 100, 100, 0, 0, 5, 1, 40, 50, 1, 150, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {9, 100, 100, 0, 0, 5, 1, 40, 50, 1, 750, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {10, 128, 100, 50, 75, 10, 1, 30, 40, 2, 8, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {11, 128, 100, 50, 75, 10, 1, 30, 40, 2, 25, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {12, 128, 100, 50, 75, 10, 1, 30, 40, 2, 70, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {13, 50, 50, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {14, 70, 70, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {15, 90, 90, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {16, 120, 120, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {17, 180, 180, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {18, 255, 255, 0, 0, 10, 1, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {19, 200, 75, 90, 90, 10, 1, 50, 90, 2, 100, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {20, 255, 75, 97, 90, 10, 1, 50, 90, 1, 40, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {21, 100, 100, 0, 0, 10, 1, 30, 50, 3, 15, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {22, 160, 160, 0, 0, 10, 1, 50, 50, 3, 500, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {23, 255, 75, 88, 0, 10, 1, 40, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {24, 200, 20, 95, 70, 10, 1, 70, 70, 3, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {25, 180, 100, 50, 60, 10, 1, 40, 60, 2, 90, 100, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {26, 60, 60, 0, 0, 10, 1, 40, 70, 3, 80, 20, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {27, 128, 90, 10, 10, 10, 1, 20, 40, 1, 5, 10, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

/**
 *    @brief general-purpose user-defined static sound
 */
class CAmbientGeneric : public CAmbient
{
    DECLARE_CLASS( CAmbientGeneric, CAmbient );
    DECLARE_DATAMAP();

    public:
        void Think() override;
        SpawnAction Spawn() override;
        void Precache() override;
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

        /**
         *    @brief Init all ramp params in preparation to play a new sound
         */
        void InitModulationParms();

    private:
        bool m_bStartOff;
        float m_flAttenuation; // attenuation value
        bool m_fActive;     // only true when the entity is playing a looping sound
        bool m_fLooping; // true when the sound played will loop
        dynpitchvol_t m_dpv;
};

BEGIN_DATAMAP( CAmbientGeneric )
    // HACKHACK - This is not really in the spirit of the save/restore design, but save this
    // out as a binary data block.  If the dynpitchvol_t is changed, old saved games will NOT
    // load these correctly, so bump the save/restore version if you change the size of the struct
    // The right way to do this is to split the input parms (read in keyvalue) into members and re-init this
    // struct in Precache(), but it's unlikely that the struct will change, so it's not worth the time right now.
    DEFINE_ARRAY( m_dpv, FIELD_CHARACTER, sizeof( dynpitchvol_t ) ),
    DEFINE_FIELD( m_flAttenuation, FIELD_FLOAT ),
    DEFINE_FIELD( m_fActive, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fLooping, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_bStartOff, FIELD_BOOLEAN ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( ambient_generic, CAmbientGeneric );

SpawnAction CAmbientGeneric::Spawn()
{
    const char* szSoundFile = STRING( m_sPlaySound );

    if( FStringNull( m_sPlaySound ) || strlen( szSoundFile ) < 1 )
    {
        Logger->error( "EMPTY AMBIENT AT: {}", pev->origin.MakeString() );
        return SpawnAction::DelayRemove;
    }

    pev->nextthink = 0;

    Precache();

    if( !m_bStartOff )
    {
        // start the sound ASAP
        if( m_fLooping )
            m_fActive = true;
    }
    if( m_fActive )
    {
        EmitAmbientSound( PlayFromEntity( nullptr, nullptr ), szSoundFile,
            ( m_dpv.vol * 0.01 ), m_flAttenuation, SND_SPAWNING, m_dpv.pitch );

        pev->nextthink = gpGlobals->time + 0.1;
    }
    return BaseClass::Spawn();
}

void CAmbientGeneric::Precache()
{
    // init all dynamic modulation parms
    InitModulationParms();

    BaseClass::Precache();
}

/**
 *    @brief Think at 5hz if we are dynamically modifying pitch or volume of the playing sound.
 *    This function will ramp pitch and/or volume up or down, modify pitch/volume with lfo if active.
 */
void CAmbientGeneric::Think()
{
    const char* szSoundFile = STRING( m_sPlaySound );
    int pitch = m_dpv.pitch;
    int vol = m_dpv.vol;
    int flags = 0;
    bool fChanged = false; // false if pitch and vol remain unchanged this round
    int prev;

    if( 0 == m_dpv.spinup && 0 == m_dpv.spindown && 0 == m_dpv.fadein && 0 == m_dpv.fadeout && 0 == m_dpv.lfotype )
        return; // no ramps or lfo, stop thinking

    // ==============
    // pitch envelope
    // ==============
    if( 0 != m_dpv.spinup || 0 != m_dpv.spindown )
    {
        prev = m_dpv.pitchfrac >> 8;

        if( m_dpv.spinup > 0 )
            m_dpv.pitchfrac += m_dpv.spinup;
        else if( m_dpv.spindown > 0 )
            m_dpv.pitchfrac -= m_dpv.spindown;

        pitch = m_dpv.pitchfrac >> 8;

        if( pitch > m_dpv.pitchrun )
        {
            pitch = m_dpv.pitchrun;
            m_dpv.spinup = 0; // done with ramp up
        }

        if( pitch < m_dpv.pitchstart )
        {
            pitch = m_dpv.pitchstart;
            m_dpv.spindown = 0; // done with ramp down

            // shut sound off
            EmitAmbientSound( PlayFromEntity( nullptr, nullptr ), szSoundFile,
                0, 0, SND_STOP, 0 );

            // return without setting nextthink
            return;
        }

        if( pitch > 255 )
            pitch = 255;
        if( pitch < 1 )
            pitch = 1;

        m_dpv.pitch = pitch;

        fChanged |= ( prev != pitch );
        flags |= SND_CHANGE_PITCH;
    }

    // ==================
    // amplitude envelope
    // ==================
    if( 0 != m_dpv.fadein || 0 != m_dpv.fadeout )
    {
        prev = m_dpv.volfrac >> 8;

        if( m_dpv.fadein > 0 )
            m_dpv.volfrac += m_dpv.fadein;
        else if( m_dpv.fadeout > 0 )
            m_dpv.volfrac -= m_dpv.fadeout;

        vol = m_dpv.volfrac >> 8;

        if( vol > m_dpv.volrun )
        {
            vol = m_dpv.volrun;
            m_dpv.fadein = 0; // done with ramp up
        }

        if( vol < m_dpv.volstart )
        {
            vol = m_dpv.volstart;
            m_dpv.fadeout = 0; // done with ramp down

            // shut sound off
            EmitAmbientSound( PlayFromEntity( nullptr, nullptr ), szSoundFile,
                0, 0, SND_STOP, 0 );

            // return without setting nextthink
            return;
        }

        if( vol > 100 )
            vol = 100;
        if( vol < 1 )
            vol = 1;

        m_dpv.vol = vol;

        fChanged |= ( prev != vol );
        flags |= SND_CHANGE_VOL;
    }

    // ===================
    // pitch/amplitude LFO
    // ===================
    if( 0 != m_dpv.lfotype )
    {
        int pos;

        if( m_dpv.lfofrac > 0x6fffffff )
            m_dpv.lfofrac = 0;

        // update lfo, lfofrac/255 makes a triangle wave 0-255
        m_dpv.lfofrac += m_dpv.lforate;
        pos = m_dpv.lfofrac >> 8;

        if( m_dpv.lfofrac < 0 )
        {
            m_dpv.lfofrac = 0;
            m_dpv.lforate = abs( m_dpv.lforate );
            pos = 0;
        }
        else if( pos > 255 )
        {
            pos = 255;
            m_dpv.lfofrac = ( 255 << 8 );
            m_dpv.lforate = -abs( m_dpv.lforate );
        }

        switch ( static_cast<LFO>( m_dpv.lfotype ) )
        {
        case LFO::Square:
            if( pos < 128 )
                m_dpv.lfomult = 255;
            else
                m_dpv.lfomult = 0;

            break;
        case LFO::Random:
            if( pos == 255 )
                m_dpv.lfomult = RANDOM_LONG( 0, 255 );
            break;
        case LFO::Triangle:
        default:
            m_dpv.lfomult = pos;
            break;
        }

        if( 0 != m_dpv.lfomodpitch )
        {
            prev = pitch;

            // pitch 0-255
            pitch += ( ( m_dpv.lfomult - 128 ) * m_dpv.lfomodpitch ) / 100;

            if( pitch > 255 )
                pitch = 255;
            if( pitch < 1 )
                pitch = 1;


            fChanged |= ( prev != pitch );
            flags |= SND_CHANGE_PITCH;
        }

        if( 0 != m_dpv.lfomodvol )
        {
            // vol 0-100
            prev = vol;

            vol += ( ( m_dpv.lfomult - 128 ) * m_dpv.lfomodvol ) / 100;

            if( vol > 100 )
                vol = 100;
            if( vol < 0 )
                vol = 0;

            fChanged |= ( prev != vol );
            flags |= SND_CHANGE_VOL;
        }
    }

    // Send update to playing sound only if we actually changed
    // pitch or volume in this routine.

    if( 0 != flags && fChanged )
    {
        if( pitch == PITCH_NORM )
            pitch = PITCH_NORM + 1; // don't send 'no pitch' !

            EmitAmbientSound( PlayFromEntity( nullptr, nullptr ), szSoundFile,
                ( vol * 0.01 ), m_flAttenuation, flags, pitch );
    }

    // update ramps at 5hz
    pev->nextthink = gpGlobals->time + 0.2;
    return;
}

void CAmbientGeneric::InitModulationParms()
{
    int pitchinc;

    m_dpv.volrun = pev->health * 10; // 0 - 100
    if( m_dpv.volrun > 100 )
        m_dpv.volrun = 100;
    if( m_dpv.volrun < 0 )
        m_dpv.volrun = 0;

    // get presets
    if( m_dpv.preset != 0 && m_dpv.preset <= CDPVPRESETMAX )
    {
        // load preset values
        m_dpv = rgdpvpreset[m_dpv.preset - 1];

        // fixup preset values, just like
        // fixups in KeyValue routine.
        if( m_dpv.spindown > 0 )
            m_dpv.spindown = ( 101 - m_dpv.spindown ) * 64;
        if( m_dpv.spinup > 0 )
            m_dpv.spinup = ( 101 - m_dpv.spinup ) * 64;

        m_dpv.volstart *= 10;
        m_dpv.volrun *= 10;

        if( m_dpv.fadein > 0 )
            m_dpv.fadein = ( 101 - m_dpv.fadein ) * 64;
        if( m_dpv.fadeout > 0 )
            m_dpv.fadeout = ( 101 - m_dpv.fadeout ) * 64;

        m_dpv.lforate *= 256;

        m_dpv.fadeinsav = m_dpv.fadein;
        m_dpv.fadeoutsav = m_dpv.fadeout;
        m_dpv.spinupsav = m_dpv.spinup;
        m_dpv.spindownsav = m_dpv.spindown;
    }

    m_dpv.fadein = m_dpv.fadeinsav;
    m_dpv.fadeout = 0;

    if( 0 != m_dpv.fadein )
        m_dpv.vol = m_dpv.volstart;
    else
        m_dpv.vol = m_dpv.volrun;

    m_dpv.spinup = m_dpv.spinupsav;
    m_dpv.spindown = 0;

    if( 0 != m_dpv.spinup )
        m_dpv.pitch = m_dpv.pitchstart;
    else
        m_dpv.pitch = m_dpv.pitchrun;

    if( m_dpv.pitch == 0 )
        m_dpv.pitch = PITCH_NORM;

    m_dpv.pitchfrac = m_dpv.pitch << 8;
    m_dpv.volfrac = m_dpv.vol << 8;

    m_dpv.lfofrac = 0;
    m_dpv.lforate = abs( m_dpv.lforate );

    m_dpv.cspincount = 1;

    if( 0 != m_dpv.cspinup )
    {
        pitchinc = ( 255 - m_dpv.pitchstart ) / m_dpv.cspinup;

        m_dpv.pitchrun = m_dpv.pitchstart + pitchinc;
        if( m_dpv.pitchrun > 255 )
            m_dpv.pitchrun = 255;
    }

    if( ( 0 != m_dpv.spinupsav || 0 != m_dpv.spindownsav || ( 0 != m_dpv.lfotype && 0 != m_dpv.lfomodpitch ) ) && ( m_dpv.pitch == PITCH_NORM ) )
        m_dpv.pitch = PITCH_NORM + 1; // must never send 'no pitch' as first pitch
                                      // if we intend to pitch shift later!
}

/**
 *    @brief turns an ambient sound on or off.
 *    If the ambient is a looping sound, mark sound as active (m_fActive) if it's playing, innactive if not.
 *    If the sound is not a looping sound, never mark it as active.
 */
void CAmbientGeneric::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    const char* szSoundFile = STRING( m_sPlaySound );
    float fraction;

    if( useType != USE_TOGGLE )
    {
        if( ( m_fActive && useType == USE_ON ) || ( !m_fActive && useType == USE_OFF ) )
            return;
    }
    // Directly change pitch if arg passed. Only works if sound is already playing.

    if( useType == USE_SET && m_fActive ) // Momentary buttons will pass down a float in here
    {
        fraction = ( value.Float.has_value() ? std::clamp( 0.01f, 1.0f, *value.Float ) : 1.0f );

        m_dpv.pitch = fraction * 255;

        EmitAmbientSound( PlayFromEntity( pActivator, pCaller ), szSoundFile, 0, 0, SND_CHANGE_PITCH, m_dpv.pitch );

        return;
    }

    // Toggle

    // m_fActive is true only if a looping sound is playing.

    if( m_fActive )
    { // turn sound off

        if( 0 != m_dpv.cspinup )
        {
            // Don't actually shut off. Each toggle causes
            // incremental spinup to max pitch

            if( m_dpv.cspincount <= m_dpv.cspinup )
            {
                int pitchinc;

                // start a new spinup
                m_dpv.cspincount++;

                pitchinc = ( 255 - m_dpv.pitchstart ) / m_dpv.cspinup;

                m_dpv.spinup = m_dpv.spinupsav;
                m_dpv.spindown = 0;

                m_dpv.pitchrun = m_dpv.pitchstart + pitchinc * m_dpv.cspincount;
                if( m_dpv.pitchrun > 255 )
                    m_dpv.pitchrun = 255;

                pev->nextthink = gpGlobals->time + 0.1;
            }
        }
        else
        {
            m_fActive = false;

            // HACKHACK - this makes the code in Precache() work properly after a save/restore
            m_bStartOff = true;

            if( 0 != m_dpv.spindownsav || 0 != m_dpv.fadeoutsav )
            {
                // spin it down (or fade it) before shutoff if spindown is set
                m_dpv.spindown = m_dpv.spindownsav;
                m_dpv.spinup = 0;

                m_dpv.fadeout = m_dpv.fadeoutsav;
                m_dpv.fadein = 0;
                pev->nextthink = gpGlobals->time + 0.1;
            }
            else
                EmitAmbientSound( PlayFromEntity( pActivator, pCaller ), szSoundFile,
                    0, 0, SND_STOP, 0 );
        }
    }
    else
    { // turn sound on

        // only toggle if this is a looping sound.  If not looping, each
        // trigger will cause the sound to play.  If the sound is still
        // playing from a previous trigger press, it will be shut off
        // and then restarted.

        if( m_fLooping )
            m_fActive = true;
        else // shut sound off now - may be interrupting a long non-looping sound
            EmitAmbientSound( PlayFromEntity( pActivator, pCaller ), szSoundFile, 0, 0, SND_STOP, 0 );

        // init all ramp params for startup

        InitModulationParms();

        EmitAmbientSound( PlayFromEntity( pActivator, pCaller ), szSoundFile,
            ( m_dpv.vol * 0.01 ), m_flAttenuation, 0, m_dpv.pitch );

        pev->nextthink = gpGlobals->time + 0.1;
    }

    BaseClass::Use( pActivator, pCaller, useType, value );
}

bool CAmbientGeneric::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_sPlaySound" ) )
    {
        m_sPlaySound = ALLOC_STRING( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_flAttenuation" ) )
    {
        m_flAttenuation = atof( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "m_fLooping" ) )
    {
        m_fLooping = ( atoi( pkvd->szValue ) == 1 );
    }
    else if( FStrEq( pkvd->szKeyName, "m_bStartOff" ) )
    {
        m_bStartOff = ( atoi( pkvd->szValue ) == 1 );
    }
    else if( FStrEq( pkvd->szKeyName, "preset" ) )
    {
        m_dpv.preset = atoi( pkvd->szValue );
    }
    else if( FStrEq( pkvd->szKeyName, "pitch" ) )
    {
        m_dpv.pitchrun = atoi( pkvd->szValue );

        if( m_dpv.pitchrun > 255 )
            m_dpv.pitchrun = 255;
        if( m_dpv.pitchrun < 0 )
            m_dpv.pitchrun = 0;
    }
    else if( FStrEq( pkvd->szKeyName, "pitchstart" ) )
    {
        m_dpv.pitchstart = atoi( pkvd->szValue );

        if( m_dpv.pitchstart > 255 )
            m_dpv.pitchstart = 255;
        if( m_dpv.pitchstart < 0 )
            m_dpv.pitchstart = 0;
    }
    else if( FStrEq( pkvd->szKeyName, "spinup" ) )
    {
        m_dpv.spinup = atoi( pkvd->szValue );

        if( m_dpv.spinup > 100 )
            m_dpv.spinup = 100;
        if( m_dpv.spinup < 0 )
            m_dpv.spinup = 0;

        if( m_dpv.spinup > 0 )
            m_dpv.spinup = ( 101 - m_dpv.spinup ) * 64;
        m_dpv.spinupsav = m_dpv.spinup;
    }
    else if( FStrEq( pkvd->szKeyName, "spindown" ) )
    {
        m_dpv.spindown = atoi( pkvd->szValue );

        if( m_dpv.spindown > 100 )
            m_dpv.spindown = 100;
        if( m_dpv.spindown < 0 )
            m_dpv.spindown = 0;

        if( m_dpv.spindown > 0 )
            m_dpv.spindown = ( 101 - m_dpv.spindown ) * 64;
        m_dpv.spindownsav = m_dpv.spindown;
    }
    else if( FStrEq( pkvd->szKeyName, "volstart" ) )
    {
        m_dpv.volstart = atoi( pkvd->szValue );

        if( m_dpv.volstart > 10 )
            m_dpv.volstart = 10;
        if( m_dpv.volstart < 0 )
            m_dpv.volstart = 0;

        m_dpv.volstart *= 10; // 0 - 100
    }
    else if( FStrEq( pkvd->szKeyName, "fadein" ) )
    {
        m_dpv.fadein = atoi( pkvd->szValue );

        if( m_dpv.fadein > 100 )
            m_dpv.fadein = 100;
        if( m_dpv.fadein < 0 )
            m_dpv.fadein = 0;

        if( m_dpv.fadein > 0 )
            m_dpv.fadein = ( 101 - m_dpv.fadein ) * 64;
        m_dpv.fadeinsav = m_dpv.fadein;
    }
    else if( FStrEq( pkvd->szKeyName, "fadeout" ) )
    {
        m_dpv.fadeout = atoi( pkvd->szValue );

        if( m_dpv.fadeout > 100 )
            m_dpv.fadeout = 100;
        if( m_dpv.fadeout < 0 )
            m_dpv.fadeout = 0;

        if( m_dpv.fadeout > 0 )
            m_dpv.fadeout = ( 101 - m_dpv.fadeout ) * 64;
        m_dpv.fadeoutsav = m_dpv.fadeout;
    }
    else if( FStrEq( pkvd->szKeyName, "lfotype" ) )
    {
        m_dpv.lfotype = atoi( pkvd->szValue );
        if( m_dpv.lfotype > 4 )
            m_dpv.lfotype = static_cast<int>( LFO::Triangle );
    }
    else if( FStrEq( pkvd->szKeyName, "lforate" ) )
    {
        m_dpv.lforate = atoi( pkvd->szValue );

        if( m_dpv.lforate > 1000 )
            m_dpv.lforate = 1000;
        if( m_dpv.lforate < 0 )
            m_dpv.lforate = 0;

        m_dpv.lforate *= 256;
    }
    else if( FStrEq( pkvd->szKeyName, "lfomodpitch" ) )
    {
        m_dpv.lfomodpitch = atoi( pkvd->szValue );
        if( m_dpv.lfomodpitch > 100 )
            m_dpv.lfomodpitch = 100;
        if( m_dpv.lfomodpitch < 0 )
            m_dpv.lfomodpitch = 0;
    }
    else if( FStrEq( pkvd->szKeyName, "lfomodvol" ) )
    {
        m_dpv.lfomodvol = atoi( pkvd->szValue );
        if( m_dpv.lfomodvol > 100 )
            m_dpv.lfomodvol = 100;
        if( m_dpv.lfomodvol < 0 )
            m_dpv.lfomodvol = 0;
    }
    else if( FStrEq( pkvd->szKeyName, "cspinup" ) )
    {
        m_dpv.cspinup = atoi( pkvd->szValue );
        if( m_dpv.cspinup > 100 )
            m_dpv.cspinup = 100;
        if( m_dpv.cspinup < 0 )
            m_dpv.cspinup = 0;
    }
    else
    {
        return CBaseEntity::KeyValue( pkvd );
    }
    return true;
}
