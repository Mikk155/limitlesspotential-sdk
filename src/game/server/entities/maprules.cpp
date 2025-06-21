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

/**
 *    @file
 *
 *    This module contains entities for implementing/changing game rules dynamically within each map (.BSP)
 */

#include "cbase.h"
#include "maprules.h"

class CRuleEntity : public CBaseEntity
{
    DECLARE_CLASS( CRuleEntity, CBaseEntity );

public:
    SpawnAction Spawn() override;

protected:
    bool CanFireForActivator( CBaseEntity* pActivator );
};

SpawnAction CRuleEntity::Spawn()
{
    pev->solid = SOLID_NOT;
    pev->movetype = MOVETYPE_NONE;
    pev->effects = EF_NODRAW;

    return SpawnAction::Spawn;
}

bool CRuleEntity::CanFireForActivator( CBaseEntity* pActivator )
{
    return !( IsLockedByMaster( pActivator ) );
}

/**
 *    @brief base class for all rule "point" entities (not brushes)
 */
class CRulePointEntity : public CRuleEntity
{
public:
    SpawnAction Spawn() override;
};

SpawnAction CRulePointEntity::Spawn()
{
    CRuleEntity::Spawn();
    pev->frame = 0;
    pev->model = string_t::Null;

    return SpawnAction::Spawn;
}

/**
 *    @brief base class for all rule "brush" entities (not brushes)
 *    Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
 */
class CRuleBrushEntity : public CRuleEntity
{
public:
    SpawnAction Spawn() override;

private:
};

SpawnAction CRuleBrushEntity::Spawn()
{
    SetModel( STRING( pev->model ) );
    CRuleEntity::Spawn();

    return SpawnAction::Spawn;
}

#define SF_SCORE_NEGATIVE 0x0001
#define SF_SCORE_TEAM 0x0002

/**
 *    @brief award points to player / team
 *    Points +/- total
 */
class CGameScore : public CRulePointEntity
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    bool KeyValue( KeyValueData* pkvd ) override;

    inline int Points() { return pev->frags; }
    inline bool AllowNegativeScore() { return ( pev->spawnflags & SF_SCORE_NEGATIVE ) != 0; }
    inline bool AwardToTeam() { return ( pev->spawnflags & SF_SCORE_TEAM ) != 0; }

    inline void SetPoints( int points ) { pev->frags = points; }

private:
};

LINK_ENTITY_TO_CLASS( game_score, CGameScore );

SpawnAction CGameScore::Spawn()
{
    return CRulePointEntity::Spawn();
}

bool CGameScore::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "points" ) )
    {
        SetPoints( atoi( pkvd->szValue ) );
        return true;
    }

    return CRulePointEntity::KeyValue( pkvd );
}

void CGameScore::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    // Only players can use this
    if( auto player = ToBasePlayer( pActivator ); player )
    {
        if( AwardToTeam() )
        {
            player->AddPointsToTeam( Points(), AllowNegativeScore() );
        }
        else
        {
            player->AddPoints( Points(), AllowNegativeScore() );
        }
    }
}

/**
 *    @brief Ends the game in MP
 */
class CGameEnd : public CRulePointEntity
{
public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

private:
};

LINK_ENTITY_TO_CLASS( game_end, CGameEnd );

void CGameEnd::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    g_pGameRules->EndMultiplayerGame( true );
}

#define SF_ENVTEXT_ALLPLAYERS 0x0001

/**
 *    @brief NON-Localized HUD Message (use env_message to display a titles.txt message)
 */
class CGameText : public CRulePointEntity
{
    DECLARE_CLASS( CGameText, CRulePointEntity );
    DECLARE_DATAMAP();

public:
    void Precache() override;
    SpawnAction Spawn() override;

    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    bool KeyValue( KeyValueData* pkvd ) override;

    inline bool MessageToAll() { return ( pev->spawnflags & SF_ENVTEXT_ALLPLAYERS ) != 0; }
    inline void MessageSet( const char* pMessage ) { pev->message = ALLOC_STRING( pMessage ); }
    inline const char* MessageGet() { return STRING( pev->message ); }

private:
    hudtextparms_t m_textParms;
};

LINK_ENTITY_TO_CLASS( game_text, CGameText );

// Save parms as a block.
// Will break save/restore if the structure changes, but this entity didn't ship with Half-Life,
// so it can't impact saved Half-Life games.
BEGIN_DATAMAP( CGameText )
    DEFINE_ARRAY( m_textParms, FIELD_CHARACTER, sizeof( hudtextparms_t ) ),
END_DATAMAP();

void CGameText::Precache()
{
    CRulePointEntity::Precache();

    // Re-allocate the message to handle escape characters
    if( !FStringNull( pev->message ) )
    {
        pev->message = ALLOC_ESCAPED_STRING( STRING( pev->message ) );
    }
}

SpawnAction CGameText::Spawn()
{
    Precache();

    return CRulePointEntity::Spawn();
}

bool CGameText::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "channel" ) )
    {
        m_textParms.channel = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "x" ) )
    {
        m_textParms.x = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "y" ) )
    {
        m_textParms.y = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "effect" ) )
    {
        m_textParms.effect = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "color" ) )
    {
        int color[4];
        UTIL_StringToIntArray( color, 4, pkvd->szValue );
        m_textParms.r1 = color[0];
        m_textParms.g1 = color[1];
        m_textParms.b1 = color[2];
        m_textParms.a1 = color[3];
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "color2" ) )
    {
        int color[4];
        UTIL_StringToIntArray( color, 4, pkvd->szValue );
        m_textParms.r2 = color[0];
        m_textParms.g2 = color[1];
        m_textParms.b2 = color[2];
        m_textParms.a2 = color[3];
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "fadein" ) )
    {
        m_textParms.fadeinTime = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "fadeout" ) )
    {
        m_textParms.fadeoutTime = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "holdtime" ) )
    {
        m_textParms.holdTime = atof( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "fxtime" ) )
    {
        m_textParms.fxTime = atof( pkvd->szValue );
        return true;
    }

    return CRulePointEntity::KeyValue( pkvd );
}

void CGameText::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    if( MessageToAll() )
    {
        UTIL_HudMessageAll( m_textParms, MessageGet() );
    }
    else
    {
        if( auto player = ToBasePlayer( pActivator ); player && player->IsNetClient() )
        {
            UTIL_HudMessage( player, m_textParms, MessageGet() );
        }
    }
}

#define SF_TEAMMASTER_FIREONCE 0x0001
#define SF_TEAMMASTER_ANYTEAM 0x0002

/**
 *    @brief "Masters" like multisource, but based on the team of the activator
 *    Only allows mastered entity to fire if the team matches my team
 *
 *    @details team index (pulled from server team list "mp_teamlist"
 *    Flag: Remove on Fire
 *    Flag: Any team until set? -- Any team can use this until the team is set (otherwise no teams can use it)
 */
class CGameTeamMaster : public CRulePointEntity
{
public:
    bool KeyValue( KeyValueData* pkvd ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    int ObjectCaps() override { return CRulePointEntity::ObjectCaps() | FCAP_MASTER; }

    bool IsTriggered( CBaseEntity* pActivator ) override;
    const char* TeamID() override;
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_TEAMMASTER_FIREONCE ) != 0; }
    inline bool AnyTeam() { return ( pev->spawnflags & SF_TEAMMASTER_ANYTEAM ) != 0; }

private:
    bool TeamMatch( CBaseEntity* pActivator );

    int m_teamIndex;
};

LINK_ENTITY_TO_CLASS( game_team_master, CGameTeamMaster );

bool CGameTeamMaster::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "teamindex" ) )
    {
        m_teamIndex = atoi( pkvd->szValue );
        return true;
    }

    return CRulePointEntity::KeyValue( pkvd );
}

void CGameTeamMaster::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    if( useType == USE_SET )
    {
        if( value.Float.has_value() && value.Float.value() < 0 )
        {
            m_teamIndex = -1;
        }
        else
        {
            m_teamIndex = g_pGameRules->GetTeamIndex( pActivator->TeamID() );
        }
        return;
    }

    if( TeamMatch( pActivator ) )
    {
        SUB_UseTargets( pActivator, m_UseType, value );
        if( RemoveOnFire() )
            UTIL_Remove( this );
    }
}

bool CGameTeamMaster::IsTriggered( CBaseEntity* pActivator )
{
    return TeamMatch( pActivator );
}

const char* CGameTeamMaster::TeamID()
{
    if( m_teamIndex < 0 ) // Currently set to "no team"
        return "";

    return g_pGameRules->GetIndexedTeamName( m_teamIndex ); // UNDONE: Fill this in with the team from the "teamlist"
}

bool CGameTeamMaster::TeamMatch( CBaseEntity* pActivator )
{
    if( m_teamIndex < 0 && AnyTeam() )
        return true;

    if( !pActivator )
        return false;

    return UTIL_TeamsMatch( pActivator->TeamID(), TeamID() );
}

#define SF_TEAMSET_FIREONCE 0x0001
#define SF_TEAMSET_CLEARTEAM 0x0002

/**
 *    @brief Changes the team of the entity it targets to the activator's team
 *    @details Flag: Fire once
 *    Flag: Clear team -- Sets the team to "NONE" instead of activator
 */
class CGameTeamSet : public CRulePointEntity
{
public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_TEAMSET_FIREONCE ) != 0; }
    inline bool ShouldClearTeam() { return ( pev->spawnflags & SF_TEAMSET_CLEARTEAM ) != 0; }

private:
};

LINK_ENTITY_TO_CLASS( game_team_set, CGameTeamSet );

void CGameTeamSet::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    if( ShouldClearTeam() )
    {
        SUB_UseTargets( pActivator, USE_SET, { .Float = -1 } );
    }
    else
    {
        SUB_UseTargets( pActivator, USE_SET );
    }

    if( RemoveOnFire() )
    {
        UTIL_Remove( this );
    }
}

#define SF_PKILL_FIREONCE 0x0001

/**
 *    @brief Damages the player who fires it
 *    @details Flag: Fire once
 */
class CGamePlayerHurt : public CRulePointEntity
{
public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_PKILL_FIREONCE ) != 0; }

private:
};

LINK_ENTITY_TO_CLASS( game_player_hurt, CGamePlayerHurt );

void CGamePlayerHurt::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    if( pActivator->IsPlayer() )
    {
        if( pev->dmg < 0 )
            pActivator->GiveHealth( -pev->dmg, DMG_GENERIC );
        else
            pActivator->TakeDamage( this, this, pev->dmg, DMG_GENERIC );
    }

    SUB_UseTargets( pActivator, useType, value );

    if( RemoveOnFire() )
    {
        UTIL_Remove( this );
    }
}

#define SF_GAMECOUNT_FIREONCE 0x0001
#define SF_GAMECOUNT_RESET 0x0002

/**
 *    @brief Counts events and fires target
 *    @details Flag: Fire once
 *    Flag: Reset on Fire
 */
class CGameCounter : public CRulePointEntity
{
public:
    SpawnAction Spawn() override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_GAMECOUNT_FIREONCE ) != 0; }
    inline bool ResetOnFire() { return ( pev->spawnflags & SF_GAMECOUNT_RESET ) != 0; }

    inline void CountUp() { pev->frags++; }
    inline void CountDown() { pev->frags--; }
    inline void ResetCount() { pev->frags = pev->dmg; }
    inline int CountValue() { return pev->frags; }
    inline int LimitValue() { return pev->health; }

    inline bool HitLimit() { return CountValue() == LimitValue(); }

private:
    inline void SetCountValue( int value ) { pev->frags = value; }
    inline void SetInitialValue( int value ) { pev->dmg = value; }
};

LINK_ENTITY_TO_CLASS( game_counter, CGameCounter );

SpawnAction CGameCounter::Spawn()
{
    // Save off the initial count
    SetInitialValue( CountValue() );
    return CRulePointEntity::Spawn();
}

void CGameCounter::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    switch ( useType )
    {
    case USE_ON:
    case USE_TOGGLE:
        CountUp();
        break;

    case USE_OFF:
        CountDown();
        break;

    case USE_SET:
        SetCountValue( value.Float.has_value() ? *value.Float : 0 );
        break;
    }

    if( HitLimit() )
    {
        SUB_UseTargets( pActivator, USE_TOGGLE );
        if( RemoveOnFire() )
        {
            UTIL_Remove( this );
        }

        if( ResetOnFire() )
        {
            ResetCount();
        }
    }
}

#define SF_GAMECOUNTSET_FIREONCE 0x0001

/**
 *    @brief Sets the counter's value
 *    @details Flag: Fire once
 */
class CGameCounterSet : public CRulePointEntity
{
public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_GAMECOUNTSET_FIREONCE ) != 0; }

private:
};

LINK_ENTITY_TO_CLASS( game_counter_set, CGameCounterSet );

void CGameCounterSet::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    SUB_UseTargets( pActivator, USE_SET, { .Float = pev->frags } );

    if( RemoveOnFire() )
    {
        UTIL_Remove( this );
    }
}

#define SF_PLAYEREQUIP_USEONLY 0x0001
#define MAX_EQUIP 32

/**
 *    @brief Sets the default player equipment
 *    Flag: USE Only
 */
class CGamePlayerEquip : public CRulePointEntity
{
    DECLARE_CLASS( CGamePlayerEquip, CRulePointEntity );
    DECLARE_DATAMAP();

public:
    bool KeyValue( KeyValueData* pkvd ) override;
    void Touch( CBaseEntity* pOther ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    inline bool UseOnly() { return ( pev->spawnflags & SF_PLAYEREQUIP_USEONLY ) != 0; }

private:
    void EquipPlayer( CBaseEntity* pPlayer );

    string_t m_weaponNames[MAX_EQUIP];
    int m_weaponCount[MAX_EQUIP];
};

BEGIN_DATAMAP( CGamePlayerEquip )
    DEFINE_ARRAY( m_weaponNames, FIELD_STRING, MAX_EQUIP ),
    DEFINE_ARRAY( m_weaponCount, FIELD_INTEGER, MAX_EQUIP ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( game_player_equip, CGamePlayerEquip );

bool CGamePlayerEquip::KeyValue( KeyValueData* pkvd )
{
    if( CRulePointEntity::KeyValue( pkvd ) )
    {
        return true;
    }

    for( int i = 0; i < MAX_EQUIP; i++ )
    {
        if( FStringNull( m_weaponNames[i] ) )
        {
            char tmp[128];

            UTIL_StripToken( pkvd->szKeyName, tmp );

            m_weaponNames[i] = ALLOC_STRING( tmp );
            m_weaponCount[i] = atoi( pkvd->szValue );
            m_weaponCount[i] = std::max( 1, m_weaponCount[i] );
            return true;
        }
    }

    return false;
}

void CGamePlayerEquip::Touch( CBaseEntity* pOther )
{
    if( !CanFireForActivator( pOther ) )
        return;

    if( UseOnly() )
        return;

    EquipPlayer( pOther );
}

void CGamePlayerEquip::EquipPlayer( CBaseEntity* pEntity )
{
    auto player = ToBasePlayer( pEntity );

    if( !player )
    {
        return;
    }

    for( int i = 0; i < MAX_EQUIP; i++ )
    {
        if( FStringNull( m_weaponNames[i] ) )
            break;
        for( int j = 0; j < m_weaponCount[i]; j++ )
        {
            player->GiveNamedItem( STRING( m_weaponNames[i] ) );
        }
    }
}

void CGamePlayerEquip::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    EquipPlayer( pActivator );
}

#define SF_PTEAM_FIREONCE 0x0001
#define SF_PTEAM_KILL 0x0002
#define SF_PTEAM_GIB 0x0004

/**
 *    @brief Changes the team of the player who fired it
 *    @details Flag: Fire once
 *    Flag: Kill Player
 *    Flag: Gib Player
 */
class CGamePlayerTeam : public CRulePointEntity
{
public:
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

private:
    inline bool RemoveOnFire() { return ( pev->spawnflags & SF_PTEAM_FIREONCE ) != 0; }
    inline bool ShouldKillPlayer() { return ( pev->spawnflags & SF_PTEAM_KILL ) != 0; }
    inline bool ShouldGibPlayer() { return ( pev->spawnflags & SF_PTEAM_GIB ) != 0; }

    const char* TargetTeamName( const char* pszTargetName );
};

LINK_ENTITY_TO_CLASS( game_player_team, CGamePlayerTeam );

const char* CGamePlayerTeam::TargetTeamName( const char* pszTargetName )
{
    CBaseEntity* pTeamEntity = nullptr;

    while( ( pTeamEntity = UTIL_FindEntityByTargetname( pTeamEntity, pszTargetName ) ) != nullptr )
    {
        if( pTeamEntity->ClassnameIs( "game_team_master" ) )
            return pTeamEntity->TeamID();
    }

    return nullptr;
}

void CGamePlayerTeam::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !CanFireForActivator( pActivator ) )
        return;

    auto player = ToBasePlayer( pActivator );

    if( player )
    {
        const char* pszTargetTeam = TargetTeamName( STRING( pev->target ) );
        if( pszTargetTeam )
        {
            g_pGameRules->ChangePlayerTeam( player, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer() );
        }
    }

    if( RemoveOnFire() )
    {
        UTIL_Remove( this );
    }
}
