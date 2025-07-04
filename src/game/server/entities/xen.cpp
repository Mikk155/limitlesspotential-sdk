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

#define XEN_PLANT_GLOW_SPRITE "sprites/flare3.spr"
#define XEN_PLANT_HIDE_TIME 5

class CActAnimating : public CBaseAnimating
{
    DECLARE_CLASS( CActAnimating, CBaseAnimating );
    DECLARE_DATAMAP();

public:
    void SetActivity( Activity act );
    inline Activity GetActivity() { return m_Activity; }

    int ObjectCaps() override { return CBaseAnimating::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

private:
    Activity m_Activity;
};

BEGIN_DATAMAP( CActAnimating )
    DEFINE_FIELD( m_Activity, FIELD_INTEGER ),
END_DATAMAP();

void CActAnimating::SetActivity( Activity act )
{
    int sequence = LookupActivity( act );
    if( sequence != ACTIVITY_NOT_AVAILABLE )
    {
        pev->sequence = sequence;
        m_Activity = act;
        pev->frame = 0;
        ResetSequenceInfo();
    }
}

class CXenPLight : public CActAnimating
{
    DECLARE_CLASS( CXenPLight, CActAnimating );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void Touch( CBaseEntity* pOther ) override;
    void Think() override;
    void UpdateOnRemove() override;

    void LightOn();
    void LightOff();

private:
    CSprite* m_pGlow;
};

LINK_ENTITY_TO_CLASS( xen_plantlight, CXenPLight );

BEGIN_DATAMAP( CXenPLight )
    DEFINE_FIELD( m_pGlow, FIELD_CLASSPTR ),
END_DATAMAP();

void CXenPLight::OnCreate()
{
    CActAnimating::OnCreate();

    pev->model = MAKE_STRING( "models/light.mdl" );
}

SpawnAction CXenPLight::Spawn()
{
    Precache();

    SetModel( STRING( pev->model ) );
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_TRIGGER;

    SetSize( Vector( -80, -80, 0 ), Vector( 80, 80, 32 ) );
    SetActivity( ACT_IDLE );
    pev->nextthink = gpGlobals->time + 0.1;
    pev->frame = RANDOM_FLOAT( 0, 255 );

    m_pGlow = CSprite::SpriteCreate( XEN_PLANT_GLOW_SPRITE, pev->origin + Vector( 0, 0, ( pev->mins.z + pev->maxs.z ) * 0.5 ), false );
    m_pGlow->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
    m_pGlow->SetAttachment( edict(), 1 );

    return SpawnAction::Spawn;
}

void CXenPLight::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheModel( XEN_PLANT_GLOW_SPRITE );
}

void CXenPLight::Think()
{
    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.1;

    switch ( GetActivity() )
    {
    case ACT_CROUCH:
        if( m_fSequenceFinished )
        {
            SetActivity( ACT_CROUCHIDLE );
            LightOff();
        }
        break;

    case ACT_CROUCHIDLE:
        if( gpGlobals->time > pev->dmgtime )
        {
            SetActivity( ACT_STAND );
            LightOn();
        }
        break;

    case ACT_STAND:
        if( m_fSequenceFinished )
            SetActivity( ACT_IDLE );
        break;

    case ACT_IDLE:
    default:
        break;
    }
}

void CXenPLight::Touch( CBaseEntity* pOther )
{
    if( pOther->IsPlayer() )
    {
        pev->dmgtime = gpGlobals->time + XEN_PLANT_HIDE_TIME;
        if( GetActivity() == ACT_IDLE || GetActivity() == ACT_STAND )
        {
            SetActivity( ACT_CROUCH );
        }
    }
}

void CXenPLight::UpdateOnRemove()
{
    if( m_pGlow )
    {
        UTIL_Remove( m_pGlow );
        m_pGlow = nullptr;
    }

    CActAnimating::UpdateOnRemove();
}

void CXenPLight::LightOn()
{
    SUB_UseTargets( this, USE_ON );
    if( m_pGlow )
        m_pGlow->pev->effects &= ~EF_NODRAW;
}

void CXenPLight::LightOff()
{
    SUB_UseTargets( this, USE_OFF );
    if( m_pGlow )
        m_pGlow->pev->effects |= EF_NODRAW;
}

class CXenHair : public CActAnimating
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void Think() override;
};

LINK_ENTITY_TO_CLASS( xen_hair, CXenHair );

#define SF_HAIR_SYNC 0x0001

void CXenHair::OnCreate()
{
    CActAnimating::OnCreate();

    pev->model = MAKE_STRING( "models/hair.mdl" );
}

SpawnAction CXenHair::Spawn()
{
    Precache();
    SetModel( STRING( pev->model ) );
    SetSize( Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );
    pev->sequence = 0;

    if( ( pev->spawnflags & SF_HAIR_SYNC ) == 0 )
    {
        pev->frame = RANDOM_FLOAT( 0, 255 );
        pev->framerate = RANDOM_FLOAT( 0.7, 1.4 );
    }
    ResetSequenceInfo();

    pev->solid = SOLID_NOT;
    pev->movetype = MOVETYPE_NONE;
    pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.4 ); // Load balance these a bit

    return SpawnAction::Spawn;
}

void CXenHair::Think()
{
    StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.5;
}

void CXenHair::Precache()
{
    PrecacheModel( STRING( pev->model ) );
}

class CXenTreeTrigger : public CBaseEntity
{
public:
    void Touch( CBaseEntity* pOther ) override;
    static CXenTreeTrigger* TriggerCreate( CBaseEntity* owner, const Vector& position );
};

LINK_ENTITY_TO_CLASS( xen_ttrigger, CXenTreeTrigger );

CXenTreeTrigger* CXenTreeTrigger::TriggerCreate( CBaseEntity* owner, const Vector& position )
{
    CXenTreeTrigger* pTrigger = g_EntityDictionary->Create<CXenTreeTrigger>( "xen_ttrigger" );
    pTrigger->pev->origin = position;
    pTrigger->pev->solid = SOLID_TRIGGER;
    pTrigger->pev->movetype = MOVETYPE_NONE;
    pTrigger->SetOwner( owner );

    return pTrigger;
}

void CXenTreeTrigger::Touch( CBaseEntity* pOther )
{
    if( pev->owner )
    {
        CBaseEntity* pEntity = CBaseEntity::Instance( pev->owner );
        pEntity->Touch( pOther );
    }
}

#define TREE_AE_ATTACK 1

class CXenTree : public CActAnimating
{
    DECLARE_CLASS( CXenTree, CActAnimating );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void Precache() override;
    void Touch( CBaseEntity* pOther ) override;
    void Think() override;
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override
    {
        Attack();
        return false;
    }
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    void Attack();

    void UpdateOnRemove() override;

    static const char* pAttackHitSounds[];
    static const char* pAttackMissSounds[];

private:
    CXenTreeTrigger* m_pTrigger;
};

LINK_ENTITY_TO_CLASS( xen_tree, CXenTree );

BEGIN_DATAMAP( CXenTree )
    DEFINE_FIELD( m_pTrigger, FIELD_CLASSPTR ),
END_DATAMAP();

void CXenTree::OnCreate()
{
    CActAnimating::OnCreate();

    pev->model = MAKE_STRING( "models/tree.mdl" );

    SetClassification( "alien_flora" );
}

SpawnAction CXenTree::Spawn()
{
    Precache();

    SetModel( STRING( pev->model ) );
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_BBOX;

    pev->takedamage = DAMAGE_YES;

    SetSize( Vector( -30, -30, 0 ), Vector( 30, 30, 188 ) );
    SetActivity( ACT_IDLE );
    pev->nextthink = gpGlobals->time + 0.1;
    pev->frame = RANDOM_FLOAT( 0, 255 );
    pev->framerate = RANDOM_FLOAT( 0.7, 1.4 );

    Vector triggerPosition;
    AngleVectors( pev->angles, &triggerPosition, nullptr, nullptr );
    triggerPosition = pev->origin + ( triggerPosition * 64 );
    // Create the trigger
    m_pTrigger = CXenTreeTrigger::TriggerCreate( this, triggerPosition );
    m_pTrigger->SetSize( Vector( -24, -24, 0 ), Vector( 24, 24, 128 ) );

    return SpawnAction::Spawn;
}

const char* CXenTree::pAttackHitSounds[] =
    {
        "zombie/claw_strike1.wav",
        "zombie/claw_strike2.wav",
        "zombie/claw_strike3.wav",
};

const char* CXenTree::pAttackMissSounds[] =
    {
        "zombie/claw_miss1.wav",
        "zombie/claw_miss2.wav",
};

void CXenTree::Precache()
{
    PrecacheModel( STRING( pev->model ) );
    PrecacheModel( XEN_PLANT_GLOW_SPRITE );
    PRECACHE_SOUND_ARRAY( pAttackHitSounds );
    PRECACHE_SOUND_ARRAY( pAttackMissSounds );
}

void CXenTree::Touch( CBaseEntity* pOther )
{
    if( !pOther->IsPlayer() && pOther->ClassnameIs( "monster_bigmomma" ) )
        return;

    Attack();
}

void CXenTree::Attack()
{
    if( GetActivity() == ACT_IDLE )
    {
        SetActivity( ACT_MELEE_ATTACK1 );
        pev->framerate = RANDOM_FLOAT( 1.0, 1.4 );
        EMIT_SOUND_ARRAY_DYN( CHAN_WEAPON, pAttackMissSounds );
    }
}

void CXenTree::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    switch ( pEvent->event )
    {
    case TREE_AE_ATTACK:
    {
        CBaseEntity* pList[8];
        bool sound = false;
        int count = UTIL_EntitiesInBox( pList, 8, m_pTrigger->pev->absmin, m_pTrigger->pev->absmax, FL_MONSTER | FL_CLIENT );
        Vector forward;

        AngleVectors( pev->angles, &forward, nullptr, nullptr );

        for( int i = 0; i < count; i++ )
        {
            if( pList[i] != this )
            {
                if( pList[i]->pev->owner != edict() )
                {
                    sound = true;
                    pList[i]->TakeDamage( this, this, 25, DMG_CRUSH | DMG_SLASH );
                    pList[i]->pev->punchangle.x = 15;
                    pList[i]->pev->velocity = pList[i]->pev->velocity + forward * 100;
                }
            }
        }

        if( sound )
        {
            EMIT_SOUND_ARRAY_DYN( CHAN_WEAPON, pAttackHitSounds );
        }
    }
        return;
    }

    CActAnimating::HandleAnimEvent( pEvent );
}

void CXenTree::Think()
{
    float flInterval = StudioFrameAdvance();
    pev->nextthink = gpGlobals->time + 0.1;
    DispatchAnimEvents( flInterval );

    switch ( GetActivity() )
    {
    case ACT_MELEE_ATTACK1:
        if( m_fSequenceFinished )
        {
            SetActivity( ACT_IDLE );
            pev->framerate = RANDOM_FLOAT( 0.6, 1.4 );
        }
        break;

    default:
    case ACT_IDLE:
        break;
    }
}

void CXenTree::UpdateOnRemove()
{
    if( m_pTrigger )
    {
        UTIL_Remove( m_pTrigger );
        m_pTrigger = nullptr;
    }

    CActAnimating::UpdateOnRemove();
}

// UNDONE:    These need to smoke somehow when they take damage
//            Touch behavior?
//            Cause damage in smoke area

class CXenSpore : public CActAnimating
{
public:
    SpawnAction Spawn() override;
    void Precache() override;
    void Touch( CBaseEntity* pOther ) override;
    void Think() override;
    bool TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) override
    {
        Attack();
        return false;
    }
    //    void        HandleAnimEvent( MonsterEvent_t *pEvent );
    void Attack() {}

    static const char* pModelNames[];
};

class CXenSporeSmall : public CXenSpore
{
    SpawnAction Spawn() override;
};

class CXenSporeMed : public CXenSpore
{
    SpawnAction Spawn() override;
};

class CXenSporeLarge : public CXenSpore
{
    SpawnAction Spawn() override;

    static const Vector m_hullSizes[];
};

/**
 *    @brief Fake collision box for big spores
 */
class CXenHull : public CPointEntity
{
public:
    static CXenHull* CreateHull( CBaseEntity* source, const Vector& mins, const Vector& maxs, const Vector& offset );

    void OnCreate() override
    {
        BaseClass::OnCreate();

        SetClassification( "alien_flora" );
    }
};

CXenHull* CXenHull::CreateHull( CBaseEntity* source, const Vector& mins, const Vector& maxs, const Vector& offset )
{
    CXenHull* pHull = g_EntityDictionary->Create<CXenHull>( "xen_hull" );

    pHull->SetOrigin( source->pev->origin + offset );
    pHull->SetModel( STRING( source->pev->model ) );
    pHull->pev->solid = SOLID_BBOX;
    pHull->pev->movetype = MOVETYPE_NONE;
    pHull->pev->owner = source->edict();
    pHull->SetSize( mins, maxs );
    pHull->pev->renderamt = 0;
    pHull->pev->rendermode = kRenderTransTexture;
    //    pHull->pev->effects = EF_NODRAW;

    return pHull;
}

LINK_ENTITY_TO_CLASS( xen_spore_small, CXenSporeSmall );
LINK_ENTITY_TO_CLASS( xen_spore_medium, CXenSporeMed );
LINK_ENTITY_TO_CLASS( xen_spore_large, CXenSporeLarge );
LINK_ENTITY_TO_CLASS( xen_hull, CXenHull );

SpawnAction CXenSporeSmall::Spawn()
{
    pev->skin = 0;
    CXenSpore::Spawn();
    SetSize( Vector( -16, -16, 0 ), Vector( 16, 16, 64 ) );

    return SpawnAction::Spawn;
}
SpawnAction CXenSporeMed::Spawn()
{
    pev->skin = 1;
    CXenSpore::Spawn();
    SetSize( Vector( -40, -40, 0 ), Vector( 40, 40, 120 ) );

    return SpawnAction::Spawn;
}

// I just eyeballed these -- fill in hulls for the legs
const Vector CXenSporeLarge::m_hullSizes[] =
    {
        Vector( 90, -25, 0 ),
        Vector( 25, 75, 0 ),
        Vector( -15, -100, 0 ),
        Vector( -90, -35, 0 ),
        Vector( -90, 60, 0 ),
};

SpawnAction CXenSporeLarge::Spawn()
{
    pev->skin = 2;
    CXenSpore::Spawn();
    SetSize( Vector( -48, -48, 110 ), Vector( 48, 48, 240 ) );

    Vector forward, right;

    AngleVectors( pev->angles, &forward, &right, nullptr );

    // Rotate the leg hulls into position
    for( std::size_t i = 0; i < std::size( m_hullSizes ); i++ )
        CXenHull::CreateHull( this, Vector( -12, -12, 0 ), Vector( 12, 12, 120 ), ( m_hullSizes[i].x * forward ) + ( m_hullSizes[i].y * right ) );

    return SpawnAction::Spawn;
}

SpawnAction CXenSpore::Spawn()
{
    Precache();
    if( FStringNull( pev->model ) )
    {
        SetModel( pModelNames[pev->skin] );
    }
    else
    {
        SetModel( STRING( pev->model ) );
    }

    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_BBOX;
    pev->takedamage = DAMAGE_YES;

    //    SetActivity( ACT_IDLE );
    pev->sequence = 0;
    pev->frame = RANDOM_FLOAT( 0, 255 );
    pev->framerate = RANDOM_FLOAT( 0.7, 1.4 );
    ResetSequenceInfo();
    pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1, 0.4 ); // Load balance these a bit

    return SpawnAction::Spawn;
}

const char* CXenSpore::pModelNames[] =
    {
        "models/fungus(small).mdl",
        "models/fungus.mdl",
        "models/fungus(large).mdl",
};

void CXenSpore::Precache()
{
    if( FStringNull( pev->model ) )
    {
        // TODO: bound check
        PrecacheModel( pModelNames[pev->skin] );
    }
    else
    {
        PrecacheModel( STRING( pev->model ) );
    }
}

void CXenSpore::Touch( CBaseEntity* pOther )
{
}

void CXenSpore::Think()
{
    pev->nextthink = gpGlobals->time + 0.1;

#if 0
    DispatchAnimEvents( flInterval );

    switch ( GetActivity() )
    {
    default:
    case ACT_IDLE:
        break;

    }
#endif
}
