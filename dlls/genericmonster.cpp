/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Generic Monster - purely for scripted sequence work.
//=========================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "soundent.h"

// For holograms, make them not solid so the player can walk through them
//LRC- this seems to interfere with SF_MONSTER_CLIP
#define SF_GENERICMONSTER_NOTSOLID 4
#define SF_GENERICMONSTER_PLAYERMODEL 8
#define SF_GENERICMONSTER_INVULNERABLE 32
//Not implemented:
#define SF_GENERICMONSTER_CORPSE 64

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CGenericMonster : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int ISoundMask() override;
	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool HasCustomGibs() override { return !FStringNull(m_iszGibModel); }

	int m_iszGibModel;
};
LINK_ENTITY_TO_CLASS(monster_generic, CGenericMonster);

TYPEDESCRIPTION CGenericMonster::m_SaveData[] =
	{
		DEFINE_FIELD(CGenericMonster, m_iszGibModel, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CGenericMonster, CBaseMonster);

bool CGenericMonster::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_bloodColor"))
	{
		m_bloodColor = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszGibModel"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	return CBaseMonster::KeyValue(pkvd);
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CGenericMonster::Classify()
{
	return m_iClass != 0 ? m_iClass : CLASS_PLAYER_ALLY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGenericMonster::SetYawSpeed()
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGenericMonster::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// ISoundMask - generic monster can't hear.
//=========================================================
int CGenericMonster::ISoundMask()
{
	return bits_SOUND_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CGenericMonster::Spawn()
{
	// store the size, so we can use it to set up the hulls after Set_Model overwrites it.
	Vector vecSize = pev->size;

	//LRC - if the level designer forgets to set a model, don't crash!
	if (FStringNull(pev->model))
	{
		if (!FStringNull(pev->targetname))
			ALERT(at_error, "No model specified for monster_generic \"%s\"\n", STRING(pev->targetname));
		else
			ALERT(at_error, "No model specified for monster_generic at %.2f %.2f %.2f\n", pev->origin.x, pev->origin.y, pev->origin.z);
		pev->model = MAKE_STRING("models/player.mdl");
	}

	Precache();

	SET_MODEL(ENT(pev), (char*)STRING(pev->model));

	if (vecSize != g_vecZero)
	{
		Vector vecMax = vecSize / 2;
		Vector vecMin = -vecMax;
		if (!FBitSet(pev->spawnflags, SF_GENERICMONSTER_PLAYERMODEL))
		{
			vecMin.z = 0;
			vecMax.z = vecSize.z;
		}
		UTIL_SetSize(pev, vecMin, vecMax);
	}
	else if (
		FBitSet(pev->spawnflags, SF_GENERICMONSTER_PLAYERMODEL) ||
		FStrEq(STRING(pev->model), "models/player.mdl") ||
		FStrEq(STRING(pev->model), "models/holo.mdl"))
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	if (m_bloodColor == 0)
		m_bloodColor = BLOOD_COLOR_RED;
	if (pev->health == 0)
		pev->health = 8;
	m_flFieldOfView = 0.5; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	if (FBitSet(pev->spawnflags, SF_GENERICMONSTER_NOTSOLID))
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}
	else if (FBitSet(pev->spawnflags, SF_GENERICMONSTER_INVULNERABLE))
	{
		pev->takedamage = DAMAGE_NO;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGenericMonster::Precache()
{
	PRECACHE_MODEL((char*)STRING(pev->model));
	if (!FStringNull(m_iszGibModel))
		PRECACHE_MODEL((char*)STRING(m_iszGibModel)); //LRC
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================


//=========================================================
// GENERIC DEAD MONSTER, PROP
//=========================================================
class CDeadGenericMonster : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_PLAYER_ALLY; }
	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	bool HasCustomGibs() override { return !FStringNull(m_iszGibModel); }

	int m_iszGibModel;
};

LINK_ENTITY_TO_CLASS(monster_generic_dead, CDeadGenericMonster);

TYPEDESCRIPTION CDeadGenericMonster::m_SaveData[] =
	{
		DEFINE_FIELD(CDeadGenericMonster, m_iszGibModel, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CDeadGenericMonster, CBaseMonster);

bool CDeadGenericMonster::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_bloodColor"))
	{
		m_bloodColor = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszGibModel"))
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	return CBaseMonster::KeyValue(pkvd);
}

//=========================================================
// ********** DeadGenericMonster SPAWN **********
//=========================================================
void CDeadGenericMonster::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), STRING(pev->model));

	pev->effects = 0;
	pev->yaw_speed = 8; //LRC -- what?
	pev->sequence = 0;

	if (!FStringNull(pev->netname))
	{
		pev->sequence = LookupSequence(STRING(pev->netname));

		if (pev->sequence == -1)
		{
			ALERT(at_debug, "Invalid sequence name \"%s\" in monster_generic_dead\n", STRING(pev->netname));
		}
	}
	else
	{
		pev->sequence = LookupActivity(pev->frags);
	}

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();

	ResetSequenceInfo();
	pev->frame = 255; // pose at the _end_ of its death sequence.
}

void CDeadGenericMonster::Precache()
{
	PRECACHE_MODEL((char*)STRING(pev->model));
	if (!FStringNull(m_iszGibModel))
		PRECACHE_MODEL((char*)STRING(m_iszGibModel)); //LRC
}
