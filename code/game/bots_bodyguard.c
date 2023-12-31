#include "g_local.h"

#define MAX_LASERS				3
#define MAX_LASER_HITS			10
#define MAX_LASER_ON_TIME		15000
#define MAX_LASER_PULSE_TIME	4000
#define DECOY_COOLDOWN_SECONDS	30000
#define	DECOY_ACTIVE_SECONDS	15000

typedef struct laserState_s {
	qboolean active;		// has the laser base been placed in the game world?
	qboolean on;			// is the laser currently on?
	gentity_t *base;		// pointer to the laser base entity
	gentity_t *beam;		// pointer to the laser beam entity
	gentity_t *reflection;	// pointer to the laser beam reflection entity
	int onTime;				// time the laser was turned on
	gentity_t *bodyguard;	// pointer to the bodyguard that owns the laser
	vec3_t initialAngle;	// initial angle for reflection
	vec3_t overrideAngle;	// override angle for reflection
	void (*enable)(struct laserState_s *state);
	void (*disable)(struct laserState_s *state);
	void (*kill)(struct laserState_s *state);
	void (*pulseOn)(struct laserState_s *state);
	void (*pulseOff)(struct laserState_s *state);
	qboolean (*applyAngle)(struct laserState_s *state, gentity_t *ent);
	void (*resetAngle)(struct laserState_s *state);
} laserState_t;

typedef struct bodyguardState_s {
	laserState_t lasers[MAX_LASERS];
	qboolean protect;
	int decoy;
	qboolean pulse;
} bodyguardState_t;

bodyguardState_t bodyguardStates[MAX_CLIENTS];

bodyguardState_t *BOTS_Bodyguard_GetState(int clientNum)
{
	return &bodyguardStates[clientNum];
}

void BOTS_Bodyguard_Network(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	int i = 0;

	if (state->decoy > level.time)
	{
		trap_Net_WriteBits(1, 1);
		trap_Net_WriteBits( (state->decoy - level.time) / 1000, 8);
	}
	else if (state->decoy + DECOY_COOLDOWN_SECONDS > level.time)
	{
		trap_Net_WriteBits(0, 1);
		trap_Net_WriteBits( (state->decoy + DECOY_COOLDOWN_SECONDS - level.time) / 1000, 8);
	}
	else
	{
		trap_Net_WriteBits(0, 1);
		trap_Net_WriteBits(0, 8);
	}

	trap_Net_WriteBits(state->protect ? 1 : 0, 1);
	trap_Net_WriteBits(state->pulse ? 1 : 0, 1);
	for (i=0;i<MAX_LASERS;i++)
	{
		laserState_t *laser = &state->lasers[i];
		if (laser->active)
		{
			trap_Net_WriteBits(1, 1);
			trap_Net_WriteBits(laser->on ? 1 : 0, 1);
			trap_Net_WriteFloat(Distance(ent->r.currentOrigin, laser->base->r.currentOrigin));
		}
		else
		{
			trap_Net_WriteBits(0, 1);
		}
	}
}

laserState_t *BOTS_Bodyguard_GetLaserState(gentity_t *beam)
{
	gentity_t *bodyguard = beam->parent;
	int i;
	int clientNum = bodyguard->client->ps.clientNum;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	for (i=0;i<MAX_LASERS;i++)
	{
		laserState_t *laserState = &state->lasers[i];
		if (laserState->beam != (gentity_t *)NULL && laserState->beam == beam)
		{
			return laserState;
		}

		if (laserState->reflection != (gentity_t *)NULL && laserState->reflection == beam) 
		{
			return laserState;
		}
	}
	return (laserState_t *)NULL;
}

laserState_t *BOTS_Bodyguard_FindLaserState_ByNumber(bodyguardState_t *bodyguardState, int number)
{
	number -= 1;
	if (number >= 0 && number < MAX_LASERS) {
		return &bodyguardState->lasers[number];
	}
	return (laserState_t *)NULL;
}

void BOTS_Bodyguard_LaserBeam_Attack(gentity_t *inflictor, gentity_t *bodyguard, vec3_t origin, vec3_t angle, int passEntityNum, int damage, vec3_t *finalStart, vec3_t *finalEnd)
{
	gentity_t *unlinkedEntities[MAX_LASER_HITS];
	int unlinked = 0;
	int i;
	vec3_t end;
	trace_t tr;

	// fire forward and see what we hit
	VectorMA(origin, 8192, angle, end);

	do
	{
		gentity_t* traceEnt;

		trap_Trace(&tr, origin, NULL, NULL, end, passEntityNum, MASK_SHOT);

		if (tr.entityNum >= ENTITYNUM_MAX_NORMAL)
			break;

		traceEnt = &g_entities[tr.entityNum];

		// hurt it if we can
		if (traceEnt->takedamage && traceEnt->client && !OnSameTeam(bodyguard, traceEnt))
			G_Damage(traceEnt, inflictor, bodyguard, angle, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);

		// we hit something solid enough to stop the beam
		if (tr.contents & CONTENTS_SOLID)
			break;

		// unlink this entity, so the next trace will go past it
		trap_UnlinkEntity(traceEnt);
		unlinkedEntities[unlinked] = traceEnt;
		unlinked++;
	} while (unlinked < MAX_LASER_HITS);

	// link back in any entities we unlinked
	for (i = 0; i < unlinked; i++)
		trap_LinkEntity(unlinkedEntities[i]);

	// save net bandwidth
	SnapVector(tr.endpos);

	// set endpoint of laser
	VectorCopy(origin, *finalStart);
	VectorCopy(tr.endpos, *finalEnd);
}

void BOTS_Bodyguard_LaserBeam_Think(gentity_t* self) 
{
	gentity_t *bodyguard = self->parent;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(bodyguard->s.clientNum);
	laserState_t *laserState = BOTS_Bodyguard_GetLaserState(self);
	int elapsed;
	qboolean hitSolidSurface;
	vec3_t reflectedAngle;

	if (laserState == (laserState_t *)NULL)
		return;
	
	elapsed = level.time - laserState->onTime;
	if (elapsed >= MAX_LASER_ON_TIME)
	{
		laserState->kill(laserState);
		return;
	}

	if (state->pulse)
	{
		if (elapsed > MAX_LASER_PULSE_TIME * 2)
		{
			laserState->pulseOn(laserState);
		}
		else if (elapsed > MAX_LASER_PULSE_TIME)
		{
			laserState->pulseOff(laserState);
			return;
		}
	}

	BOTS_Bodyguard_LaserBeam_Attack(self, bodyguard, self->s.pos.trBase, self->movedir, self->s.number, self->damage, &self->s.origin, &self->s.origin2);

	trap_LinkEntity( self );

	self->nextthink = level.time + FRAMETIME;
}

void BOTS_Bodyguard_LaserBeamReflection_Think(gentity_t *self) 
{
	gentity_t *bodyguard = self->parent;

	BOTS_Bodyguard_LaserBeam_Attack(self, bodyguard, self->s.pos.trBase, self->movedir, self->s.number, self->damage, &self->s.origin, &self->s.origin2);

	trap_LinkEntity(self);

	self->nextthink = level.time + FRAMETIME;
}

gentity_t *BOTS_Bodyguard_CreateLaserBeam(gentity_t *ent, trace_t tr)
{
	vec3_t wallp;
	gentity_t *laser;
	// Make the laser beam
	laser = G_Spawn();
	laser->s.eType	= ET_BOTS_LASER;
	laser->s.modelindex	= 1;
	laser->s.modelindex2 = 0; // This is non-zero if it's a dropped item
	laser->s.number	= laser - g_entities;
	laser->s.frame = 2;	// beam diameter
	laser->classname = "bots_laser";
	laser->damage = 10000;
	laser->activator = ent;
	laser->parent = ent;
	laser->s.pos.trTime	= level.time;
	laser->nextthink = 0;
	laser->think = BOTS_Bodyguard_LaserBeam_Think;
	laser->s.clientNum = ent->s.clientNum;
	laser->s.pos.trType	= TR_LINEAR;

	// Set orgin of laser to the end of the lasermount model (aprox 10u)
	VectorMA(tr.endpos, 10, tr.plane.normal, wallp);
	G_SetOrigin(laser, wallp);

	// setup laser movedir (projection of laser)
	VectorCopy(tr.plane.normal, laser->movedir);
	VectorCopy(tr.plane.normal, laser->s.angles);

	trap_LinkEntity (laser);

	return laser;
}

gentity_t *BOTS_Bodyguard_CreateLaserBeam_Reflection(gentity_t *parentBeam, gentity_t *bodyguard) 
{
	trace_t tr;
	vec3_t traceEnd;
	vec3_t newOrigin;
	vec3_t newEnd;
	gentity_t *laser;

	VectorMA(parentBeam->s.pos.trBase, 8192, parentBeam->s.angles, traceEnd);

	trap_Trace(&tr, parentBeam->s.pos.trBase, NULL, NULL, traceEnd, parentBeam->s.number, MASK_SOLID);

	if (tr.surfaceFlags & SURF_SKY) {
		return (gentity_t *)NULL;
	}

	// save net bandwidth
	SnapVector(tr.endpos);

	// set endpoint of laser
	VectorCopy(tr.endpos, newOrigin);

	// Make the laser beam
	laser = G_Spawn();
	laser->s.eType = ET_BOTS_LASER;
	laser->s.modelindex = 1;
	laser->s.modelindex2 = 0; // This is non-zero if it's a dropped item
	laser->s.number = laser - g_entities;
	laser->s.frame = 2;	// beam diameter
	laser->classname = "bots_laser";
	laser->damage = 10000;
	laser->activator = bodyguard;
	laser->parent = bodyguard;
	laser->s.pos.trTime = level.time;
	laser->nextthink = 0;
	laser->think = BOTS_Bodyguard_LaserBeamReflection_Think;
	laser->s.clientNum = bodyguard->s.clientNum;
	laser->s.pos.trType = TR_LINEAR;

	// Set orgin of laser to the end of the lasermount model (aprox 10u)
	G_SetOrigin(laser, newOrigin);

	// setup laser movedir (projection of laser)
	VectorCopy(tr.plane.normal, laser->movedir);
	VectorCopy(tr.plane.normal, laser->s.angles);

	trap_LinkEntity(laser);

	return laser;
}

gentity_t *BOTS_Bodyguard_CreateLaserBase(gentity_t *ent, trace_t tr)
{
	gentity_t *laser_mount;
	//make the laser mount
	laser_mount					= G_Spawn();
	laser_mount->s.eType		= ET_GENERAL;
	laser_mount->s.modelindex	= G_ModelIndex("models/bots/laser_pod.md3");
	laser_mount->s.modelindex2	= 0; // This is non-zero if it's a dropped item
	laser_mount->s.number		= laser_mount - g_entities;
	laser_mount->classname		= "laser_mount";
	laser_mount->r.contents		= CONTENTS_SOLID;
	laser_mount->clipmask		= CONTENTS_SOLID;
	laser_mount->parent			= ent;

	laser_mount->classname = "laser_mount";

	//set the bounding box (the area that something can hit the laser trap)
	VectorSet(laser_mount->r.mins, -7, -7, -7);
	VectorCopy(laser_mount->r.mins, laser_mount->r.absmin);
	VectorSet(laser_mount->r.maxs, 7, 7, 7);
	VectorCopy(laser_mount->r.maxs, laser_mount->r.absmax);

	// set the movedir so the explosion looks right
	VectorCopy(tr.plane.normal, laser_mount->movedir);
	vectoangles(tr.plane.normal, laser_mount->s.angles);

	G_SetOrigin(laser_mount, tr.endpos);

	vectoangles(tr.plane.normal, laser_mount->s.apos.trBase); // rotate the mount
	//VectorCopy( up, laser_mount->s.apos.trBase );

	trap_LinkEntity (laser_mount);

	return laser_mount;
}

void BOTS_Bodyguard_LaserEnable(laserState_t *laserState)
{
	int pLevel = laserState->bodyguard->client->ps.persistant[PERS_LEVEL];

	if (laserState->active && !laserState->on)
	{
		laserState->onTime = level.time;
		laserState->beam->nextthink = level.time + FRAMETIME;
		if (laserState->reflection && pLevel >= 3) 
		{
			laserState->reflection->nextthink = level.time + FRAMETIME;
		}
		laserState->on = qtrue;
	}
}

void BOTS_Bodyguard_LaserDisable(laserState_t *laserState)
{
	if (laserState->active && laserState->on)
	{
		laserState->onTime = 0;
		trap_UnlinkEntity(laserState->beam);
		laserState->beam->nextthink = 0;
		if (laserState->reflection)
		{
			trap_UnlinkEntity(laserState->reflection);
			laserState->reflection->nextthink = 0;
		}
		laserState->on = qfalse;
	}
}

void BOTS_Bodyguard_LaserKill(laserState_t *laserState)
{
	if (laserState->active)
	{
		G_FreeEntity(laserState->base);
		G_FreeEntity(laserState->beam);
		if (laserState->reflection) 
		{
			G_FreeEntity(laserState->reflection);
		}
		laserState->base = (gentity_t *)NULL;
		laserState->beam = (gentity_t *)NULL;
		laserState->reflection = (gentity_t *)NULL;
		laserState->onTime = 0;
		laserState->on = qfalse;
		laserState->active = qfalse;
		VectorClear(laserState->initialAngle);
		VectorClear(laserState->overrideAngle);
	}
}

void BOTS_Bodyguard_LaserPulseOn(laserState_t *laserState)
{
	if (laserState->active)
		laserState->enable(laserState);
}

void BOTS_Bodyguard_LaserPulseOff(laserState_t *laserState)
{
	if (laserState->active)
	{
		laserState->on = qfalse;
		trap_UnlinkEntity(laserState->beam);
		laserState->beam->nextthink = level.time + FRAMETIME;
		if (laserState->reflection) 
		{
			trap_UnlinkEntity(laserState->reflection);
			laserState->reflection->nextthink = level.time + FRAMETIME;
		}
	}
}

qboolean BOTS_Bodyguard_LaserApplyAngle(laserState_t *laserState, gentity_t *ent) 
{
	vec3_t forward;
	if (laserState->reflection) 
	{
		AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
		VectorCopy(forward, laserState->overrideAngle);
		VectorCopy(forward, laserState->reflection->movedir);
		VectorCopy(forward, laserState->reflection->s.angles);
		return qtrue;
	}
	return qfalse;
}

void BOTS_Bodyguard_LaserResetAngle(laserState_t *laserState) 
{
	if (laserState->reflection) 
	{
		VectorCopy(laserState->initialAngle, laserState->reflection->movedir);
		VectorCopy(laserState->initialAngle, laserState->reflection->s.angles);
		VectorClear(laserState->overrideAngle);
	}
}

void BOTS_Bodyguard_PlaceLaser(gentity_t *ent, laserState_t *laserState) 
{
	int clientNum = ent->client->ps.clientNum;
	vec3_t		wallp;
	vec3_t		forward, right, up;
	vec3_t		muzzle;
	trace_t		tr;
	gentity_t *laser;
	gentity_t *laser_mount;
	gentity_t *reflection;

	// set aiming directions
	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	// setup muzzle point
	CalcMuzzlePoint(ent, forward, NULL, NULL, muzzle);

	VectorNormalize (forward);

	// Setup end point of the vector we want to trace
	VectorCopy(muzzle, wallp);
	VectorMA(wallp, 50, forward, wallp);

	// do the trace
	trap_Trace(&tr, muzzle, NULL, NULL, wallp, ent->s.number, MASK_SOLID);

	// Line complete ? (ie. no collision)
	if ((Q_stricmp (g_entities[tr.entityNum].classname, "func_door") == 0))
	{
		trap_SendServerCommand( clientNum, "print \"You cannot place lasers on doors.\n\"");
		return;
	}
	else if (tr.fraction > .99) 
	{
		trap_SendServerCommand( clientNum, "print \"You are too far from a wall.\n\"");
		return;
	}
	else if (tr.fraction < .2) //Check to see if your too close to the wall so you don't get stuck
	{
		trap_SendServerCommand( clientNum, "print \"You are too close to the wall.\n\"");
		return;
	}

	// save net bandwidth
	SnapVector( tr.endpos );

	laser = BOTS_Bodyguard_CreateLaserBeam(ent, tr);
	laser_mount = BOTS_Bodyguard_CreateLaserBase(ent, tr);
	reflection = BOTS_Bodyguard_CreateLaserBeam_Reflection(laser, ent);
	
	laserState->bodyguard = ent;
	laserState->active = qtrue;
	laserState->beam = laser;
	laserState->base = laser_mount;
	laserState->reflection = reflection;
	laserState->on = qfalse;
	laserState->enable = BOTS_Bodyguard_LaserEnable;
	laserState->disable = BOTS_Bodyguard_LaserDisable;
	laserState->kill = BOTS_Bodyguard_LaserKill;
	laserState->pulseOn = BOTS_Bodyguard_LaserPulseOn;
	laserState->pulseOff = BOTS_Bodyguard_LaserPulseOff;
	laserState->applyAngle = BOTS_Bodyguard_LaserApplyAngle;
	laserState->resetAngle = BOTS_Bodyguard_LaserResetAngle;


	if (reflection)
	{
		VectorCopy(reflection->movedir, laserState->initialAngle);
	}
	else
	{
		VectorClear(laserState->initialAngle);
	}
	VectorClear(laserState->overrideAngle);
}

void BOTS_BodyguardCommand_Laser(int clientNum)
{
	int i = 0;
	laserState_t *laserState;
	gentity_t *ent = g_entities + clientNum;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];

	int totalActive = 0;

	for (i=0;i<MAX_LASERS;i++)
	{
		laserState = &state->lasers[i];
		if (laserState->active)
			totalActive++;
	}

	if (pLevel < 2 && totalActive == 1)
	{
		BOTS_Print(clientNum, "You must be level 2 to place a 2nd laser.");
	}
	else if (pLevel < 3 && totalActive == 2)
	{
		BOTS_Print(clientNum, "You must be level 3 to place a 3rd laser.");
	}
	else if (totalActive == MAX_LASERS)
	{
		BOTS_Print(clientNum, "You have placed the maximum number of lasers.");
	}
	else
	{
		for (i=0;i<MAX_LASERS;i++)
		{
			laserState = &state->lasers[i];
			if (!laserState->active)
			{
				BOTS_Bodyguard_PlaceLaser(ent, laserState);
				break;
			}
		}
	}
}

void BOTS_BodyguardCommand_LaserOn(int clientNum)
{
	int i = 0;
	laserState_t *laserState;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);

	if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv(1, cmd, sizeof(cmd));
		laserState = BOTS_Bodyguard_FindLaserState_ByNumber(state, atoi(cmd));
		if (laserState && laserState->active)
			laserState->enable(laserState);
	}
	else 
	{
		for (i = 0;i < MAX_LASERS;i++)
		{
			laserState = &state->lasers[i];
			if (laserState->active)
				laserState->enable(laserState);
		}
	}
}

void BOTS_BodyguardCommand_LaserOff(int clientNum)
{
	int i = 0;
	laserState_t *laserState;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);

	if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv(1, cmd, sizeof(cmd));
		laserState = BOTS_Bodyguard_FindLaserState_ByNumber(state, atoi(cmd));
		if (laserState && laserState->active)
			laserState->disable(laserState);
	}
	else 
	{
		for (i = 0;i<MAX_LASERS;i++)
		{
			laserState = &state->lasers[i];
			if (laserState->active)
				laserState->disable(laserState);
		}
	}
}

void BOTS_BodyguardCommand_LaserKill(int clientNum)
{
	int i = 0;
	laserState_t *laserState;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);

	if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv(1, cmd, sizeof(cmd));
		laserState = BOTS_Bodyguard_FindLaserState_ByNumber(state, atoi(cmd));
		if (laserState && laserState->active)
			laserState->kill(laserState);
	}
	else 
	{
		for (i = 0;i < MAX_LASERS;i++)
		{
			laserState = &state->lasers[i];
			if (laserState->active)
				laserState->kill(laserState);
		}
	}
}

void BOTS_BodyguardCommand_Angle(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	laserState_t *laserState;

	if (pLevel < 3) 
	{
		BOTS_Print(clientNum, "You must be level 3 to angle your lasers.");
	}
	else if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv(1, cmd, sizeof(cmd));
		laserState = BOTS_Bodyguard_FindLaserState_ByNumber(state, atoi(cmd));
		if (laserState && laserState->active) 
			laserState->applyAngle(laserState, ent);
	}
	else
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv(0, cmd, sizeof(cmd));
		BOTS_Print(clientNum, va("usage: %s number", cmd));
	}
}

void BOTS_BodyguardCommand_Protect(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);

	if (pLevel < 1)
	{
		BOTS_Print(clientNum, "You must be level 1 to use protect.");
		return;
	}

	state->protect = state->protect ? qfalse : qtrue;

	if (state->protect)
		trap_SendServerCommand( clientNum, "print \"Protection enabled.\n\"");
	else
		trap_SendServerCommand( clientNum, "print \"Protection disabled.\n\"");
}

void BOTS_BodyguardCommand_Decoy(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	if (pLevel < 1)
	{
		BOTS_Print(clientNum, "You must be level 1 to use decoy.");
		return;
	}

	if (state->decoy > level.time)
	{
		trap_SendServerCommand( clientNum, "print \"Decoy currently active.\n\"");
	}
	else if (state->decoy + DECOY_COOLDOWN_SECONDS > level.time)
	{
		trap_SendServerCommand( clientNum, "print \"Decoy currently on cooldown.\n\"");
	}
	else
	{
		state->decoy = level.time + DECOY_ACTIVE_SECONDS;
		trap_SendServerCommand( clientNum, "print \"Decoy enabled.\n\"");
	}
}

void BOTS_BodyguardCommand_Pulse(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	bodyguardState_t *state = BOTS_Bodyguard_GetState(clientNum);
	if (pLevel < 4)
	{
		BOTS_Print(clientNum, "You must be level 4 to use pulse.");
		return;
	}

	state->pulse = state->pulse ? qfalse : qtrue;

	if (state->pulse)
		trap_SendServerCommand( clientNum, "print \"Laser Pulse enabled.\n\"");
	else
		trap_SendServerCommand( clientNum, "print \"Laser Pulse disabled.\n\"");
}

gentity_t *BOTS_Bodyguard_FindNearByProtector(gentity_t *ent)
{
	gentity_t *bodyguard;
	float d, maxDistance;
	int i = 0;
	if (ent->bots_class != CLASS_CAPTAIN)
		return (gentity_t *)NULL;

	for (i=0;i<level.maxclients;i++)
	{
		bodyguard = g_entities + i;
		if (bodyguard && 
			bodyguard->inuse &&
			bodyguard != ent && 
			bodyguard->client && 
			bodyguard->health > 0 &&
			bodyguard->client->ps.persistant[PERS_CLASS] == CLASS_BODYGUARD &&
			bodyguard->client->ps.persistant[PERS_TEAM] == ent->bots_team &&
			BOTS_Bodyguard_GetState(i)->protect)
		{
			int level = bodyguard->client->ps.persistant[PERS_LEVEL];
			if (level < 1)
				continue;
			d = Distance(ent->client->ps.origin, bodyguard->client->ps.origin);
			maxDistance = level > 1 ? 750.0f : 500.0f;
			if (d <= maxDistance)
				return bodyguard;
		}
	}
	return (gentity_t *)NULL;
}

void BOTS_Bodyguard_Modify_EntityState()
{
	gentity_t *ent;
	bodyguardState_t *state;
	int i=0;
	for (i=0;i<MAX_CLIENTS;i++)
	{
		ent = g_entities + i;
		if (ent && ent->inuse && ent->client && ent->health > 0 && 
			ent->bots_class == CLASS_BODYGUARD && 
			!ent->client->ps.powerups[PW_REDFLAG] && !ent->client->ps.powerups[PW_BLUEFLAG] && !ent->client->ps.powerups[PW_NEUTRALFLAG])
		{
			state = BOTS_Bodyguard_GetState(i);
			if (state->decoy >= level.time)
			{
				if (ent->bots_team == TEAM_BLUE)
					ent->s.powerups |= (1 << PW_REDFLAG);
				else if (ent->bots_team == TEAM_RED)
					ent->s.powerups |= (1 << PW_BLUEFLAG);
			}
		}
	}
}

void BOTS_Bodyguard_Rollback_EntityState()
{
	gentity_t *ent;
	bodyguardState_t *state;
	int i=0;
	for (i=0;i<MAX_CLIENTS;i++)
	{
		ent = g_entities + i;
		if (ent && ent->inuse && ent->client && ent->health > 0 && 
			ent->bots_class == CLASS_BODYGUARD && 
			!ent->client->ps.powerups[PW_REDFLAG] && !ent->client->ps.powerups[PW_BLUEFLAG] && !ent->client->ps.powerups[PW_NEUTRALFLAG])
		{
			state = BOTS_Bodyguard_GetState(i);
			if (state->decoy >= level.time)
			{
				if (ent->bots_team == TEAM_BLUE)
					ent->s.powerups &= ~(1 << PW_REDFLAG);
				else if (ent->bots_team == TEAM_RED)
					ent->s.powerups &= ~(1 << PW_BLUEFLAG);
			}
		}
	}
}