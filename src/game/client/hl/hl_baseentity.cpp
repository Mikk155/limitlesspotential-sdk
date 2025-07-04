/***
 *
 *    Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

/*
==========================
This file contains "stubs" of class member implementations so that we can predict certain
 weapons client side.  From time to time you might find that you need to implement part of the
 these functions.  If so, cut it from here, paste it in hl_weapons.cpp or somewhere else and
 add in the functionality you need.
==========================
*/
#include "cbase.h"
#include "nodes.h"

DEFINE_DUMMY_DATAMAP(CBaseDelay);
DEFINE_DUMMY_DATAMAP( CBaseAnimating );
DEFINE_DUMMY_DATAMAP(CBaseToggle);
DEFINE_DUMMY_DATAMAP(CBaseMonster);
DEFINE_DUMMY_DATAMAP(CBasePlayer);

BEGIN_CUSTOM_SCHEDULES_NOBASE(CBaseMonster)
END_CUSTOM_SCHEDULES();

// CBaseEntity Stubs
void CBaseEntity::SetOrigin( const Vector& origin )
{
}

int CBaseEntity::PrecacheModel( const char* s )
{
    return UTIL_PrecacheModel( s );
}

void CBaseEntity::SetModel( const char* s )
{
    // Nothing.
}

int CBaseEntity::PrecacheSound( const char* s )
{
    return UTIL_PrecacheSound( s );
}

void CBaseEntity::SetSize( const Vector& min, const Vector& max )
{
    // Nothing.
}

bool CBaseEntity::GiveHealth( float flHealth, int bitsDamageType ) { return true; }
bool CBaseEntity::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) { return true; }
CBaseEntity* CBaseEntity::GetNextTarget() { return nullptr; }
void CBaseEntity::SetObjectCollisionBox() {}
bool CBaseEntity::Intersects( CBaseEntity* pOther ) { return false; }
void CBaseEntity::MakeDormant() {}
bool CBaseEntity::IsDormant() { return false; }
bool CBaseEntity::IsInWorld() { return true; }
bool CBaseEntity::ShouldToggle( USE_TYPE useType, bool currentState ) { return false; }
int CBaseEntity::DamageDecal( int bitsDamageType ) { return -1; }
CBaseEntity* CBaseEntity::Create( const char* szName, const Vector& vecOrigin, const Vector& vecAngles, CBaseEntity* owner, bool callSpawn ) { return nullptr; }
void CBaseEntity::SUB_Remove() {}
void CBaseEntity::SUB_StartFadeOut() {}
void CBaseEntity::SUB_FadeOut() {}
void CBaseEntity::EmitSound( int channel, const char* sample, float volume, float attenuation ) {}
void CBaseEntity::EmitSoundDyn( int channel, const char* sample, float volume, float attenuation, int flags, int pitch ) {}
void CBaseEntity::StopSound( int channel, const char* sample ) {}
void CBaseEntity::UpdateOnRemove() {}

// CBaseDelay Stubs
bool CBaseDelay::KeyValue( KeyValueData* ) { return false; }

// UTIL_* Stubs
void UTIL_PrecacheOther( const char* szClassname ) {}
void UTIL_BloodDrips( const Vector& origin, const Vector& direction, int color, int amount ) {}
void UTIL_DecalTrace( TraceResult* pTrace, int decalNumber ) {}
void UTIL_GunshotDecalTrace( TraceResult* pTrace, int decalNumber ) {}
void UTIL_MakeVectors( const Vector& vecAngles ) {}
bool UTIL_IsValidEntity( edict_t* pent ) { return true; }
void UTIL_ClientPrintAll( int, char const*, char const*, char const*, char const*, char const* ) {}
void ClientPrint( CBasePlayer* client, int msg_dest, const char* msg_name, const char* param1, const char* param2, const char* param3, const char* param4 ) {}

// CBaseToggle Stubs
bool CBaseToggle::KeyValue( KeyValueData* ) { return false; }

// CGrenade Stubs
void CGrenade::BounceSound() {}
void CGrenade::Explode( Vector, Vector ) {}
void CGrenade::Explode( TraceResult*, int ) {}
void CGrenade::Killed( CBaseEntity*, int ) {}
void CGrenade::OnCreate() { CBaseMonster::OnCreate(); }
void CGrenade::Precache() {}
SpawnAction CGrenade::Spawn() { return SpawnAction::Spawn; }
CGrenade* CGrenade::ShootTimed( CBaseEntity* owner, Vector vecStart, Vector vecVelocity, float time ) { return nullptr; }
CGrenade* CGrenade::ShootContact( CBaseEntity* owner, Vector vecStart, Vector vecVelocity ) { return nullptr; }
void CGrenade::DetonateUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value ) {}

void UTIL_Remove( CBaseEntity* pEntity ) {}
CBaseEntity* UTIL_FindEntityInSphere( CBaseEntity* pStartEntity, const Vector& vecCenter, float flRadius ) { return nullptr; }
CBaseEntity* UTIL_FindEntityByClassname( CBaseEntity* pStartEntity, const char* szName ) { return nullptr; }

CSprite* CSprite::SpriteCreate( const char* pSpriteName, const Vector& origin, bool animate ) { return nullptr; }
void CBeam::PointEntInit( const Vector& start, int endIndex ) {}
CBeam* CBeam::BeamCreate( const char* pSpriteName, int width ) { return nullptr; }
void CSprite::Expand( float scaleSpeed, float fadeSpeed ) {}


CBaseEntity* CBaseMonster::CheckTraceHullAttack( float flDist, int iDamage, int iDmgType ) { return nullptr; }
void CBaseMonster::Eat( float flFullDuration ) {}
bool CBaseMonster::FShouldEat() { return true; }
void CBaseMonster::BarnacleVictimBitten( CBaseEntity* pevBarnacle ) {}
void CBaseMonster::BarnacleVictimReleased() {}
void CBaseMonster::Listen() {}
float CBaseMonster::FLSoundVolume( CSound* pSound ) { return 0.0; }
bool CBaseMonster::FValidateHintType( short sHint ) { return false; }
void CBaseMonster::Look( int iDistance ) {}
int CBaseMonster::ISoundMask() { return 0; }
CSound* CBaseMonster::PBestSound() { return nullptr; }
CSound* CBaseMonster::PBestScent() { return nullptr; }
float CBaseAnimating::StudioFrameAdvance( float flInterval ) { return 0.0; }
void CBaseMonster::MonsterThink() {}
void CBaseMonster::MonsterUse( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, UseValue value ) {}
int CBaseMonster::IgnoreConditions() { return 0; }
void CBaseMonster::RouteClear() {}
void CBaseMonster::RouteNew() {}
bool CBaseMonster::FRouteClear() { return false; }
bool CBaseMonster::FRefreshRoute() { return false; }
bool CBaseMonster::MoveToEnemy( Activity movementAct, float waitTime ) { return false; }
bool CBaseMonster::MoveToLocation( Activity movementAct, float waitTime, const Vector& goal ) { return false; }
bool CBaseMonster::MoveToTarget( Activity movementAct, float waitTime ) { return false; }
bool CBaseMonster::MoveToNode( Activity movementAct, float waitTime, const Vector& goal ) { return false; }
void CBaseMonster::RouteSimplify( CBaseEntity* pTargetEnt ) {}
bool CBaseMonster::FBecomeProne() { return true; }
bool CBaseMonster::CheckRangeAttack1( float flDot, float flDist ) { return false; }
bool CBaseMonster::CheckRangeAttack2( float flDot, float flDist ) { return false; }
bool CBaseMonster::CheckMeleeAttack1( float flDot, float flDist ) { return false; }
bool CBaseMonster::CheckMeleeAttack2( float flDot, float flDist ) { return false; }
void CBaseMonster::CheckAttacks( CBaseEntity* pTarget, float flDist ) {}
bool CBaseMonster::FCanCheckAttacks() { return false; }
bool CBaseMonster::CheckEnemy( CBaseEntity* pEnemy ) { return false; }
void CBaseMonster::PushEnemy( CBaseEntity* pEnemy, Vector& vecLastKnownPos ) {}
bool CBaseMonster::PopEnemy() { return false; }
void CBaseMonster::SetActivity( Activity NewActivity ) {}
void CBaseMonster::SetSequenceByName( const char* szSequence ) {}
int CBaseMonster::CheckLocalMove( const Vector& vecStart, const Vector& vecEnd, CBaseEntity* pTarget, float* pflDist ) { return 0; }
float CBaseMonster::OpenDoorAndWait( CBaseEntity* door ) { return 0.0; }
void CBaseMonster::AdvanceRoute( float distance ) {}
int CBaseMonster::RouteClassify( int iMoveFlag ) { return 0; }
bool CBaseMonster::BuildRoute( const Vector& vecGoal, int iMoveFlag, CBaseEntity* pTarget ) { return false; }
void CBaseMonster::InsertWaypoint( Vector vecLocation, int afMoveFlags ) {}
bool CBaseMonster::FTriangulate( const Vector& vecStart, const Vector& vecEnd, float flDist, CBaseEntity* pTargetEnt, Vector* pApex ) { return false; }
void CBaseMonster::Move( float flInterval ) {}
bool CBaseMonster::ShouldAdvanceRoute( float flWaypointDist ) { return false; }
void CBaseMonster::MoveExecute( CBaseEntity* pTargetEnt, const Vector& vecDir, float flInterval ) {}
void CBaseMonster::MonsterInit() {}
void CBaseMonster::MonsterInitThink() {}
void CBaseMonster::StartMonster() {}
void CBaseMonster::MovementComplete() {}
bool CBaseMonster::TaskIsRunning() { return false; }
Relationship CBaseMonster::IRelationship( CBaseEntity* pTarget ) { return Relationship::None; }
bool CBaseMonster::FindCover( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return false; }
bool CBaseMonster::BuildNearestRoute( Vector vecThreat, Vector vecViewOffset, float flMinDist, float flMaxDist ) { return false; }
CBaseEntity* CBaseMonster::BestVisibleEnemy() { return nullptr; }
bool CBaseMonster::FInViewCone( CBaseEntity* pEntity ) { return false; }
bool CBaseMonster::FInViewCone( Vector* pOrigin ) { return false; }
bool CBaseEntity::FVisible( CBaseEntity* pEntity ) { return false; }
bool CBaseEntity::FVisible( const Vector& vecOrigin ) { return false; }
void CBaseMonster::MakeIdealYaw( Vector vecTarget ) {}
float CBaseMonster::FlYawDiff() { return 0.0; }
float CBaseMonster::ChangeYaw( int yawSpeed ) { return 0; }
float CBaseMonster::VecToYaw( Vector vecDir ) { return 0.0; }
int CBaseAnimating::LookupActivity( int activity ) { return 0; }
int CBaseAnimating::LookupActivityHeaviest( int activity ) { return 0; }
void CBaseMonster::SetEyePosition() {}
int CBaseAnimating::LookupSequence( const char* label ) { return 0; }
void CBaseAnimating::ResetSequenceInfo() {}
int CBaseAnimating::GetSequenceFlags() { return 0; }
void CBaseAnimating::DispatchAnimEvents( float flInterval ) {}
void CBaseMonster::HandleAnimEvent( MonsterEvent_t* pEvent ) {}
float CBaseAnimating::SetBoneController( int iController, float flValue ) { return 0.0; }
void CBaseAnimating::InitBoneControllers() {}
float CBaseAnimating::SetBlending( int iBlender, float flValue ) { return 0; }
void CBaseAnimating::GetBonePosition( int iBone, Vector& origin, Vector& angles ) {}
void CBaseAnimating::GetAttachment( int iAttachment, Vector& origin, Vector& angles ) {}
int CBaseAnimating::FindTransition( int iEndingSequence, int iGoalSequence, int* piDir ) { return -1; }
void CBaseAnimating::GetAutomovement( Vector& origin, Vector& angles, float flInterval ) {}
void CBaseAnimating::SetBodygroup( int iGroup, int iValue ) {}
int CBaseAnimating::GetBodygroup( int iGroup ) const { return 0; }
Vector CBaseMonster::GetGunPosition() { return g_vecZero; }
void CBaseEntity::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) {}
void CBaseEntity::FireBullets( unsigned int cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, CBaseEntity* attacker ) {}
void CBaseEntity::TraceBleed( float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) {}
void CBaseMonster::MakeDamageBloodDecal( int cCount, float flNoise, TraceResult* ptr, const Vector& vecDir ) {}
bool CBaseMonster::FGetNodeRoute( Vector vecDest ) { return true; }
int CBaseMonster::FindHintNode() { return NO_NODE; }
void CBaseMonster::ReportAIState() {}
bool CBaseMonster::KeyValue( KeyValueData* pkvd ) { return false; }
bool CBaseMonster::FCheckAITrigger() { return false; }
bool CBaseMonster::CanPlaySequence( bool fDisregardMonsterState, int interruptLevel ) { return false; }
bool CBaseMonster::FindLateralCover( const Vector& vecThreat, const Vector& vecViewOffset ) { return false; }
Vector CBaseMonster::ShootAtEnemy( const Vector& shootOrigin ) { return g_vecZero; }
bool CBaseMonster::FacingIdeal() { return false; }
bool CBaseMonster::FCanActiveIdle() { return false; }
void CBaseMonster::PlaySentenceCore( const char* pszSentence, float duration, float volume, float attenuation ) {}
void CBaseMonster::PlayScriptedSentence( const char* pszSentence, float duration, float volume, float attenuation, bool bConcurrent, CBaseEntity* pListener ) {}
void CBaseMonster::SentenceStop() {}
void CBaseMonster::CorpseFallThink() {}
void CBaseMonster::MonsterInitDead() {}
bool CBaseMonster::BBoxFlat() { return true; }
bool CBaseMonster::GetEnemy() { return false; }
void CBaseMonster::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) {}
CBaseEntity* CBaseMonster::DropItem( const char* pszItemName, const Vector& vecPos, const Vector& vecAng ) { return nullptr; }
bool CBaseMonster::ShouldFadeOnDeath() { return false; }
void CBaseMonster::RadiusDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType, EntityClassification iClassIgnore ) {}
void CBaseMonster::RadiusDamage( Vector vecSrc, CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType, EntityClassification iClassIgnore ) {}
void CBaseMonster::FadeMonster() {}
void CBaseMonster::GibMonster() {}
Activity CBaseMonster::GetDeathActivity() { return ACT_DIE_HEADSHOT; }
MONSTERSTATE CBaseMonster::GetIdealState() { return MONSTERSTATE_ALERT; }
const Schedule_t* CBaseMonster::GetScheduleOfType( int Type ) { return nullptr; }
const Schedule_t* CBaseMonster::GetSchedule() { return nullptr; }
void CBaseMonster::RunTask( const Task_t* pTask ) {}
void CBaseMonster::StartTask( const Task_t* pTask ) {}
const Schedule_t* CBaseMonster::ScheduleFromName( const char* pName ) const { return nullptr; }
void CBaseMonster::BecomeDead() {}
void CBaseMonster::RunAI() {}
void CBaseMonster::Killed( CBaseEntity* attacker, int iGib ) {}
bool CBaseMonster::GiveHealth( float flHealth, int bitsDamageType ) { return false; }
bool CBaseMonster::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) { return false; }
void CBaseMonster::PostRestore() {}
void CBaseMonster::StopFollowing( bool clearSchedule ) {}
void CBaseMonster::StartFollowing( CBaseEntity* pLeader ) {}

int TrainSpeed( int iSpeed, int iMax ) { return 0; }
void CBasePlayer::DeathSound() {}
bool CBasePlayer::GiveHealth( float flHealth, int bitsDamageType ) { return false; }
void CBasePlayer::TraceAttack( CBaseEntity* attacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType ) {}
bool CBasePlayer::TakeDamage( CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, int bitsDamageType ) { return false; }
void CBasePlayer::PackDeadPlayerItems() {}
void CBasePlayer::RemoveAllItems( bool removeSuit ) {}
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim ) {}
void CBasePlayer::WaterMove() {}
bool CBasePlayer::IsOnLadder() { return false; }
void CBasePlayer::PlayerDeathThink() {}
void CBasePlayer::StartDeathCam() {}
void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle ) {}
void CBasePlayer::PlayerUse() {}
void CBasePlayer::Jump() {}
void CBasePlayer::Duck() {}
void CBasePlayer::PreThink() {}
void CBasePlayer::CheckTimeBasedDamage() {}
void CBasePlayer::UpdateGeigerCounter() {}
void CBasePlayer::CheckSuitUpdate() {}
void CBasePlayer::SetSuitUpdate( const char* name, int iNoRepeatTime ) {}
void CBasePlayer::UpdatePlayerSound() {}
void CBasePlayer::PostThink() {}
void CBasePlayer::Precache() {}
void CBasePlayer::RenewItems() {}
void CBasePlayer::SelectNextItem( int iItem ) {}
bool CBasePlayer::HasWeapons() { return false; }
void CBasePlayer::SelectPrevItem( int iItem ) {}
bool CBasePlayer::FlashlightIsOn() { return false; }
void CBasePlayer::FlashlightTurnOn() {}
void CBasePlayer::FlashlightTurnOff() {}
void CBasePlayer::ForceClientDllUpdate() {}
void CBasePlayer::ImpulseCommands() {}
void CBasePlayer::CheatImpulseCommands( int iImpulse ) {}
ItemAddResult CBasePlayer::AddPlayerWeapon( CBasePlayerWeapon* weapon ) { return ItemAddResult::NotAdded; }
bool CBasePlayer::RemovePlayerWeapon( CBasePlayerWeapon* weapon ) { return false; }
void CBasePlayer::ItemPreFrame() {}
void CBasePlayer::ItemPostFrame() {}
void CBasePlayer::SendAmmoUpdate() {}
void CBasePlayer::UpdateClientData() {}
bool CBasePlayer::FBecomeProne() { return true; }
void CBasePlayer::BarnacleVictimBitten( CBaseEntity* pevBarnacle ) {}
void CBasePlayer::BarnacleVictimReleased() {}
int CBasePlayer::Illumination() { return 0; }
void CBasePlayer::EnableControl( bool fControl ) {}
Vector CBasePlayer::GetAutoaimVector( float flDelta ) { return g_vecZero; }
Vector CBasePlayer::AutoaimDeflection( const Vector& vecSrc, float flDist, float flDelta ) { return g_vecZero; }
void CBasePlayer::ResetAutoaim() {}
void CBasePlayer::SetCustomDecalFrames( int nFrames ) {}
int CBasePlayer::GetCustomDecalFrames() { return -1; }
void CBasePlayer::DropPlayerWeapon( const char* pszItemName ) {}
bool CBasePlayer::HasPlayerWeapon( CBasePlayerWeapon* weapon ) { return false; }
bool CBasePlayer::SwitchWeapon( CBasePlayerWeapon* weapon ) { return false; }
Vector CBasePlayer::GetGunPosition() { return g_vecZero; }
const char* CBasePlayer::TeamID() { return ""; }
int CBasePlayer::GiveAmmo( int iCount, const char* szName ) { return 0; }
void CBasePlayer::AddPoints( int score, bool bAllowNegativeScore ) {}
void CBasePlayer::AddPointsToTeam( int score, bool bAllowNegativeScore ) {}
void CBasePlayer::PostRestore() {}

void ClearMultiDamage() {}
void ApplyMultiDamage( CBaseEntity* inflictor, CBaseEntity* attacker ) {}
void AddMultiDamage( CBaseEntity* inflictor, CBaseEntity* pEntity, float flDamage, int bitsDamageType ) {}
void SpawnBlood( Vector vecSpot, int bloodColor, float flDamage ) {}
int DamageDecal( CBaseEntity* pEntity, int bitsDamageType ) { return 0; }
void DecalGunshot( TraceResult* pTrace, int iBulletType ) {}
void EjectBrass( const Vector& vecOrigin, const Vector& vecVelocity, float rotation, int model, int soundtype ) {}
bool CBasePlayerWeapon::KeyValue( KeyValueData* pkvd ) { return false; }
float CBasePlayerWeapon::GetNextAttackDelay( float flTime ) { return flTime; }
void CBasePlayerWeapon::SetObjectCollisionBox() {}
void CBasePlayerWeapon::DestroyItem() {}
void CBasePlayerWeapon::Kill() {}
void CBasePlayerWeapon::AttachToPlayer( CBasePlayer* pPlayer ) {}
bool CBasePlayerWeapon::AddDuplicate( CBasePlayerWeapon* original ) { return false; }
void CBasePlayerWeapon::AddToPlayer( CBasePlayer* pPlayer ) {}
bool CBasePlayerWeapon::UpdateClientData( CBasePlayer* pPlayer ) { return false; }
bool CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon* weapon ) { return false; }
bool CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon* weapon ) { return false; }
void CBasePlayerWeapon::RetireWeapon() {}
void CBasePlayerWeapon::DoRetireWeapon() {}
CBasePlayerWeapon* CBasePlayerWeapon::GetItemToRespawn( const Vector& respawnPoint ) { return nullptr; }
ItemAddResult CBasePlayerWeapon::Apply( CBasePlayer* player ) { return ItemAddResult::NotAdded; }
void CBasePlayerWeapon::PostRestore() {}

bool CBasePlayerAmmo::KeyValue( KeyValueData* pkvd ) { return false; }
SpawnAction CBasePlayerAmmo::Spawn() { return SpawnAction::Spawn; }
bool CBasePlayerAmmo::AddAmmo( CBasePlayer* player ) { return false; }

void CSoundEnt::InsertSound( int iType, const Vector& vecOrigin, int iVolume, float flDuration ) {}
void RadiusDamage( Vector vecSrc, CBaseEntity* inflictor, CBaseEntity* attacker, float flDamage, float flRadius, int bitsDamageType, EntityClassification iClassIgnore ) {}
