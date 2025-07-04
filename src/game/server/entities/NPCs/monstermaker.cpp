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

// Monstermaker spawnflags
#define SF_MONSTERMAKER_START_ON 1      //!< start active ( if has targetname )
#define SF_MONSTERMAKER_CYCLIC 4      //!< drop one monster every time fired.
#define SF_MONSTERMAKER_MONSTERCLIP 8 //!< Children are blocked by monsterclip

/**
 *    @brief Amount of @c monstermaker child keys that can be passed on to children on creation.
 */
constexpr int MaxMonsterMakerChildKeys = 64;

/**
 *    @brief this entity creates monsters during the game.
 */
class CMonsterMaker : public CBaseMonster
{
    DECLARE_CLASS( CMonsterMaker, CBaseMonster );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;
    bool KeyValue( KeyValueData* pkvd ) override;
    bool IsMonster() override { return false; }

    /**
     *    @brief activates/deactivates the monster maker
     */
    void ToggleUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );

    /**
     *    @brief drops one monster from the monstermaker each time we call this.
     */
    void CyclicUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );

    /**
     *    @brief creates a new monster every so often
     */
    void MakerThink();

    /**
     *    @brief monster maker children use this to tell the monster maker that they have died.
     */
    void DeathNotice( CBaseEntity* child ) override;

    /**
     *    @brief this is the code that drops the monster
     */
    void MakeMonster();

    string_t m_iszMonsterClassname; // classname of the monster(s) that will be created.

    int m_cNumMonsters; // max number of monsters this ent can create


    int m_cLiveChildren;    // how many monsters made by this monster maker that are currently alive
    int m_iMaxLiveChildren; // max number of monsters that this maker may have out at one time.

    float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

    bool m_fActive;
    bool m_fFadeChildren; // should we make the children fadeout?

    string_t m_ChildKeys[MaxMonsterMakerChildKeys];
    string_t m_ChildValues[MaxMonsterMakerChildKeys];

    int m_ChildKeyCount = 0;
};

LINK_ENTITY_TO_CLASS( monstermaker, CMonsterMaker );

BEGIN_DATAMAP( CMonsterMaker )
    DEFINE_FIELD( m_iszMonsterClassname, FIELD_STRING ),
    DEFINE_FIELD( m_cNumMonsters, FIELD_INTEGER ),
    DEFINE_FIELD( m_cLiveChildren, FIELD_INTEGER ),
    DEFINE_FIELD( m_flGround, FIELD_FLOAT ),
    DEFINE_FIELD( m_iMaxLiveChildren, FIELD_INTEGER ),
    DEFINE_FIELD( m_fActive, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_fFadeChildren, FIELD_BOOLEAN ),
    DEFINE_ARRAY( m_ChildKeys, FIELD_STRING, MaxMonsterMakerChildKeys ),
    DEFINE_ARRAY( m_ChildValues, FIELD_STRING, MaxMonsterMakerChildKeys ),
    DEFINE_FIELD( m_ChildKeyCount, FIELD_INTEGER ),
    DEFINE_FUNCTION( ToggleUse ),
    DEFINE_FUNCTION( CyclicUse ),
    DEFINE_FUNCTION( MakerThink ),
END_DATAMAP();

bool CMonsterMaker::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "monstercount" ) )
    {
        m_cNumMonsters = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "m_imaxlivechildren" ) )
    {
        m_iMaxLiveChildren = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( pkvd->szKeyName, "monstertype" ) )
    {
        m_iszMonsterClassname = ALLOC_STRING( pkvd->szValue );
        return true;
    }
    // Pass this key on to children.
    else if( pkvd->szKeyName[0] == '#' )
    {
        if( m_ChildKeyCount < MaxMonsterMakerChildKeys )
        {
            m_ChildKeys[m_ChildKeyCount] = ALLOC_STRING( pkvd->szKeyName + 1 );
            m_ChildValues[m_ChildKeyCount] = ALLOC_STRING( pkvd->szValue );
            ++m_ChildKeyCount;
        }
        else
        {
            AILogger->warn( "{}:{} ({}): Too many child keys specified (> {})",
                GetClassname(), entindex(), GetTargetname(), MaxMonsterMakerChildKeys );
        }

        return true;
    }

    return CBaseMonster::KeyValue( pkvd );
}

SpawnAction CMonsterMaker::Spawn()
{
    pev->solid = SOLID_NOT;

    m_cLiveChildren = 0;
    Precache();
    if( !FStringNull( pev->targetname ) )
    {
        if( ( pev->spawnflags & SF_MONSTERMAKER_CYCLIC ) != 0 )
        {
            SetUse( &CMonsterMaker::CyclicUse ); // drop one monster each time we fire
        }
        else
        {
            SetUse( &CMonsterMaker::ToggleUse ); // so can be turned on/off
        }

        if( FBitSet( pev->spawnflags, SF_MONSTERMAKER_START_ON ) )
        { // start making monsters as soon as monstermaker spawns
            m_fActive = true;
            SetThink( &CMonsterMaker::MakerThink );
        }
        else
        { // wait to be activated.
            m_fActive = false;
            SetThink( nullptr );
        }
    }
    else
    { // no targetname, just start.
        pev->nextthink = gpGlobals->time + m_flDelay;
        m_fActive = true;
        SetThink( &CMonsterMaker::MakerThink );
    }

    if( m_cNumMonsters == 1 )
    {
        m_fFadeChildren = false;
    }
    else
    {
        m_fFadeChildren = true;
    }

    m_flGround = 0;

    return SpawnAction::Spawn;
}

void CMonsterMaker::Precache()
{
    CBaseMonster::Precache();

    UTIL_PrecacheOther( STRING( m_iszMonsterClassname ), m_ChildKeys, m_ChildValues, m_ChildKeyCount );
}

void CMonsterMaker::MakeMonster()
{
    if( m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren )
    { // not allowed to make a new one yet. Too many live ones out right now.
        return;
    }

    if( 0 == m_flGround )
    {
        // set altitude. Now that I'm activated, any breakables, etc should be out from under me.
        TraceResult tr;

        UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 2048 ), ignore_monsters, edict(), &tr );
        m_flGround = tr.vecEndPos.z;
    }

    Vector mins = pev->origin - Vector( 34, 34, 0 );
    Vector maxs = pev->origin + Vector( 34, 34, 0 );
    maxs.z = pev->origin.z;
    mins.z = m_flGround;

    CBaseEntity* pList[2];
    int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, FL_CLIENT | FL_MONSTER );
    if( 0 != count )
    {
        // don't build a stack of monsters!
        return;
    }

    auto entity = g_EntityDictionary->Create( STRING( m_iszMonsterClassname ) );

    if( FNullEnt( entity ) )
    {
        AILogger->debug( "nullptr Ent in MonsterMaker!" );
        return;
    }

    entity->pev->origin = pev->origin;
    entity->pev->angles = pev->angles;
    SetBits( entity->pev->spawnflags, SF_MONSTER_FALL_TO_GROUND );

    // Children hit monsterclip brushes
    if( ( pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP ) != 0 )
        SetBits( entity->pev->spawnflags, SF_MONSTER_HITMONSTERCLIP );

    // Pass any keyvalues specified by the designer.
    UTIL_InitializeKeyValues( entity, m_ChildKeys, m_ChildValues, m_ChildKeyCount );

    DispatchSpawn( entity->edict() );
    entity->pev->owner = edict();

    if( !FStringNull( pev->netname ) )
    {
        // if I have a netname (overloaded), give the child monster that name as a targetname
        entity->pev->targetname = pev->netname;
    }

    ++m_cLiveChildren; // count this monster

    if( m_cNumMonsters != -1 )
    {
        --m_cNumMonsters;

        if( m_cNumMonsters == 0 )
        {
            // Disable this forever.  Don't kill it because it still gets death notices
            SetThink( nullptr );
            SetUse( nullptr );
        }
    }

    // If I have a target, fire!
    // Do this after the monster has spawned and this monstermaker has finished so we can access the target entity.
    if( !FStringNull( pev->target ) )
    {
        // delay already overloaded for this entity, so can't call SUB_UseTargets()
        FireTargets( STRING( pev->target ), this, this, USE_TOGGLE );
    }
}

void CMonsterMaker::CyclicUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    MakeMonster();
}

void CMonsterMaker::ToggleUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    if( !ShouldToggle( useType, m_fActive ) )
        return;

    if( m_fActive )
    {
        m_fActive = false;
        SetThink( nullptr );
    }
    else
    {
        m_fActive = true;
        SetThink( &CMonsterMaker::MakerThink );
    }

    pev->nextthink = gpGlobals->time;
}

void CMonsterMaker::MakerThink()
{
    pev->nextthink = gpGlobals->time + m_flDelay;

    MakeMonster();
}

void CMonsterMaker::DeathNotice( CBaseEntity* child )
{
    // ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
    --m_cLiveChildren;

    if( !m_fFadeChildren )
    {
        child->pev->owner = nullptr;
    }
}
