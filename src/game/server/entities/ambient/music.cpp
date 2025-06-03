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
#include "PlayerList.h"

enum class AmbientMusicCommand
{
    Play = 0,
    Loop,
    Fadeout,
    Stop,
    Toggle,
    ToggleFadeout,
    ToggleLoop,
    // Used for CAmbientMusic::KeyValue
    End
};

class CAmbientMusic : public CAmbient
{
    DECLARE_CLASS( CAmbientMusic, CAmbient );
    DECLARE_DATAMAP();

    public:
        bool Spawn() override;
        bool KeyValue( KeyValueData* pkvd ) override;
        void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

        void PlayMusic( CBasePlayer* player, AmbientMusicCommand command );

    private:
        AmbientMusicCommand m_Command = AmbientMusicCommand::Play;
};

BEGIN_DATAMAP( CAmbientMusic )
    DEFINE_FIELD( m_Command, FIELD_INTEGER ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( ambient_music, CAmbientMusic );

bool CAmbientMusic::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "m_Command" ) )
    {
        m_Command = static_cast<AmbientMusicCommand>( atoi( pkvd->szValue ) );

        if( AmbientMusicCommand val = std::clamp( m_Command, AmbientMusicCommand::Play, AmbientMusicCommand::End );
            val != m_Command || val == AmbientMusicCommand::End ) {
            Logger->warn( "Invalid ambient_music command {}, falling back to \"Stop\"", static_cast<int>( m_Command ) );
            m_Command = AmbientMusicCommand::Stop;
        }
    }
    else
    {
        return BaseClass::KeyValue( pkvd );
    }

    return true;
}

void CAmbientMusic::PlayMusic( CBasePlayer* player, AmbientMusicCommand command )
{
    if( player != nullptr )
    {
        switch( command )
        {
            case AmbientMusicCommand::Play:
            {
                CLIENT_COMMAND( player->edict(), "%s", fmt::format( "music play \"{}\"\n", STRING( m_sPlaySound ) ).c_str() );
                break;
            }
            case AmbientMusicCommand::Loop:
            {
                CLIENT_COMMAND( player->edict(), "%s", fmt::format( "music loop \"{}\"\n", STRING( m_sPlaySound ) ).c_str() );
                break;
            }
            case AmbientMusicCommand::Fadeout:
            {
                CLIENT_COMMAND( player->edict(), "music fadeout\n" );
                break;
            }
            case AmbientMusicCommand::Toggle:
            {
                if( g_PlayerList.toggle(player, this) ) {
                    PlayMusic(player, AmbientMusicCommand::Play);
                } else {
                    PlayMusic(player, AmbientMusicCommand::Stop);
                }
                break;
            }
            case AmbientMusicCommand::ToggleFadeout:
            {
                if( g_PlayerList.toggle(player, this) ) {
                    PlayMusic(player, AmbientMusicCommand::Play);
                } else {
                    PlayMusic(player, AmbientMusicCommand::Fadeout);
                }
                break;
            }
            case AmbientMusicCommand::ToggleLoop:
            {
                if( g_PlayerList.toggle(player, this) ) {
                    PlayMusic(player, AmbientMusicCommand::Loop);
                } else {
                    PlayMusic(player, AmbientMusicCommand::Stop);
                }
                break;
            }
            default:
            case AmbientMusicCommand::Stop:
            {
                CLIENT_COMMAND( player->edict(), "music stop\n" );
                break;
            }
        }
    }
}

void CAmbientMusic::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( ( pev->spawnflags & AMBIENT_ACTIVATOR_ONLY ) != 0 )
    {
        if( pActivator != nullptr && pActivator->IsPlayer() )
        {
            PlayMusic( static_cast<CBasePlayer*>( pActivator ), m_Command );
        }
    }
    else
    {
        for( CBasePlayer* player : UTIL_FindPlayers() )
        {
            PlayMusic( player, m_Command );
        }
    }

    BaseClass::Use( pActivator, pCaller, useType, value );
}

bool CAmbientMusic::Spawn()
{
    Precache();
    return BaseClass::Spawn();
}
