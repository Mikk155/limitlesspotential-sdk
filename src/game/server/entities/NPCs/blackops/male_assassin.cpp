/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *    This product contains software technology licensed from Id
 *    Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *    All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/

#include "cbase.h"
#include "squadmonster.h"
#include "talkmonster.h"
#include "military/hgrunt.h"

constexpr int MASSN_SNIPER_CLIP_SIZE = 1;

namespace MAssassinBodygroup
{
enum MAssassinBodygroup
{
    Heads = 1,
    Weapons = 2
};
}

namespace MAssassinHead
{
enum MAssassinHead
{
    Random = -1,
    White = 0,
    Black,
    ThermalVision
};
}

namespace MAssassinWeapon
{
enum MAssassinWeapon
{
    Blank = 0,
    MP5,
    SniperRifle,
};
}

namespace MAssassinWeaponFlag
{
enum MAssassinWeaponFlag
{
    SniperRifle = 1 << 3,
};
}

class CMOFAssassin : public CHGrunt
{
    DECLARE_CLASS( CMOFAssassin, CHGrunt );
    DECLARE_DATAMAP();

public:
    void OnCreate() override;
    SpawnAction Spawn() override;
    void HandleAnimEvent( MonsterEvent_t* pEvent ) override;
    bool CheckRangeAttack1( float flDot, float flDist ) override;
    bool CheckRangeAttack2( float flDot, float flDist ) override;
    void CheckAmmo() override;
    void PainSound() override;
    void IdleSound() override;
    void Shoot( bool firstShotInBurst ) override;
    void GibMonster() override;

    void TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) override;

    // Male Assassin never speaks
    bool FOkToSpeak() override { return false; }

    bool KeyValue( KeyValueData* pkvd ) override;

    float m_flLastShot;
    bool m_fStandingGround;
    float m_flStandGroundRange;

    int m_iAssassinHead;

protected:
    std::tuple<int, Activity> GetSequenceForActivity( Activity NewActivity ) override;
};

LINK_ENTITY_TO_CLASS( monster_male_assassin, CMOFAssassin );

BEGIN_DATAMAP( CMOFAssassin )
    DEFINE_FIELD( m_iAssassinHead, FIELD_INTEGER ),
    DEFINE_FIELD( m_flLastShot, FIELD_TIME ),
    DEFINE_FIELD( m_fStandingGround, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flStandGroundRange, FIELD_FLOAT ),
END_DATAMAP();

void CMOFAssassin::OnCreate()
{
    CHGrunt::OnCreate();

    pev->model = MAKE_STRING( "models/massn.mdl" );
}

void CMOFAssassin::GibMonster()
{
    Vector vecGunPos;
    Vector vecGunAngles;

    if( GetBodygroup( MAssassinBodygroup::Weapons ) != MAssassinWeapon::Blank )
    { // throw a gun if the grunt has one
        GetAttachment( 0, vecGunPos, vecGunAngles );

        CBaseEntity* pGun;
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            pGun = DropItem( "weapon_9mmar", vecGunPos, vecGunAngles );
        }
        else
        {
            pGun = DropItem( "weapon_sniperrifle", vecGunPos, vecGunAngles );
        }
        if( pGun )
        {
            pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
            pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
        }

        if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
        {
            pGun = DropItem( "ammo_argrenades", vecGunPos, vecGunAngles );
            if( pGun )
            {
                pGun->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );
                pGun->pev->avelocity = Vector( 0, RANDOM_FLOAT( 200, 400 ), 0 );
            }
        }
    }

    CBaseMonster::GibMonster();
}

bool CMOFAssassin::CheckRangeAttack1( float flDot, float flDist )
{
    if( 0 != pev->weapons )
    {
        if( m_fStandingGround && m_flStandGroundRange >= flDist )
        {
            m_fStandingGround = false;
        }

        return CHGrunt::CheckRangeAttack1( flDot, flDist );
    }

    return false;
}

bool CMOFAssassin::CheckRangeAttack2( float flDot, float flDist )
{
    return CheckRangeAttack2Core( flDot, flDist, g_cfg.GetValue<float>( "massassin_gspeed"sv, 800, this ) );
}

void CMOFAssassin::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType )
{
    // check for helmet shot
    if( ptr->iHitgroup == 11 )
    {
        // it's head shot anyways
        ptr->iHitgroup = HITGROUP_HEAD;
    }

    CSquadMonster::TraceAttack( attacker, flDamage, vecDir, ptr, bitsDamageType );
}

void CMOFAssassin::IdleSound()
{
    // Male Assassin doesn't make idle chat
}

void CMOFAssassin::CheckAmmo()
{
    if( 0 != pev->weapons )
    {
        CHGrunt::CheckAmmo();
    }
}

void CMOFAssassin::Shoot( bool firstShotInBurst )
{
    if( m_hEnemy )
    {
        if( !( FBitSet( pev->weapons, MAssassinWeaponFlag::SniperRifle ) && gpGlobals->time - m_flLastShot <= 0.11 ) )
        {
            Vector vecShootOrigin = GetGunPosition();
            Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

            UTIL_MakeVectors( pev->angles );

            if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
            {
                Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
                EjectBrass( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
                FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 2048, BULLET_MONSTER_MP5 ); // shoot +-5 degrees
            }
            else
            {
                // TODO: why is this 556? is 762 too damaging?
                FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_1DEGREES, 2048, BULLET_PLAYER_556 );
            }

            pev->effects |= EF_MUZZLEFLASH;

            m_cAmmoLoaded--; // take away a bullet!

            Vector angDir = UTIL_VecToAngles( vecShootDir );
            SetBlending( 0, angDir.x );
            m_flLastShot = gpGlobals->time;
        }
    }

    if( firstShotInBurst )
    {
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            // the first round of the three round burst plays the sound and puts a sound in the world sound list.
            if( RANDOM_LONG( 0, 1 ) )
            {
                EmitSound( CHAN_WEAPON, "hgrunt/gr_mgun1.wav", 1, ATTN_NORM );
            }
            else
            {
                EmitSound( CHAN_WEAPON, "hgrunt/gr_mgun2.wav", 1, ATTN_NORM );
            }
        }
        else
        {
            EmitSound( CHAN_WEAPON, "weapons/sniper_fire.wav", 1, ATTN_NORM );
        }

        CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
    }
}

void CMOFAssassin::HandleAnimEvent( MonsterEvent_t* pEvent )
{
    // Override grunt events that require assassin-specific behavior
    switch ( pEvent->event )
    {
    case HGRUNT_AE_DROP_GUN:
    {
        Vector vecGunPos;
        Vector vecGunAngles;

        GetAttachment( 0, vecGunPos, vecGunAngles );

        // switch to body group with no gun.
        SetBodygroup( MAssassinBodygroup::Weapons, MAssassinWeapon::Blank );

        // now spawn a gun.
        if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
        {
            DropItem( "weapon_9mmar", vecGunPos, vecGunAngles );
        }
        else
        {
            DropItem( "weapon_sniperrifle", vecGunPos, vecGunAngles );
        }
        if( FBitSet( pev->weapons, HGRUNT_GRENADELAUNCHER ) )
        {
            DropItem( "ammo_argrenades", BodyTarget( pev->origin ), vecGunAngles );
        }
    }
    break;

    case HGRUNT_AE_KICK:
    {
        CBaseEntity* pHurt = Kick();

        if( pHurt )
        {
            // SOUND HERE!
            UTIL_MakeVectors( pev->angles );
            pHurt->pev->punchangle.x = 15;
            pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
            pHurt->TakeDamage( this, this, g_cfg.GetValue<float>( "massassin_kick"sv, 25, this ), DMG_CLUB );
        }
    }
    break;

    default:
        CHGrunt::HandleAnimEvent( pEvent );
        break;
    }
}

SpawnAction CMOFAssassin::Spawn()
{
    Precache();

    if( pev->health < 1 )
        pev->health = g_cfg.GetValue<float>( "massassin_health"sv, 80, this );

    SetModel( STRING( pev->model ) );
    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    m_bloodColor = BLOOD_COLOR_RED;
    pev->effects = 0;
    m_flFieldOfView = 0.2; // indicates the width of this monster's forward view cone ( as a dotproduct result )
    m_MonsterState = MONSTERSTATE_NONE;
    m_flNextGrenadeCheck = gpGlobals->time + 1;
    m_flNextPainTime = gpGlobals->time;
    m_iSentence = HGRUNT_SENT_NONE;

    m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

    m_fEnemyEluded = false;
    m_fFirstEncounter = true; // this is true when the grunt spawns, because he hasn't encountered an enemy yet.

    m_HackedGunPos = Vector( 0, 0, 55 );

    if( pev->weapons == 0 )
    {
        // initialize to original values
        pev->weapons = HGRUNT_9MMAR | HGRUNT_HANDGRENADE;
        // pev->weapons = HGRUNT_SHOTGUN;
        // pev->weapons = HGRUNT_9MMAR | HGRUNT_GRENADELAUNCHER;
    }

    if( m_iAssassinHead == MAssassinHead::Random )
    {
        m_iAssassinHead = RANDOM_LONG( MAssassinHead::White, MAssassinHead::ThermalVision );
    }

    auto weaponModel = MAssassinWeapon::Blank;

    if( FBitSet( pev->weapons, HGRUNT_9MMAR ) )
    {
        weaponModel = MAssassinWeapon::MP5;
        m_cClipSize = GRUNT_CLIP_SIZE;
    }
    else if( FBitSet( pev->weapons, MAssassinWeaponFlag::SniperRifle ) )
    {
        weaponModel = MAssassinWeapon::SniperRifle;
        m_cClipSize = MASSN_SNIPER_CLIP_SIZE;
    }
    else
    {
        weaponModel = MAssassinWeapon::Blank;
        m_cClipSize = 0;
    }

    SetBodygroup( MAssassinBodygroup::Heads, m_iAssassinHead );
    SetBodygroup( MAssassinBodygroup::Weapons, weaponModel );

    m_cAmmoLoaded = m_cClipSize;

    m_flLastShot = gpGlobals->time;

    m_fStandingGround = m_flStandGroundRange != 0;

    CTalkMonster::g_talkWaitTime = 0;

    MonsterInit();

    return SpawnAction::Spawn;
}

void CMOFAssassin::PainSound()
{
    // Male Assassin doesn't make pain sounds
}

std::tuple<int, Activity> CMOFAssassin::GetSequenceForActivity( Activity NewActivity )
{
    int iSequence = ACTIVITY_NOT_AVAILABLE;

    switch ( NewActivity )
    {
    case ACT_RANGE_ATTACK1:
        // grunt is either shooting standing or shooting crouched
        // Sniper uses the same set
        if( m_fStanding )
        {
            // get aimable sequence
            iSequence = LookupSequence( "standing_mp5" );
        }
        else
        {
            // get crouching shoot
            iSequence = LookupSequence( "crouching_mp5" );
        }
        break;
    default:
        return CHGrunt::GetSequenceForActivity( NewActivity );
    }

    return {iSequence, NewActivity};
}

bool CMOFAssassin::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( "head", pkvd->szKeyName ) )
    {
        m_iAssassinHead = atoi( pkvd->szValue );
        return true;
    }
    else if( FStrEq( "standgroundrange", pkvd->szKeyName ) )
    {
        m_flStandGroundRange = atof( pkvd->szValue );
        return true;
    }

    return CHGrunt::KeyValue( pkvd );
}

class CMOFAssassinRepel : public CHGruntRepel
{
    DECLARE_CLASS( CMOFAssassinRepel, CHGruntRepel );
    DECLARE_DATAMAP();

public:
    void Precache() override;
    SpawnAction Spawn() override;
    void RepelUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value );
};

BEGIN_DATAMAP( CMOFAssassinRepel )
    DEFINE_FUNCTION( RepelUse ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( monster_assassin_repel, CMOFAssassinRepel );
void CMOFAssassinRepel::Precache()
{
    PrecacheCore( "monster_male_assassin" );
}

SpawnAction CMOFAssassinRepel::Spawn()
{
    CHGruntRepel::Spawn();
    SetUse( &CMOFAssassinRepel::RepelUse );

    return SpawnAction::Spawn;
}

void CMOFAssassinRepel::RepelUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value )
{
    CreateMonster( "monster_male_assassin" );
}

class CDeadMOFAssassin : public CDeadHGrunt
{
public:
    void OnCreate() override;
    SpawnAction Spawn() override;
};

LINK_ENTITY_TO_CLASS( monster_massassin_dead, CDeadMOFAssassin );

void CDeadMOFAssassin::OnCreate()
{
    CDeadHGrunt::OnCreate();

    pev->model = MAKE_STRING( "models/massn.mdl" );
}

SpawnAction CDeadMOFAssassin::Spawn()
{
    SpawnCore();

    // map old bodies onto new bodies
    switch ( pev->body )
    {
    case 0: // Grunt with Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( MAssassinBodygroup::Heads, MAssassinHead::White );
        SetBodygroup( MAssassinBodygroup::Weapons, MAssassinWeapon::MP5 );
        break;
    case 1: // Commander with Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( MAssassinBodygroup::Heads, MAssassinHead::ThermalVision );
        SetBodygroup( MAssassinBodygroup::Weapons, MAssassinWeapon::MP5 );
        break;
    case 2: // Grunt no Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( MAssassinBodygroup::Heads, MAssassinHead::Black );
        SetBodygroup( MAssassinBodygroup::Weapons, MAssassinWeapon::Blank );
        break;
    case 3: // Commander no Gun
        pev->body = 0;
        pev->skin = 0;
        SetBodygroup( MAssassinBodygroup::Heads, MAssassinHead::ThermalVision );
        SetBodygroup( MAssassinBodygroup::Weapons, MAssassinWeapon::Blank );
        break;
    }

    return SpawnAction::Spawn;
}
