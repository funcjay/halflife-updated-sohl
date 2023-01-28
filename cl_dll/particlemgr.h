// 02/08/02 November235: Particle System
#pragma once

#include "particlesys.h"

class ParticleSystemManager
{
public:
	ParticleSystemManager();
	~ParticleSystemManager();
	void AddSystem(ParticleSystem*);
	ParticleSystem* FindSystem(cl_entity_t* pEntity);
	void UpdateSystems(float frametime);
	void ClearSystems();
	void SortSystems();
	
	ParticleSystem* m_pFirstSystem;
};

extern ParticleSystemManager* g_pParticleSystems;
