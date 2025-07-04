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

#pragma once

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH 0x0001
#define SF_TRACKTRAIN_NOCONTROL 0x0002
#define SF_TRACKTRAIN_FORWARDONLY 0x0004
#define SF_TRACKTRAIN_PASSABLE 0x0008

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED 0x00000001
#define SF_PATH_FIREONCE 0x00000002
#define SF_PATH_ALTREVERSE 0x00000004
#define SF_PATH_DISABLE_TRAIN 0x00000008
#define SF_PATH_ALTERNATE 0x00008000

// Spawnflags of CPathCorner
#define SF_CORNER_WAITFORTRIG 0x001
#define SF_CORNER_TELEPORT 0x002
#define SF_CORNER_FIREONCE 0x004

// #define PATH_SPARKLE_DEBUG        1    // This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
    DECLARE_CLASS( CPathTrack, CPointEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Activate() override;
    bool KeyValue( KeyValueData* pkvd ) override;

    void SetPrevious( CPathTrack* pprevious );
    void Link();
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;

    CPathTrack* ValidPath( CPathTrack* ppath, bool testFlag ); // Returns ppath if enabled, nullptr otherwise
    void Project( CPathTrack* pstart, CPathTrack* pend, Vector* origin, float dist );

    static CPathTrack* Instance( CBaseEntity* pent );

    CPathTrack* LookAhead( Vector* origin, float dist, bool move );
    CPathTrack* Nearest( Vector origin );

    CPathTrack* GetNext();
    CPathTrack* GetPrevious();

#if PATH_SPARKLE_DEBUG
    void Sparkle();
#endif

    float m_length;
    string_t m_altName;
    CPathTrack* m_pnext;
    CPathTrack* m_pprevious;
    CPathTrack* m_paltpath;
};

class CFuncTrackTrain : public CBaseEntity
{
    DECLARE_CLASS( CFuncTrackTrain, CBaseEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;
    void Precache() override;

    void Blocked( CBaseEntity* pOther ) override;
    void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value = {} ) override;
    bool KeyValue( KeyValueData* pkvd ) override;

    void Next();
    void Find();
    void NearestPath();
    void DeadEnd();

    void NextThink( float thinkTime, bool alwaysThink );

    void SetTrack( CPathTrack* track ) { m_ppath = track->Nearest( pev->origin ); }
    void SetControls( CBaseEntity* controls );
    bool OnControls( CBaseEntity* controller ) override;

    void StopTrainSound();

    /**
     *    @brief update pitch based on speed, start sound if not playing
     *    NOTE: when train goes through transition, m_soundPlaying should go to 0,
     *    which will cause the looped sound to restart.
     */
    void UpdateTrainSound();

    static CFuncTrackTrain* Instance( CBaseEntity* pent );

    int ObjectCaps() override { return ( CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_DIRECTIONAL_USE; }

    void OverrideReset() override;

    CPathTrack* m_ppath;
    float m_length;
    float m_height;
    float m_speed;
    float m_dir;
    float m_startSpeed;
    Vector m_controlMins;
    Vector m_controlMaxs;
    bool m_soundPlaying;
    string_t m_sounds;
    float m_flVolume;
    float m_flBank;
    float m_oldSpeed;

private:
    float m_CachedPitch;
    float m_LastPlayerJoinTimeCheck;
};
