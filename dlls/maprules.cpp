/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

//	-------------------------------------------
//
//	maprules.cpp
//
//	This module contains entities for implementing/changing game
//	rules dynamically within each map (.BSP)
//
//	-------------------------------------------

#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "gamerules.h"
#include "maprules.h"
#include "cbase.h"
#include "player.h"

class CRuleEntity : public CBaseEntity
{
public:
	void Spawn() override;
	bool KeyValue(KeyValueData* pkvd) override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void SetMaster(int iszMaster) { m_iszMaster = iszMaster; }

protected:
	bool CanFireForActivator(CBaseEntity* pActivator);

private:
	string_t m_iszMaster;
};

TYPEDESCRIPTION CRuleEntity::m_SaveData[] =
	{
		DEFINE_FIELD(CRuleEntity, m_iszMaster, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CRuleEntity, CBaseEntity);


void CRuleEntity::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;
}


bool CRuleEntity::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		SetMaster(ALLOC_STRING(pkvd->szValue));
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

bool CRuleEntity::CanFireForActivator(CBaseEntity* pActivator)
{
	if (!pActivator)
	{
		return true;
	}
	else if (!FStringNull(m_iszMaster))
	{
		return UTIL_IsMasterTriggered(m_iszMaster, pActivator);
	}

	return true;
}

//
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
public:
	void Spawn() override;
};

void CRulePointEntity::Spawn()
{
	CRuleEntity::Spawn();
	pev->frame = 0;
	pev->model = 0;
}

//
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
public:
	void Spawn() override;

private:
};

void CRuleBrushEntity::Spawn()
{
	SET_MODEL(edict(), STRING(pev->model));
	CRuleEntity::Spawn();
}


// CGameScore / game_score	-- award points to player / team
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

#define SF_SCORE_NEGATIVE 0x0001
#define SF_SCORE_TEAM 0x0002

class CGameScore : public CRulePointEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

	inline int Points() { return pev->frags; }
	inline bool AllowNegativeScore() { return (pev->spawnflags & SF_SCORE_NEGATIVE) != 0; }
	inline bool AwardToTeam() { return (pev->spawnflags & SF_SCORE_TEAM) != 0; }

	inline void SetPoints(int points) { pev->frags = points; }

private:
};

LINK_ENTITY_TO_CLASS(game_score, CGameScore);


void CGameScore::Spawn()
{
	CRulePointEntity::Spawn();
}


bool CGameScore::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "points"))
	{
		SetPoints(atoi(pkvd->szValue));
		return true;
	}

	return CRulePointEntity::KeyValue(pkvd);
}



void CGameScore::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	// Only players can use this
	if (pActivator->IsPlayer())
	{
		if (AwardToTeam())
		{
			pActivator->AddPointsToTeam(Points(), AllowNegativeScore());
		}
		else
		{
			pActivator->AddPoints(Points(), AllowNegativeScore());
		}
	}
}


// CGameEnd / game_end	-- Ends the game in MP

class CGameEnd : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
};

LINK_ENTITY_TO_CLASS(game_end, CGameEnd);


void CGameEnd::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	g_pGameRules->EndMultiplayerGame();
}


//
// CGameText / game_text	-- NON-Localized HUD Message (use env_message to display a titles.txt message)
//	Flag: All players					SF_ENVTEXT_ALLPLAYERS
//


#define SF_ENVTEXT_ALLPLAYERS 0x0001
#define SF_ENVTEXT_ONLY_ONCE 0x0002


class CGameText : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	inline bool MessageToAll() { return (pev->spawnflags & SF_ENVTEXT_ALLPLAYERS) != 0; }
	inline void MessageSet(const char* pMessage) { pev->message = ALLOC_STRING(pMessage); }
	inline const char* MessageGet() { return STRING(pev->message); }

	void EXPORT TriggerThink();

private:
	hudtextparms_t m_textParms;
	CBaseEntity* m_pActivator;
};

LINK_ENTITY_TO_CLASS(game_text, CGameText);

// Save parms as a block.  Will break save/restore if the structure changes, but this entity didn't ship with Half-Life, so
// it can't impact saved Half-Life games.
TYPEDESCRIPTION CGameText::m_SaveData[] =
	{
		DEFINE_ARRAY(CGameText, m_textParms, FIELD_CHARACTER, sizeof(hudtextparms_t)),
		DEFINE_FIELD(CGameText, m_pActivator, FIELD_CLASSPTR),
};

IMPLEMENT_SAVERESTORE(CGameText, CRulePointEntity);


bool CGameText::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "channel"))
	{
		m_textParms.channel = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "x"))
	{
		m_textParms.x = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "y"))
	{
		m_textParms.y = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "effect"))
	{
		m_textParms.effect = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color[4];
		UTIL_StringToIntArray(color, 4, pkvd->szValue);
		m_textParms.r1 = color[0];
		m_textParms.g1 = color[1];
		m_textParms.b1 = color[2];
		m_textParms.a1 = color[3];
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "color2"))
	{
		int color[4];
		UTIL_StringToIntArray(color, 4, pkvd->szValue);
		m_textParms.r2 = color[0];
		m_textParms.g2 = color[1];
		m_textParms.b2 = color[2];
		m_textParms.a2 = color[3];
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fadein"))
	{
		m_textParms.fadeinTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fadeout"))
	{
		m_textParms.fadeoutTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		m_textParms.holdTime = atof(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "fxtime"))
	{
		m_textParms.fxTime = atof(pkvd->szValue);
		return true;
	}

	return CRulePointEntity::KeyValue(pkvd);
}


void CGameText::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	if (MessageToAll())
	{
		UTIL_HudMessageAll(m_textParms, MessageGet());
	}
	else
	{
		if (pActivator && pActivator->IsNetClient())
		{
			UTIL_HudMessage(pActivator, m_textParms, MessageGet());
		}
	}

	if (pev->target)
	{
		m_pActivator = pActivator;
		SetThink(&CGameText::TriggerThink);
		SetNextThink(m_textParms.fadeinTime + m_textParms.holdTime + m_textParms.fadeoutTime);
		//		ALERT(at_console, "GameText sets NextThink = %f\n", m_textParms.fadeinTime + m_textParms.holdTime + m_textParms.fadeoutTime);
	}
	else if (pev->spawnflags & SF_ENVTEXT_ONLY_ONCE)
	{
		SetThink(&CGameText::SUB_Remove);
		SetNextThink(0.1);
	}
}

//LRC
void CGameText::TriggerThink()
{
	//	ALERT(at_console, "GameText uses targets\n");
	SUB_UseTargets(m_pActivator, USE_TOGGLE, 0);

	if (pev->spawnflags & SF_ENVTEXT_ONLY_ONCE)
	{
		SetThink(&CGameText::SUB_Remove);
		SetNextThink(0.1);
	}
}



//
// CGameTeamMaster / game_team_master	-- "Masters" like multisource, but based on the team of the activator
// Only allows mastered entity to fire if the team matches my team
//
// team index (pulled from server team list "mp_teamlist"
// Flag: Remove on Fire
// Flag: Any team until set?		-- Any team can use this until the team is set (otherwise no teams can use it)
//

#define SF_TEAMMASTER_FIREONCE 0x0001
#define SF_TEAMMASTER_ANYTEAM 0x0002

class CGameTeamMaster : public CRulePointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool IsTriggered(CBaseEntity* pActivator);
	const char* TeamID() override;
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_TEAMMASTER_FIREONCE) != 0; }
	inline bool AnyTeam() { return (pev->spawnflags & SF_TEAMMASTER_ANYTEAM) != 0; }

private:
	bool TeamMatch(CBaseEntity* pActivator);

	int m_teamIndex;
	USE_TYPE triggerType;
};

LINK_ENTITY_TO_CLASS(game_team_master, CGameTeamMaster);

bool CGameTeamMaster::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "teamindex"))
	{
		m_teamIndex = atoi(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi(pkvd->szValue);
		switch (type)
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		return true;
	}

	return CRulePointEntity::KeyValue(pkvd);
}


void CGameTeamMaster::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	if (useType == USE_SET)
	{
		if (value < 0)
		{
			m_teamIndex = -1;
		}
		else
		{
			m_teamIndex = g_pGameRules->GetTeamIndex(pActivator->TeamID());
		}
		return;
	}

	if (TeamMatch(pActivator))
	{
		SUB_UseTargets(pActivator, triggerType, value);
		if (RemoveOnFire())
			UTIL_Remove(this);
	}
}


bool CGameTeamMaster::IsTriggered(CBaseEntity* pActivator)
{
	return TeamMatch(pActivator);
}


const char* CGameTeamMaster::TeamID()
{
	if (m_teamIndex < 0) // Currently set to "no team"
		return "";

	return g_pGameRules->GetIndexedTeamName(m_teamIndex); // UNDONE: Fill this in with the team from the "teamlist"
}


bool CGameTeamMaster::TeamMatch(CBaseEntity* pActivator)
{
	if (m_teamIndex < 0 && AnyTeam())
		return true;

	if (!pActivator)
		return false;

	return UTIL_TeamsMatch(pActivator->TeamID(), TeamID());
}


//
// CGameTeamSet / game_team_set	-- Changes the team of the entity it targets to the activator's team
// Flag: Fire once
// Flag: Clear team				-- Sets the team to "NONE" instead of activator

#define SF_TEAMSET_FIREONCE 0x0001
#define SF_TEAMSET_CLEARTEAM 0x0002

class CGameTeamSet : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_TEAMSET_FIREONCE) != 0; }
	inline bool ShouldClearTeam() { return (pev->spawnflags & SF_TEAMSET_CLEARTEAM) != 0; }

private:
};

LINK_ENTITY_TO_CLASS(game_team_set, CGameTeamSet);


void CGameTeamSet::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	if (ShouldClearTeam())
	{
		SUB_UseTargets(pActivator, USE_SET, -1);
	}
	else
	{
		SUB_UseTargets(pActivator, USE_SET, 0);
	}

	if (RemoveOnFire())
	{
		UTIL_Remove(this);
	}
}


//
// CGamePlayerZone / game_player_zone -- players in the zone fire my target when I'm fired
//
// Needs master?
class CGamePlayerZone : public CRuleBrushEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	string_t m_iszInTarget;
	string_t m_iszOutTarget;
	string_t m_iszInCount;
	string_t m_iszOutCount;
};

LINK_ENTITY_TO_CLASS(game_zone_player, CGamePlayerZone);
TYPEDESCRIPTION CGamePlayerZone::m_SaveData[] =
	{
		DEFINE_FIELD(CGamePlayerZone, m_iszInTarget, FIELD_STRING),
		DEFINE_FIELD(CGamePlayerZone, m_iszOutTarget, FIELD_STRING),
		DEFINE_FIELD(CGamePlayerZone, m_iszInCount, FIELD_STRING),
		DEFINE_FIELD(CGamePlayerZone, m_iszOutCount, FIELD_STRING),
};

IMPLEMENT_SAVERESTORE(CGamePlayerZone, CRuleBrushEntity);

bool CGamePlayerZone::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "intarget"))
	{
		m_iszInTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "outtarget"))
	{
		m_iszOutTarget = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "incount"))
	{
		m_iszInCount = ALLOC_STRING(pkvd->szValue);
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "outcount"))
	{
		m_iszOutCount = ALLOC_STRING(pkvd->szValue);
		return true;
	}

	return CRuleBrushEntity::KeyValue(pkvd);
}

void CGamePlayerZone::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if (!CanFireForActivator(pActivator))
		return;

	CBaseEntity* pPlayer = NULL;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer)
		{
			TraceResult trace;
			int hullNumber;
			bool inside = false;

			hullNumber = human_hull;
			if ((pPlayer->pev->flags & FL_DUCKING) != 0)
			{
				hullNumber = human_hull;
				if ((pPlayer->pev->flags & FL_DUCKING) != 0)
				{
					hullNumber = head_hull;
				}
				UTIL_TraceModel(pPlayer->pev->origin, pPlayer->pev->origin, hullNumber, edict(), &trace);
				inside = 0 != trace.fStartSolid;
			}
			else
			{
				//LIMITATION: this doesn't allow for non-cuboid game_zone_player entities.
				// (is that a problem?)
				inside = this->Intersects(pPlayer);
			}

			if (inside)
			{
				playersInCount++;
				if (!FStringNull(m_iszInTarget))
				{
					FireTargets(STRING(m_iszInTarget), pPlayer, pActivator, useType, value);
				}
			}
			else
			{
				playersOutCount++;
				if (!FStringNull(m_iszOutTarget))
				{
					FireTargets(STRING(m_iszOutTarget), pPlayer, pActivator, useType, value);
				}
			}
		}
	}

	if (!FStringNull(m_iszInCount))
	{
		FireTargets(STRING(m_iszInCount), pActivator, this, USE_SET, playersInCount);
	}

	if (!FStringNull(m_iszOutCount))
	{
		FireTargets(STRING(m_iszOutCount), pActivator, this, USE_SET, playersOutCount);
	}
}



//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once

#define SF_PKILL_FIREONCE 0x0001
class CGamePlayerHurt : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_PKILL_FIREONCE) != 0; }

private:
};

LINK_ENTITY_TO_CLASS(game_player_hurt, CGamePlayerHurt);


void CGamePlayerHurt::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	if (pActivator->IsPlayer())
	{
		if (pev->dmg < 0)
			pActivator->TakeHealth(-pev->dmg, DMG_GENERIC);
		else
			pActivator->TakeDamage(pev, pev, pev->dmg, DMG_GENERIC);
	}

	SUB_UseTargets(pActivator, useType, value);

	if (RemoveOnFire())
	{
		UTIL_Remove(this);
	}
}



//
// CGameCounter / game_counter	-- Counts events and fires target
// Flag: Fire once
// Flag: Reset on Fire

#define SF_GAMECOUNT_FIREONCE 0x0001
#define SF_GAMECOUNT_RESET 0x0002

class CGameCounter : public CRulePointEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_GAMECOUNT_FIREONCE) != 0; }
	inline bool ResetOnFire() { return (pev->spawnflags & SF_GAMECOUNT_RESET) != 0; }

	inline void CountUp() { pev->frags++; }
	inline void CountDown() { pev->frags--; }
	inline void ResetCount() { pev->frags = pev->dmg; }
	inline int CountValue() { return pev->frags; }
	inline int LimitValue() { return pev->health; }

	inline bool HitLimit() { return CountValue() == LimitValue(); }

private:
	inline void SetCountValue(int value) { pev->frags = value; }
	inline void SetInitialValue(int value) { pev->dmg = value; }
};

LINK_ENTITY_TO_CLASS(game_counter, CGameCounter);

void CGameCounter::Spawn()
{
	// Save off the initial count
	SetInitialValue(CountValue());
	CRulePointEntity::Spawn();
}


void CGameCounter::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	switch (useType)
	{
	case USE_ON:
	case USE_TOGGLE:
		CountUp();
		break;

	case USE_OFF:
		CountDown();
		break;

	case USE_SET:
		SetCountValue((int)value);
		break;
	}

	if (HitLimit())
	{
		SUB_UseTargets(pActivator, USE_TOGGLE, 0);
		if (RemoveOnFire())
		{
			UTIL_Remove(this);
		}

		if (ResetOnFire())
		{
			ResetCount();
		}
	}
}



//
// CGameCounterSet / game_counter_set	-- Sets the counter's value
// Flag: Fire once

#define SF_GAMECOUNTSET_FIREONCE 0x0001

class CGameCounterSet : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_GAMECOUNTSET_FIREONCE) != 0; }

private:
};

LINK_ENTITY_TO_CLASS(game_counter_set, CGameCounterSet);


void CGameCounterSet::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	SUB_UseTargets(pActivator, USE_SET, pev->frags);

	if (RemoveOnFire())
	{
		UTIL_Remove(this);
	}
}


//
// CGamePlayerEquip / game_playerequip	-- Sets the default player equipment
// Flag: USE Only

#define SF_PLAYEREQUIP_USEONLY 0x0001
#define MAX_EQUIP 32

class CGamePlayerEquip : public CRulePointEntity
{
public:
	bool KeyValue(KeyValueData* pkvd) override;
	void Touch(CBaseEntity* pOther) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

	inline bool UseOnly() { return (pev->spawnflags & SF_PLAYEREQUIP_USEONLY) != 0; }

private:
	void EquipPlayer(CBaseEntity* pPlayer);

	string_t m_weaponNames[MAX_EQUIP];
	int m_weaponCount[MAX_EQUIP];
};

LINK_ENTITY_TO_CLASS(game_player_equip, CGamePlayerEquip);


bool CGamePlayerEquip::KeyValue(KeyValueData* pkvd)
{
	if (CRulePointEntity::KeyValue(pkvd))
	{
		return true;
	}

	for (int i = 0; i < MAX_EQUIP; i++)
	{
		if (FStringNull(m_weaponNames[i]))
		{
			char tmp[128];

			UTIL_StripToken(pkvd->szKeyName, tmp);

			m_weaponNames[i] = ALLOC_STRING(tmp);
			m_weaponCount[i] = atoi(pkvd->szValue);
			m_weaponCount[i] = V_max(1, m_weaponCount[i]);
			return true;
		}
	}

	return false;
}


void CGamePlayerEquip::Touch(CBaseEntity* pOther)
{
	if (!CanFireForActivator(pOther))
		return;

	if (UseOnly())
		return;

	EquipPlayer(pOther);
}

void CGamePlayerEquip::EquipPlayer(CBaseEntity* pEntity)
{
	CBasePlayer* pPlayer = NULL;

	if (pEntity->IsPlayer())
	{
		pPlayer = (CBasePlayer*)pEntity;
	}

	if (!pPlayer)
		return;

	for (int i = 0; i < MAX_EQUIP; i++)
	{
		if (FStringNull(m_weaponNames[i]))
			break;
		for (int j = 0; j < m_weaponCount[i]; j++)
		{
			pPlayer->GiveNamedItem(STRING(m_weaponNames[i]));
		}
	}
}


void CGamePlayerEquip::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	EquipPlayer(pActivator);
}


//
// CGamePlayerTeam / game_player_team	-- Changes the team of the player who fired it
// Flag: Fire once
// Flag: Kill Player
// Flag: Gib Player

#define SF_PTEAM_FIREONCE 0x0001
#define SF_PTEAM_KILL 0x0002
#define SF_PTEAM_GIB 0x0004

class CGamePlayerTeam : public CRulePointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
	inline bool RemoveOnFire() { return (pev->spawnflags & SF_PTEAM_FIREONCE) != 0; }
	inline bool ShouldKillPlayer() { return (pev->spawnflags & SF_PTEAM_KILL) != 0; }
	inline bool ShouldGibPlayer() { return (pev->spawnflags & SF_PTEAM_GIB) != 0; }

	const char* TargetTeamName(const char* pszTargetName);
};

LINK_ENTITY_TO_CLASS(game_player_team, CGamePlayerTeam);


const char* CGamePlayerTeam::TargetTeamName(const char* pszTargetName)
{
	CBaseEntity* pTeamEntity = NULL;

	while ((pTeamEntity = UTIL_FindEntityByTargetname(pTeamEntity, pszTargetName)) != NULL)
	{
		if (FClassnameIs(pTeamEntity->pev, "game_team_master"))
			return pTeamEntity->TeamID();
	}

	return NULL;
}


void CGamePlayerTeam::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!CanFireForActivator(pActivator))
		return;

	if (pActivator->IsPlayer())
	{
		const char* pszTargetTeam = TargetTeamName(STRING(pev->target));
		if (pszTargetTeam)
		{
			CBasePlayer* pPlayer = (CBasePlayer*)pActivator;
			g_pGameRules->ChangePlayerTeam(pPlayer, pszTargetTeam, ShouldKillPlayer(), ShouldGibPlayer());
		}
	}

	if (RemoveOnFire())
	{
		UTIL_Remove(this);
	}
}
