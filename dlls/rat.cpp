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
// rat - environmental monster
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CRat : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void SetYawSpeed() override { pev->yaw_speed = 45; }
	int Classify() override;
};
LINK_ENTITY_TO_CLASS(monster_rat, CRat);

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CRat::Classify()
{
	return m_iClass != 0 ? m_iClass : CLASS_INSECT; //LRC- maybe someone needs to give them a basic biology lesson...
}

//=========================================================
// Spawn
//=========================================================
void CRat::Spawn()
{
	Precache();

	if (!FStringNull(pev->model))
		SET_MODEL(ENT(pev), (char*)STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/bigrat.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->health = 8;
	pev->view_ofs = Vector(0, 0, 6); // position of the eyes relative to monster's origin.
	m_flFieldOfView = 0.5;			 // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CRat::Precache()
{
	if (!FStringNull(pev->model))
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/bigrat.mdl");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
