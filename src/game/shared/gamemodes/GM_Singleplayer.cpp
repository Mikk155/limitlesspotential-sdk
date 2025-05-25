/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#include "GM_Singleplayer.h"

void GM_Singleplayer::OnRegister()
{
#ifdef CLIENT_DLL
#else
    // Define this as a dummy command to silence console errors.
    m_VModEnableCommand = g_ClientCommands.CreateScoped( "vmodenable", []( auto, const auto& ) {} );
#endif

    BaseClass::OnRegister();
}

void GM_Singleplayer::OnUnRegister()
{
#ifdef CLIENT_DLL
#else
    m_VModEnableCommand.Remove();
#endif

    BaseClass::OnUnRegister();
}
