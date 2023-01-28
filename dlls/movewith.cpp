#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "movewith.h"
#include "saverestore.h"
#include "player.h"

CWorld* g_pWorld = NULL; //LRC

bool g_doingDesired = false; //LRC - marks whether the Desired functions are currently
							 // being processed.

void UTIL_AddToAssistList(CBaseEntity* pEnt)
{
	if (pEnt->m_pAssistLink)
	{
		return; // is pEnt already in the body of the list?
	}

	if (!g_pWorld)
	{
		ALERT(at_debug, "AddToAssistList %s \"%s\" has no AssistList!\n", STRING(pEnt->pev->classname), STRING(pEnt->pev->targetname));
		return;
	}

	CBaseEntity* pListMember = g_pWorld;

	// find the last entry in the list...
	while (pListMember->m_pAssistLink != NULL)
		pListMember = pListMember->m_pAssistLink;

	if (pListMember == pEnt)
	{
		return; // pEnt is the last entry in the list.
	}

	pListMember->m_pAssistLink = pEnt; // it's not in the list already, so add pEnt to the list.
}

void HandlePostAssist(CBaseEntity* pEnt)
{
	if (FBitSet(pEnt->m_iLFlags, LF_POSTASSISTVEL))
	{
		pEnt->pev->velocity = pEnt->m_vecPostAssistVel;
		pEnt->m_vecPostAssistVel = g_vecZero;
		pEnt->m_iLFlags &= ~LF_POSTASSISTVEL;
	}
	if (FBitSet(pEnt->m_iLFlags, LF_POSTASSISTAVEL))
	{
		pEnt->pev->avelocity = pEnt->m_vecPostAssistAVel;
		pEnt->m_vecPostAssistAVel = g_vecZero;
		pEnt->m_iLFlags &= ~LF_POSTASSISTAVEL;
	}
	CBaseEntity* pChild;
	for (pChild = pEnt->m_pChildMoveWith; pChild != NULL; pChild = pChild->m_pSiblingMoveWith)
		HandlePostAssist(pChild);
}

// returns 0 if no desired settings needed, else returns 1
int ApplyDesiredSettings(CBaseEntity* pListMember)
{
	if (FBitSet(pListMember->m_iLFlags, LF_DODESIRED))
	{
		pListMember->m_iLFlags &= ~LF_DODESIRED;
	}
	else
	{
		// don't need to apply any desired settings for this entity.
		return 0;
	}

	if (FBitSet(pListMember->m_iLFlags, LF_DESIRED_ACTION))
	{
		pListMember->m_iLFlags &= ~LF_DESIRED_ACTION;
		pListMember->DesiredAction();
	}

	if (FBitSet(pListMember->m_iLFlags, LF_DESIRED_INFO))
	{
		pListMember->m_iLFlags &= ~LF_DESIRED_INFO;
		ALERT(at_debug, "DesiredInfo: pos %f %f %f, vel %f %f %f. Child pos %f %f %f, vel %f %f %f\n\n", pListMember->pev->origin.x, pListMember->pev->origin.y, pListMember->pev->origin.z, pListMember->pev->velocity.x, pListMember->pev->velocity.y, pListMember->pev->velocity.z, pListMember->m_pChildMoveWith->pev->origin.x, pListMember->m_pChildMoveWith->pev->origin.y, pListMember->m_pChildMoveWith->pev->origin.z, pListMember->m_pChildMoveWith->pev->velocity.x, pListMember->m_pChildMoveWith->pev->velocity.y, pListMember->m_pChildMoveWith->pev->velocity.z);
	}

	if (FBitSet(pListMember->m_iLFlags, LF_DESIRED_POSTASSIST))
	{
		pListMember->m_iLFlags &= ~LF_DESIRED_POSTASSIST;
		HandlePostAssist(pListMember);
	}

	if (FBitSet(pListMember->m_iLFlags, LF_DESIRED_THINK))
	{
		pListMember->m_iLFlags &= ~LF_DESIRED_THINK;
		pListMember->Think();
	}

	return 1;
}

void AssistChildren(CBaseEntity* pEnt, Vector vecAdjustVel, Vector vecAdjustAVel)
{
	CBaseEntity* pChild;
	for (pChild = pEnt->m_pChildMoveWith; pChild != NULL; pChild = pChild->m_pSiblingMoveWith)
	{
		if (!FBitSet(pChild->m_iLFlags, LF_POSTASSISTVEL))
		{
			pChild->m_vecPostAssistVel = pChild->pev->velocity;
			pChild->m_iLFlags |= LF_POSTASSISTVEL;
		}
		if (!FBitSet(pChild->m_iLFlags, LF_POSTASSISTAVEL))
		{
			pChild->m_vecPostAssistAVel = pChild->pev->avelocity;
			pChild->m_iLFlags |= LF_POSTASSISTAVEL;
		}
		pChild->pev->velocity = pChild->pev->velocity - vecAdjustVel;
		pChild->pev->avelocity = pChild->pev->avelocity - vecAdjustAVel;

		AssistChildren(pChild, vecAdjustVel, vecAdjustAVel);
	}
}

// pEnt is a PUSH entity. Before physics is applied in a frame, we need to assist:
// 1) this entity, if it will stop moving this turn but its parent will continue.
// 2) this entity's PUSH children, if 1) was applied.
// 3) this entity's non-PUSH children, if this entity will stop moving this turn.
//
// returns 0 if the entity wasn't flagged for DoAssist.
int TryAssistEntity(CBaseEntity* pEnt)
{
	if (gpGlobals->frametime == 0)
	{
		return 0;
	}

	// not flagged as needing assistance?
	if (!FBitSet(pEnt->m_iLFlags, LF_DOASSIST))
		return 0;

	if (pEnt->m_fNextThink <= 0)
	{
		pEnt->m_iLFlags &= ~LF_DOASSIST;
		return 0; // the think has been cancelled. Oh well...
	}

	// a fraction of the current velocity. (the part of it that the engine should see.)
	float fFraction = 0;

	// is this the frame when the entity will stop?
	if (pEnt->pev->movetype == MOVETYPE_PUSH)
	{
		if (pEnt->m_fNextThink <= pEnt->pev->ltime + gpGlobals->frametime)
			fFraction = (pEnt->m_fNextThink - pEnt->pev->ltime) / gpGlobals->frametime;
	}
	else if (pEnt->m_fNextThink <= gpGlobals->time + gpGlobals->frametime)
	{
		fFraction = (pEnt->m_fNextThink - gpGlobals->time) / gpGlobals->frametime;
	}

	if (fFraction != 0)
	{
		if (FBitSet(pEnt->m_iLFlags, LF_CORRECTSPEED))
		{
			if (!FBitSet(pEnt->m_iLFlags, LF_POSTASSISTVEL))
			{
				pEnt->m_vecPostAssistVel = pEnt->pev->velocity;
				pEnt->m_iLFlags |= LF_POSTASSISTVEL;
			}

			if (!FBitSet(pEnt->m_iLFlags, LF_POSTASSISTAVEL))
			{
				pEnt->m_vecPostAssistAVel = pEnt->pev->avelocity;
				pEnt->m_iLFlags |= LF_POSTASSISTAVEL;
			}

			Vector vecVelTemp = pEnt->pev->velocity;
			Vector vecAVelTemp = pEnt->pev->avelocity;

			if (pEnt->m_pMoveWith)
			{
				pEnt->pev->velocity = (pEnt->pev->velocity - pEnt->m_pMoveWith->pev->velocity) * fFraction + pEnt->m_pMoveWith->pev->velocity;
				pEnt->pev->avelocity = (pEnt->pev->avelocity - pEnt->m_pMoveWith->pev->avelocity) * fFraction + pEnt->m_pMoveWith->pev->avelocity;
			}
			else
			{
				pEnt->pev->velocity = pEnt->pev->velocity * fFraction;
				pEnt->pev->avelocity = pEnt->pev->avelocity * fFraction;
			}

			AssistChildren(pEnt, vecVelTemp - pEnt->pev->velocity, vecAVelTemp - pEnt->pev->avelocity);
			UTIL_DesiredPostAssist(pEnt);
		}

		UTIL_DesiredThink(pEnt);

		pEnt->m_iLFlags &= ~LF_DOASSIST;
	}

	return 1;
}

// called every frame, by StartFrame
void CheckAssistList()
{
	CBaseEntity* pListMember;

	if (!g_pWorld)
	{
		ALERT(at_error, "CheckAssistList has no AssistList!\n");
		return;
	}

	int count = 0;

	for (pListMember = g_pWorld; pListMember; pListMember = pListMember->m_pAssistLink)
	{
		if (FBitSet(pListMember->m_iLFlags, LF_DOASSIST))
			count++;
	}
	count = 0;
	pListMember = g_pWorld;

	while (pListMember->m_pAssistLink) // handle the remaining entries in the list
	{
		TryAssistEntity(pListMember->m_pAssistLink);
		if (!FBitSet(pListMember->m_pAssistLink->m_iLFlags, LF_ASSISTLIST))
		{
			CBaseEntity* pTemp = pListMember->m_pAssistLink;
			pListMember->m_pAssistLink = pListMember->m_pAssistLink->m_pAssistLink;
			pTemp->m_pAssistLink = NULL;
		}
		else
		{
			pListMember = pListMember->m_pAssistLink;
		}
	}
}

// called every frame, by PostThink
void CheckDesiredList()
{
	CBaseEntity* pListMember;
	int loopbreaker = 1000; //assume this is the max. number of entities which will use DesiredList in any one frame

	if (g_doingDesired)
		ALERT(at_debug, "CheckDesiredList: doingDesired is already set!?\n");
	g_doingDesired = true;

	if (!g_pWorld)
	{
		ALERT(at_console, "CheckDesiredList has no AssistList!\n");
		return;
	}
	pListMember = g_pWorld;
	CBaseEntity* pNext;

	pListMember = g_pWorld->m_pAssistLink;

	while (pListMember)
	{
		// cache this, in case ApplyDesiredSettings does a SUB_Remove.
		pNext = pListMember->m_pAssistLink;
		ApplyDesiredSettings(pListMember);
		pListMember = pNext;
		loopbreaker--;
		if (loopbreaker <= 0)
		{
			ALERT(at_error, "Infinite(?) loop in DesiredList!");
			break;
		}
	}
	
	g_doingDesired = false;
}



void UTIL_MarkForAssist(CBaseEntity* pEnt, bool correctSpeed)
{
	pEnt->m_iLFlags |= LF_DOASSIST;

	if (correctSpeed)
		pEnt->m_iLFlags |= LF_CORRECTSPEED;
	else
		pEnt->m_iLFlags &= ~LF_CORRECTSPEED;

	UTIL_AddToAssistList(pEnt);
}

void UTIL_MarkForDesired(CBaseEntity* pEnt)
{
	pEnt->m_iLFlags |= LF_DODESIRED;

	if (g_doingDesired)
	{
		ApplyDesiredSettings(pEnt);
		return;
	}

	UTIL_AddToAssistList(pEnt);
}

void UTIL_DesiredAction(CBaseEntity* pEnt)
{
	pEnt->m_iLFlags |= LF_DESIRED_ACTION;
	UTIL_MarkForDesired(pEnt);
}

void UTIL_DesiredInfo(CBaseEntity* pEnt)
{
	pEnt->m_iLFlags |= LF_DESIRED_INFO;
	UTIL_MarkForDesired(pEnt);
}

void UTIL_DesiredPostAssist(CBaseEntity* pEnt)
{
	pEnt->m_iLFlags |= LF_DESIRED_POSTASSIST;
	UTIL_MarkForDesired(pEnt);
}

void UTIL_DesiredThink(CBaseEntity* pEnt)
{
	pEnt->m_iLFlags |= LF_DESIRED_THINK;
	pEnt->DontThink();
	UTIL_MarkForDesired(pEnt);
}



// LRC- change the origin to the given position, and bring any movewiths along too.
void UTIL_AssignOrigin(CBaseEntity* pEntity, const Vector vecOrigin)
{
	UTIL_AssignOrigin(pEntity, vecOrigin, true);
}

// LRC- bInitiator is true if this is being called directly, rather than because pEntity is moving with something else.
void UTIL_AssignOrigin(CBaseEntity* pEntity, const Vector vecOrigin, bool bInitiator)
{
	Vector vecDiff = vecOrigin - pEntity->pev->origin;
	if (vecDiff.Length() > 0.01 && CVAR_GET_FLOAT("sohl_mwdebug"))
		ALERT(at_debug, "AssignOrigin %s %s: (%f %f %f) goes to (%f %f %f)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->targetname), pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z, vecOrigin.x, vecOrigin.y, vecOrigin.z);

	UTIL_SetOrigin(pEntity, vecOrigin);

	if (bInitiator && pEntity->m_pMoveWith)
	{
		pEntity->m_vecMoveWithOffset = pEntity->pev->origin - pEntity->m_pMoveWith->pev->origin;
	}
	if (pEntity->m_pChildMoveWith) // now I've moved pEntity, does anything else have to move with it?
	{
		CBaseEntity* pChild = pEntity->m_pChildMoveWith;
		Vector vecTemp;
		while (pChild)
		{
			if (pChild->pev->movetype != MOVETYPE_PUSH || pChild->pev->velocity == pEntity->pev->velocity) // if the child isn't moving under its own power
			{
				UTIL_AssignOrigin(pChild, vecOrigin + pChild->m_vecMoveWithOffset, false);
			}
			else
			{
				vecTemp = vecDiff + pChild->pev->origin;
				UTIL_AssignOrigin(pChild, vecTemp, false);
			}
			pChild = pChild->m_pSiblingMoveWith;
		}
	}
}

void UTIL_SetAngles(CBaseEntity* pEntity, const Vector vecAngles)
{
	UTIL_SetAngles(pEntity, vecAngles, true);
}

void UTIL_SetAngles(CBaseEntity* pEntity, const Vector vecAngles, bool bInitiator)
{
	Vector vecDiff = vecAngles - pEntity->pev->angles;
	if (vecDiff.Length() > 0.01 && CVAR_GET_FLOAT("sohl_mwdebug"))
		ALERT(at_debug, "SetAngles %s %s: (%f %f %f) goes to (%f %f %f)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->targetname), pEntity->pev->angles.x, pEntity->pev->angles.y, pEntity->pev->angles.z, vecAngles.x, vecAngles.y, vecAngles.z);

	pEntity->pev->angles = vecAngles;

	if (bInitiator && pEntity->m_pMoveWith)
	{
		pEntity->m_vecRotWithOffset = vecAngles - pEntity->m_pMoveWith->pev->angles;
	}
	if (pEntity->m_pChildMoveWith) // now I've moved pEntity, does anything else have to move with it?
	{
		CBaseEntity* pChild = pEntity->m_pChildMoveWith;
		Vector vecTemp;
		while (pChild)
		{
			if (pChild->pev->avelocity == pEntity->pev->avelocity) // if the child isn't turning under its own power
			{
				UTIL_SetAngles(pChild, vecAngles + pChild->m_vecRotWithOffset, false);
			}
			else
			{
				vecTemp = vecDiff + pChild->pev->angles;
				UTIL_SetAngles(pChild, vecTemp, false);
			}
			pChild = pChild->m_pSiblingMoveWith;
		}
	}
}

//LRC- an arbitrary limit. If this number is exceeded we assume there's an infinite loop, and abort.
#define MAX_MOVEWITH_DEPTH 100

//LRC- for use in supporting movewith. Tell the entity that whatever it's moving with is about to change velocity.
// loopbreaker is there to prevent the game from hanging...
void UTIL_SetMoveWithVelocity(CBaseEntity* pEnt, const Vector vecSet, int loopbreaker)
{
	if (loopbreaker <= 0)
	{
		ALERT(at_error, "Infinite child list for MoveWith!");
		return;
	}

	if (!pEnt->m_pMoveWith)
	{
		ALERT(at_error, "SetMoveWithVelocity: no MoveWith entity!?\n");
		return;
	}

	Vector vecNew = (pEnt->pev->velocity - pEnt->m_pMoveWith->pev->velocity) + vecSet;

	if (pEnt->m_pChildMoveWith)
	{
		CBaseEntity* pMoving = pEnt->m_pChildMoveWith;
		int sloopbreaker = MAX_MOVEWITH_DEPTH; // to prevent the game hanging...
		while (pMoving)
		{
			UTIL_SetMoveWithVelocity(pMoving, vecNew, loopbreaker - 1);
			pMoving = pMoving->m_pSiblingMoveWith;
			sloopbreaker--;
			if (sloopbreaker <= 0)
			{
				ALERT(at_error, "SetMoveWithVelocity: Infinite sibling list for MoveWith!");
				break;
			}
		}
	}

	pEnt->pev->velocity = vecNew;
}

//LRC
void UTIL_SetVelocity(CBaseEntity* pEnt, const Vector vecSet)
{
	Vector vecNew;
	if (pEnt->m_pMoveWith)
		vecNew = vecSet + pEnt->m_pMoveWith->pev->velocity;
	else
		vecNew = vecSet;

	if (pEnt->m_pChildMoveWith)
	{
		CBaseEntity* pMoving = pEnt->m_pChildMoveWith;
		int sloopbreaker = MAX_MOVEWITH_DEPTH; // LRC - to save us from infinite loops
		while (pMoving)
		{
			UTIL_SetMoveWithVelocity(pMoving, vecNew, MAX_MOVEWITH_DEPTH);
			pMoving = pMoving->m_pSiblingMoveWith;
			sloopbreaker--;
			if (sloopbreaker <= 0)
			{
				ALERT(at_error, "SetVelocity: Infinite sibling list for MoveWith!\n");
				break;
			}
		}
	}

	pEnt->pev->velocity = vecNew;
}

//LRC - one more MoveWith utility. This one's for the simple version of RotWith.
void UTIL_SetMoveWithAvelocity(CBaseEntity* pEnt, const Vector vecSet, int loopbreaker)
{
	if (loopbreaker <= 0)
	{
		ALERT(at_error, "Infinite child list for MoveWith!");
		return;
	}

	if (!pEnt->m_pMoveWith)
	{
		ALERT(at_error, "SetMoveWithAvelocity: no MoveWith entity!?\n");
		return;
	}

	Vector vecNew = (pEnt->pev->avelocity - pEnt->m_pMoveWith->pev->avelocity) + vecSet;

	if (pEnt->m_pChildMoveWith)
	{
		CBaseEntity* pMoving = pEnt->m_pChildMoveWith;
		int sloopbreaker = MAX_MOVEWITH_DEPTH; // to prevent the game hanging...
		while (pMoving)
		{
			UTIL_SetMoveWithAvelocity(pMoving, vecNew, loopbreaker - 1);
			pMoving = pMoving->m_pSiblingMoveWith;
			sloopbreaker--;
			if (sloopbreaker <= 0)
			{
				ALERT(at_error, "SetMoveWithVelocity: Infinite sibling list for MoveWith!");
				break;
			}
		}
	}

	pEnt->pev->avelocity = vecNew;
}

void UTIL_SetAvelocity(CBaseEntity* pEnt, const Vector vecSet)
{
	Vector vecNew;
	if (pEnt->m_pMoveWith)
		vecNew = vecSet + pEnt->m_pMoveWith->pev->avelocity;
	else
		vecNew = vecSet;

	if (pEnt->m_pChildMoveWith)
	{
		CBaseEntity* pMoving = pEnt->m_pChildMoveWith;
		int sloopbreaker = MAX_MOVEWITH_DEPTH; // LRC - to save us from infinite loops
		while (pMoving)
		{
			UTIL_SetMoveWithAvelocity(pMoving, vecNew, MAX_MOVEWITH_DEPTH);
			pMoving = pMoving->m_pSiblingMoveWith;
			sloopbreaker--;
			if (sloopbreaker <= 0)
			{
				ALERT(at_error, "SetAvelocity: Infinite sibling list for MoveWith!\n");
				break;
			}
		}
	}
	
	pEnt->pev->avelocity = vecNew;
}
