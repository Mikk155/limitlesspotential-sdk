//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

struct screenfade_t
{
    // How fast to fade (tics / second) (+ fade in, - fade out)
    float fadeSpeed;
    // When the fading hits maximum
    float fadeEnd;
    // Total End Time of the fade (used for FFADE_OUT)
    float fadeTotalEnd;
    // When to reset to not fading (for fadeout and hold)
    float fadeReset;
    // Fade color
    byte fader, fadeg, fadeb, fadealpha;
    // Fading flags
    int fadeFlags;
};
