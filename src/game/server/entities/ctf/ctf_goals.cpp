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

#include "cbase.h"
#include "basemonster.h"
#include "CHalfLifeCTFplay.h"
#include "UserMessages.h"
#include "ctf_goals.h"

BEGIN_DATAMAP( CTFGoal )
    DEFINE_FUNCTION( PlaceGoal ),
END_DATAMAP();

bool CTFGoal::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( "goal_no", pkvd->szKeyName ) )
    {
        m_iGoalNum = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( "goal_min", pkvd->szKeyName ) )
    {
        Vector tmp;
        UTIL_StringToVector( tmp, pkvd->szValue );
        if( tmp != g_vecZero )
            m_GoalMin = tmp;

        return true;
    }
    else if( FStrEq( "goal_max", pkvd->szKeyName ) )
    {
        Vector tmp;
        UTIL_StringToVector( tmp, pkvd->szValue );
        if( tmp != g_vecZero )
            m_GoalMax = tmp;

        return true;
    }

    return false;
}

SpawnAction CTFGoal::Spawn()
{
    if( !FStringNull( pev->model ) )
    {
        const char* modelName = STRING( pev->model );

        if( *modelName == '*' )
            pev->effects |= EF_NODRAW;

        PrecacheModel( modelName );
        SetModel( STRING( pev->model ) );
    }

    pev->solid = SOLID_TRIGGER;

    if( 0 == m_iGoalState )
        m_iGoalState = 1;

    SetOrigin( pev->origin );

    SetThink( &CTFGoal::PlaceGoal );
    pev->nextthink = gpGlobals->time + 0.2;

    return SpawnAction::Spawn;
}

void CTFGoal::SetObjectCollisionBox()
{
    if( *STRING( pev->model ) == '*' )
    {
        float max = 0;
        for( int i = 0; i < 3; ++i )
        {
            float v = fabs( pev->mins[i] );
            if( v > max )
                max = v;
            v = fabs( pev->maxs[i] );
            if( v > max )
                max = v;
        }

        for( int i = 0; i < 3; ++i )
        {
            pev->absmin[i] = pev->origin[i] - max;
            pev->absmax[i] = pev->origin[i] + max;
        }

        pev->absmin.x -= 1.0;
        pev->absmin.y -= 1.0;
        pev->absmin.z -= 1.0;
        pev->absmax.x += 1.0;
        pev->absmax.y += 1.0;
        pev->absmax.z += 1.0;
    }
    else
    {
        pev->absmin = pev->origin + Vector( -24, -24, 0 );
        pev->absmax = pev->origin + Vector( 24, 24, 16 );
    }
}

void CTFGoal::StartGoal()
{
    SetThink( &CTFGoal::PlaceGoal );
    pev->nextthink = gpGlobals->time + 0.2;
}

void CTFGoal::PlaceGoal()
{
    pev->movetype = MOVETYPE_NONE;
    pev->velocity = g_vecZero;
    pev->oldorigin = pev->origin;
}

BEGIN_DATAMAP( CTFGoalBase )
    DEFINE_FUNCTION( BaseThink ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( item_ctfbase, CTFGoalBase );

void CTFGoalBase::BaseThink()
{
    Vector vecLightPos, vecLightAng;
    GetAttachment( 0, vecLightPos, vecLightAng );

    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    g_engfuncs.pfnWriteByte( TE_ELIGHT );
    g_engfuncs.pfnWriteShort( entindex() + 0x1000 );
    g_engfuncs.pfnWriteCoord( vecLightPos.x );
    g_engfuncs.pfnWriteCoord( vecLightPos.y );
    g_engfuncs.pfnWriteCoord( vecLightPos.z );
    g_engfuncs.pfnWriteCoord( 128 );

    if( m_iGoalNum == 1 )
    {
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    else
    {
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
    }

    g_engfuncs.pfnWriteByte( 255 );
    g_engfuncs.pfnWriteCoord( 0 );
    g_engfuncs.pfnMessageEnd();
    pev->nextthink = gpGlobals->time + 20.0;
}

SpawnAction CTFGoalBase::Spawn()
{
    pev->movetype = MOVETYPE_TOSS;
    pev->solid = SOLID_NOT;
    SetOrigin( pev->origin );

    Vector vecMin;
    Vector vecMax;

    vecMax.x = 16;
    vecMax.y = 16;
    vecMax.z = 16;
    vecMin.x = -16;
    vecMin.y = -16;
    vecMin.z = 0;
    SetSize( vecMin, vecMax );

    if( 0 == g_engfuncs.pfnDropToFloor( edict() ) )
    {
        CBaseEntity::Logger->error( "Item {} fell out of level at {}", STRING( pev->classname ), pev->origin.MakeString() );
        return SpawnAction::DelayRemove;
    }

    if( !FStringNull( pev->model ) )
    {
        PrecacheModel( STRING( pev->model ) );
        SetModel( STRING( pev->model ) );

        pev->sequence = LookupSequence( "on_ground" );

        if( pev->sequence != -1 )
        {
            ResetSequenceInfo();
            pev->frame = 0;
        }
    }

    SetThink( &CTFGoalBase::BaseThink );
    pev->nextthink = gpGlobals->time + 0.1;

    return SpawnAction::Spawn;
}

void CTFGoalBase::TurnOnLight( CBasePlayer* pPlayer )
{
    Vector vecLightPos, vecLightAng;
    GetAttachment( 0, vecLightPos, vecLightAng );
    MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, nullptr, pPlayer );
    g_engfuncs.pfnWriteByte( TE_ELIGHT );

    g_engfuncs.pfnWriteShort( entindex() + 0x1000 );
    g_engfuncs.pfnWriteCoord( vecLightPos.x );
    g_engfuncs.pfnWriteCoord( vecLightPos.y );
    g_engfuncs.pfnWriteCoord( vecLightPos.z );
    g_engfuncs.pfnWriteCoord( 128 );

    if( m_iGoalNum == 1 )
    {
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    else
    {
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    g_engfuncs.pfnWriteByte( 255 );
    g_engfuncs.pfnWriteCoord( 0 );
    g_engfuncs.pfnMessageEnd();
}

void DumpCTFFlagInfo( CBasePlayer* pPlayer )
{
    if( g_GameMode->IsGamemode( "ctf"sv ) )
    {
        for( auto entity : UTIL_FindEntitiesByClassname<CTFGoalFlag>( "item_ctfflag" ) )
        {
            entity->DisplayFlagStatus( pPlayer );
        }
    }
}

BEGIN_DATAMAP( CTFGoalFlag )
    DEFINE_FUNCTION( PlaceItem ),
    DEFINE_FUNCTION( ReturnFlag ),
    DEFINE_FUNCTION( FlagCarryThink ),
    DEFINE_FUNCTION( goal_item_dropthink ),
    DEFINE_FUNCTION( ReturnFlagThink ),
    DEFINE_FUNCTION( ScoreFlagTouch ),
    DEFINE_FUNCTION( goal_item_touch ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( item_ctfflag, CTFGoalFlag );

void CTFGoalFlag::Precache()
{
    if( !FStringNull( pev->model ) )
    {
        PrecacheModel( STRING( pev->model ) );
    }

    PrecacheSound( "ctf/bm_flagtaken.wav" );
    PrecacheSound( "ctf/soldier_flagtaken.wav" );
    PrecacheSound( "ctf/marine_flag_capture.wav" );
    PrecacheSound( "ctf/civ_flag_capture.wav" );
}

void CTFGoalFlag::PlaceItem()
{
    pev->velocity = g_vecZero;
    pev->movetype = MOVETYPE_NONE;
    pev->oldorigin = pev->origin;
}

void CTFGoalFlag::ReturnFlag()
{
    if( pev->owner )
    {
        auto player = ToBasePlayer( pev->owner );
        player->m_pFlag = nullptr;
        player->m_iItems = static_cast<CTFItem::CTFItem>( player->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) );
    }

    SetTouch( nullptr );
    SetThink( nullptr );

    pev->nextthink = gpGlobals->time;

    m_iGoalState = 1;
    pev->solid = SOLID_TRIGGER;
    pev->movetype = MOVETYPE_NONE;
    pev->aiment = nullptr;

    pev->angles = m_OriginalAngles;
    pev->effects |= EF_NODRAW;
    pev->origin = pev->oldorigin;

    SetOrigin( pev->origin );

    if( !FStringNull( pev->model ) )
    {
        pev->sequence = LookupSequence( "flag_positioned" );
        if( pev->sequence != -1 )
        {
            ResetSequenceInfo();
            pev->frame = 0;
        }
    }

    SetThink( &CTFGoalFlag::ReturnFlagThink );

    pev->nextthink = gpGlobals->time + 0.25;

    for( auto entity : UTIL_FindEntitiesByClassname<CTFGoalBase>( "item_ctfbase" ) )
    {
        if( entity->m_iGoalNum == m_iGoalNum )
        {
            entity->pev->skin = 0;
            break;
        }
    }
}

void CTFGoalFlag::FlagCarryThink()
{
    Vector vecLightPos, vecLightAng;
    GetAttachment( 0, vecLightPos, vecLightAng );

    MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
    g_engfuncs.pfnWriteByte( TE_ELIGHT );
    g_engfuncs.pfnWriteShort( entindex() + 0x1000 );
    g_engfuncs.pfnWriteCoord( vecLightPos.x );
    g_engfuncs.pfnWriteCoord( vecLightPos.y );
    g_engfuncs.pfnWriteCoord( vecLightPos.z );
    g_engfuncs.pfnWriteCoord( 128 );

    if( m_iGoalNum == 1 )
    {
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    else
    {
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    g_engfuncs.pfnWriteByte( 255 );
    g_engfuncs.pfnWriteCoord( 0 );
    g_engfuncs.pfnMessageEnd();

    auto owner = ToBasePlayer( pev->owner );

    bool hasFlag = false;

    if( owner )
    {
        if( ( owner->m_iItems & ( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) != 0 )
        {
            hasFlag = true;

            bool flagFound = false;

            for( auto entity : UTIL_FindEntitiesByClassname<CTFGoalFlag>( "item_ctfflag" ) )
            {
                if( static_cast<CTFTeam>( entity->m_iGoalNum ) == owner->m_iTeamNum )
                {
                    flagFound = entity->m_iGoalState == 1;
                    break;
                }
            }

            if( flagFound )
            {
                ClientPrint( owner, HUD_PRINTCENTER, "#CTFPickUpFlagP" );
            }
            else
            {
                ClientPrint( owner, HUD_PRINTCENTER, "#CTFPickUpFlagG" );
            }

            pev->nextthink = gpGlobals->time + 20.0;
        }
        else
        {
            owner->m_iClientItems = CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag;
        }
    }

    if( !hasFlag )
    {
        ReturnFlag();
    }
}

void CTFGoalFlag::goal_item_dropthink()
{
    pev->movetype = MOVETYPE_TOSS;
    pev->aiment = nullptr;

    int contents = UTIL_PointContents( pev->origin );

    if( contents == CONTENTS_SOLID || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA || contents == CONTENTS_SKY )
    {
        ReturnFlag();
    }
    else
    {
        SetThink( &CTFGoalFlag::ReturnFlag );

        pev->nextthink = gpGlobals->time + std::max( 0.f, g_flFlagReturnTime - 5 );
    }
}

SpawnAction CTFGoalFlag::Spawn()
{
    Precache();

    if( 0 != m_iGoalNum )
    {
        pev->movetype = MOVETYPE_TOSS;
        pev->solid = SOLID_TRIGGER;
        SetOrigin( pev->origin );

        Vector vecMax, vecMin;
        vecMax.x = 16;
        vecMax.y = 16;
        vecMax.z = 72;
        vecMin.x = -16;
        vecMin.y = -16;
        vecMin.z = 0;
        SetSize( vecMin, vecMax );

        if( 0 == g_engfuncs.pfnDropToFloor( edict() ) )
        {
            CBaseEntity::Logger->error( "Item {} fell out of level at {}", STRING( pev->classname ), pev->origin.MakeString() );
            return SpawnAction::DelayRemove;
        }

        if( !FStringNull( pev->model ) )
        {
            SetModel( STRING( pev->model ) );

            pev->sequence = LookupSequence( "flag_positioned" );

            if( pev->sequence != -1 )
            {
                ResetSequenceInfo();
                pev->frame = 0;
            }
        }

        m_iGoalState = 1;
        pev->solid = SOLID_TRIGGER;

        if( FStringNull( pev->netname ) )
        {
            pev->netname = MAKE_STRING( "goalflag" );
        }

        SetOrigin( pev->origin );

        m_OriginalAngles = pev->angles;

        SetTouch( &CTFGoalFlag::goal_item_touch );
        SetThink( &CTFGoalFlag::PlaceItem );

        pev->nextthink = gpGlobals->time + 0.2;
    }
    else
    {
        CBaseEntity::Logger->error( "Invalid goal_no set for CTF flag" );
    }

    return SpawnAction::Spawn;
}

void CTFGoalFlag::ReturnFlagThink()
{
    pev->effects &= ~EF_NODRAW;

    SetThink( nullptr );
    SetTouch( &CTFGoalFlag::goal_item_touch );

    const char* name = "";

    switch ( m_iGoalNum )
    {
    case 1:
        name = "ReturnedBlackMesaFlag";
        break;
    case 2:
        name = "ReturnedOpposingForceFlag";
        break;
    }

    CGameRules::Logger->trace( "World triggered \"{}\"", name );
    DisplayTeamFlags( nullptr );
}

void CTFGoalFlag::StartItem()
{
    SetThink( &CTFGoalFlag::PlaceItem );
    pev->nextthink = gpGlobals->time + 0.2;
}

void CTFGoalFlag::ScoreFlagTouch( CBasePlayer* pPlayer )
{
    if( pPlayer && ( m_iGoalNum == 1 || m_iGoalNum == 2 ) )
    {
        for( int i = 1; i <= gpGlobals->maxClients; ++i )
        {
            auto player = UTIL_PlayerByIndex( i );

            if( !player )
            {
                continue;
            }

            if( ( player->pev->flags & FL_SPECTATOR ) != 0 )
            {
                continue;
            }

            if( player->m_iTeamNum == pPlayer->m_iTeamNum )
            {
                if( static_cast<int>( player->m_iTeamNum ) == m_iGoalNum )
                {
                    if( pPlayer == player )
                        ClientPrint( player, HUD_PRINTCENTER, "#CTFCaptureFlagP" );
                    else
                        ClientPrint( player, HUD_PRINTCENTER, "#CTFCaptureFlagT" );
                }
                else if( pPlayer == player )
                {
                    bool foundFlag = false;

                    for( auto entity : UTIL_FindEntitiesByClassname<CTFGoalFlag>( "item_ctfflag" ) )
                    {
                        if( entity->m_iGoalNum == static_cast<int>( pPlayer->m_iTeamNum ) )
                        {
                            foundFlag = entity->m_iGoalState == 1;
                            break;
                        }
                    }

                    if( foundFlag )
                    {
                        ClientPrint( player, HUD_PRINTCENTER, "#CTFPickUpFlagP" );
                    }
                    else
                    {
                        ClientPrint( player, HUD_PRINTCENTER, "#CTFPickUpFlagG" );
                    }
                }
                else
                {
                    ClientPrint( player, HUD_PRINTCENTER, "#CTFPickUpFlagT" );
                }
            }
            else if( static_cast<int>( pPlayer->m_iTeamNum ) == m_iGoalNum )
            {
                ClientPrint( player, HUD_PRINTCENTER, "#CTFFlagCaptured" );
            }
            else
            {
                ClientPrint( player, HUD_PRINTCENTER, "#CTFLoseFlag" );
            }
        }

        if( m_iGoalNum == 1 )
        {
            if( pPlayer->m_iTeamNum == CTFTeam::BlackMesa )
            {
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagScorerOF" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "\n" );

                CGameRules::Logger->trace( "{} triggered \"CapturedFlag\"", PlayerLogInfo{*pPlayer} );

                ++teamscores[static_cast<int>( pPlayer->m_iTeamNum ) - 1];

                ++pPlayer->m_iFlagCaptures;
                pPlayer->m_iCTFScore += 10;
                pPlayer->m_iOffense += 10;

                MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
                g_engfuncs.pfnWriteByte( pPlayer->entindex() );
                g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
                g_engfuncs.pfnMessageEnd();

                ClientPrint( pPlayer, HUD_PRINTTALK, "#CTFScorePoints" );
                pPlayer->SendScoreInfoAll();

                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFTeamBM" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( ": %d\n", teamscores[0] ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFTeamOF" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( ": %d\n", teamscores[1] ) );

                if( m_nReturnPlayer > 0 && g_flCaptureAssistTime > gpGlobals->time - m_flReturnTime )
                {
                    auto returnPlayer = UTIL_PlayerByIndex( m_nReturnPlayer );
                    ++pPlayer->m_iCTFScore;
                    ++pPlayer->m_iOffense;
                    MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
                    g_engfuncs.pfnWriteByte( pPlayer->entindex() );
                    g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
                    g_engfuncs.pfnMessageEnd();

                    ClientPrint( returnPlayer, HUD_PRINTTALK, "#CTFScorePoint" );
                    UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s", STRING( returnPlayer->pev->netname ) ) );
                    UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagAssistBM" );
                }
            }
            else
            {
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagGetBM" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "\n" );

                CGameRules::Logger->trace( "{} triggered \"TookFlag\"", PlayerLogInfo{*pPlayer} );
            }
        }
        else
        {
            if( static_cast<int>( pPlayer->m_iTeamNum ) == m_iGoalNum )
            {
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagScorerBM" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "\n" );

                CGameRules::Logger->trace( "{} triggered \"CapturedFlag\"", PlayerLogInfo{*pPlayer} );

                ++teamscores[static_cast<int>( pPlayer->m_iTeamNum ) - 1];

                ++pPlayer->m_iFlagCaptures;
                pPlayer->m_iCTFScore += 10;
                pPlayer->m_iOffense += 10;

                MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
                g_engfuncs.pfnWriteByte( pPlayer->entindex() );
                g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
                g_engfuncs.pfnMessageEnd();

                ClientPrint( pPlayer, HUD_PRINTTALK, "#CTFScorePoints" );

                pPlayer->SendScoreInfoAll();

                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFTeamBM" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( ": %d\n", teamscores[0] ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFTeamOF" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( ": %d\n", teamscores[1] ) );

                if( m_nReturnPlayer > 0 && g_flCaptureAssistTime > gpGlobals->time - m_flReturnTime )
                {
                    auto returnPlayer = UTIL_PlayerByIndex( m_nReturnPlayer );

                    ++pPlayer->m_iCTFScore;
                    ++pPlayer->m_iOffense;

                    MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
                    g_engfuncs.pfnWriteByte( pPlayer->entindex() );
                    g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
                    g_engfuncs.pfnMessageEnd();

                    ClientPrint( returnPlayer, HUD_PRINTTALK, "#CTFScorePoint" );
                    UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s", STRING( returnPlayer->pev->netname ) ) );
                    UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagAssistOF" );
                }
            }
            else
            {
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagGetOF" );
                UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "\n" );

                CGameRules::Logger->trace( "{} triggered \"TookFlag\"", PlayerLogInfo{*pPlayer} );
            }
        }
    }

    DisplayTeamFlags( nullptr );
}

void CTFGoalFlag::TurnOnLight( CBasePlayer* pPlayer )
{
    Vector vecLightPos, vecLightAng;
    GetAttachment( 0, vecLightPos, vecLightAng );

    MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, nullptr, pPlayer );
    g_engfuncs.pfnWriteByte( TE_ELIGHT );
    g_engfuncs.pfnWriteShort( entindex() + 0x1000 );
    g_engfuncs.pfnWriteCoord( vecLightPos.x );
    g_engfuncs.pfnWriteCoord( vecLightPos.y );
    g_engfuncs.pfnWriteCoord( vecLightPos.z );
    g_engfuncs.pfnWriteCoord( 128.0 );

    if( m_iGoalNum == 1 )
    {
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 128 );
    }
    else
    {
        g_engfuncs.pfnWriteByte( 128 );
        g_engfuncs.pfnWriteByte( 255 );
        g_engfuncs.pfnWriteByte( 128 );
    }

    g_engfuncs.pfnWriteByte( 255 );
    g_engfuncs.pfnWriteCoord( 0 );
    g_engfuncs.pfnMessageEnd();
}

void CTFGoalFlag::GiveFlagToPlayer( CBasePlayer* pPlayer )
{
    pPlayer->m_pFlag = this;

    if( m_iGoalNum == 1 )
    {
        pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::BlackMesaFlag );
    }
    else if( m_iGoalNum == 2 )
    {
        pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems | CTFItem::OpposingForceFlag );
    }

    SetTouch( nullptr );
    SetThink( nullptr );

    pev->nextthink = gpGlobals->time;
    pev->owner = pPlayer->edict();
    pev->movetype = MOVETYPE_FOLLOW;
    pev->aiment = pPlayer->edict();

    if( !FStringNull( pev->model ) )
    {
        pev->sequence = LookupSequence( "carried" );

        if( pev->sequence != -1 )
        {
            ResetSequenceInfo();
            pev->frame = 0;
        }
    }
    pev->effects &= ~EF_NODRAW;

    pev->solid = SOLID_NOT;

    m_iGoalState = 2;

    if( pPlayer->m_iTeamNum == CTFTeam::BlackMesa )
    {
        EmitSound( CHAN_STATIC, "ctf/bm_flagtaken.wav", VOL_NORM, ATTN_NONE );
    }
    else if( pPlayer->m_iTeamNum == CTFTeam::OpposingForce )
    {
        EmitSound( CHAN_STATIC, "ctf/soldier_flagtaken.wav", VOL_NORM, ATTN_NONE );
    }

    ScoreFlagTouch( pPlayer );

    SetThink( &CTFGoalFlag::FlagCarryThink );
    pev->nextthink = gpGlobals->time + 0.1;

    ++pPlayer->m_iCTFScore;
    ++pPlayer->m_iOffense;

    MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
    g_engfuncs.pfnWriteByte( pPlayer->entindex() );
    g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
    g_engfuncs.pfnMessageEnd();

    ClientPrint( pPlayer, HUD_PRINTTALK, "#CTFScorePoint" );

    pPlayer->SendScoreInfoAll();

    DisplayTeamFlags( nullptr );

    for( auto entity : UTIL_FindEntitiesByClassname<CTFGoalBase>( "item_ctfbase" ) )
    {
        if( entity->m_iGoalNum == m_iGoalNum )
        {
            entity->pev->skin = 1;
            break;
        }
    }
}

void CTFGoalFlag::goal_item_touch( CBaseEntity* pOther )
{
    auto pPlayer = ToBasePlayer( pOther );

    if( !pPlayer )
        return;

    if( pPlayer->pev->health <= 0 )
        return;

    if( static_cast<int>( pPlayer->m_iTeamNum ) != m_iGoalNum )
    {
        GiveFlagToPlayer( pPlayer );
        return;
    }

    if( m_iGoalState == 1 )
    {
        if( ( pPlayer->m_iItems & ( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) ) == 0 )
            return;

        CTFGoalFlag* otherFlag = pPlayer->m_pFlag;

        if( otherFlag )
            otherFlag->ReturnFlag();

        if( pPlayer->m_iTeamNum == CTFTeam::BlackMesa )
        {
            EmitSound( CHAN_STATIC, "ctf/civ_flag_capture.wav", VOL_NORM, ATTN_NONE );
        }
        else if( pPlayer->m_iTeamNum == CTFTeam::OpposingForce )
        {
            EmitSound( CHAN_STATIC, "ctf/marine_flag_capture.wav", VOL_NORM, ATTN_NONE );
        }

        ScoreFlagTouch( pPlayer );
        return;
    }

    if( pPlayer->m_iTeamNum == CTFTeam::BlackMesa )
    {
        UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
        UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagReturnBM" );

        CGameRules::Logger->trace( "{} triggered \"ReturnedFlag\"", PlayerLogInfo{*pPlayer} );
    }
    else if( pPlayer->m_iTeamNum == CTFTeam::OpposingForce )
    {
        UTIL_ClientPrintAll( HUD_PRINTNOTIFY, STRING( pPlayer->pev->netname ) );
        UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#CTFFlagReturnOF" );

        CGameRules::Logger->trace( "{} triggered \"ReturnedFlag\"", PlayerLogInfo{*pPlayer} );
    }

    for( int i = 1; i <= gpGlobals->maxClients; ++i )
    {
        auto player = UTIL_PlayerByIndex( i );

        if( !player || ( player->pev->flags & FL_SPECTATOR ) != 0 )
        {
            continue;
        }

        if( static_cast<int>( player->m_iTeamNum ) == m_iGoalNum )
        {
            ClientPrint( player, HUD_PRINTCENTER, "#CTFFlagReturnedT" );
        }
        else
        {
            ClientPrint( player, HUD_PRINTCENTER, "#CTFFlagReturnedE" );
        }
    }

    ++pPlayer->m_iCTFScore;
    ++pPlayer->m_iDefense;

    MESSAGE_BEGIN( MSG_ALL, gmsgCTFScore );
    g_engfuncs.pfnWriteByte( pPlayer->entindex() );
    g_engfuncs.pfnWriteByte( pPlayer->m_iCTFScore );
    g_engfuncs.pfnMessageEnd();

    ClientPrint( pPlayer, HUD_PRINTTALK, "#CTFScorePoint" );

    pPlayer->SendScoreInfoAll();

    ReturnFlag();

    m_nReturnPlayer = pPlayer->entindex();
    m_flReturnTime = gpGlobals->time;
}

void CTFGoalFlag::SetDropTouch()
{
    SetTouch( &CTFGoalFlag::goal_item_touch );
    SetThink( &CTFGoalFlag::goal_item_dropthink );
    pev->nextthink = gpGlobals->time + 4.5;
}

void CTFGoalFlag::DoDrop( const Vector& vecOrigin )
{
    pev->flags &= ~FL_ONGROUND;
    pev->origin = vecOrigin;
    SetOrigin( pev->origin );

    pev->angles = g_vecZero;

    pev->solid = SOLID_TRIGGER;
    pev->effects &= ~EF_NODRAW;
    pev->movetype = MOVETYPE_TOSS;

    pev->velocity = g_vecZero;
    pev->velocity.z = -100;

    SetTouch( &CTFGoalFlag::goal_item_touch );
    SetThink( &CTFGoalFlag::goal_item_dropthink );
    pev->nextthink = gpGlobals->time + 5.0;
}

void CTFGoalFlag::DropFlag( CBasePlayer* pPlayer )
{
    pPlayer->m_pFlag = nullptr;
    pPlayer->m_iItems = static_cast<CTFItem::CTFItem>( pPlayer->m_iItems & ~( CTFItem::BlackMesaFlag | CTFItem::OpposingForceFlag ) );

    pev->solid = SOLID_TRIGGER;
    pev->movetype = MOVETYPE_NONE;
    pev->aiment = nullptr;
    pev->owner = nullptr;

    if( !FStringNull( pev->model ) )
    {
        pev->sequence = LookupSequence( "not_carried" );

        if( pev->sequence != -1 )
        {
            ResetSequenceInfo();
            pev->frame = 0;
        }
    }

    const float height = ( pev->flags & FL_DUCKING ) != 0 ? 34 : 16;

    pev->origin = pPlayer->pev->origin + Vector( 0, 0, height );

    DoDrop( pev->origin );

    m_iGoalState = 3;

    DisplayTeamFlags( nullptr );
}

void CTFGoalFlag::DisplayFlagStatus( CBasePlayer* pPlayer )
{
    switch ( m_iGoalState )
    {
    case 1:
        if( m_iGoalNum == 1 )
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagAtBaseBM" );
        else
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagAtBaseOF" );
        break;

    case 2:
    {
        auto owner = CBaseEntity::Instance( pev->owner );

        if( owner )
        {
            if( m_iGoalNum == 1 )
            {
                ClientPrint( pPlayer, HUD_PRINTNOTIFY, "%s", STRING( owner->pev->netname ) );
                ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagCarriedBM" );
            }
            else
            {
                ClientPrint( pPlayer, HUD_PRINTNOTIFY, "%s", STRING( owner->pev->netname ) );
                ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagCarriedOF" );
            }
        }
        else if( m_iGoalNum == 1 )
        {
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagCarryBM" );
        }
        else
        {
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagCarryOF" );
        }
    }
    break;

    case 3:
        if( m_iGoalNum == 1 )
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagDroppedBM" );
        else
            ClientPrint( pPlayer, HUD_PRINTNOTIFY, "#CTFInfoFlagDroppedOF" );
        break;
    }
}

void CTFGoalFlag::SetObjectCollisionBox()
{
    pev->absmin = pev->origin + Vector( -16, -16, 0 );
    pev->absmax = pev->origin + Vector( 16, 16, 72 );
}
