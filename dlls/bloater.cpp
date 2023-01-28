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
// Bloater
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define BLOATER_AE_ATTACK_MELEE1 0x01


class CBloater : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override { pev->yaw_speed = 120; }
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

	void PainSound() override;
	void AlertSound() override;
	void IdleSound() override;
	void AttackSnd();

	// No range attacks
	bool CheckRangeAttack1(float flDot, float flDist) override { return false; }
	bool CheckRangeAttack2(float flDot, float flDist) override { return false; }
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
};

LINK_ENTITY_TO_CLASS(monster_bloater, CBloater);

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CBloater::Classify()
{
	return m_iClass != 0 ? m_iClass : CLASS_ALIEN_MONSTER;
}

bool CBloater::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	PainSound();
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CBloater::PainSound()
{
}

void CBloater::AlertSound()
{
}

void CBloater::IdleSound()
{
}

void CBloater::AttackSnd()
{
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CBloater::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case BLOATER_AE_ATTACK_MELEE1:
	{
		// do stuff for this event.
		AttackSnd();
	}
	break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CBloater::Spawn()
{
	Precache();

	if (!FStringNull(pev->model))
		SET_MODEL(ENT(pev), (char*)STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/floater.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->spawnflags |= FL_FLY;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->health = 40;
	pev->view_ofs = VEC_VIEW; // position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;	  // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBloater::Precache()
{
	if (!FStringNull(pev->model))
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/floater.mdl");
}
