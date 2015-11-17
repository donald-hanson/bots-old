#include "g_local.h"

typedef struct captainState_s {
	bfgMode_t bfgMode;
} captainState_t;

captainState_t captainStates[MAX_CLIENTS];

captainState_t *BOTS_Captain_GetState(int clientNum)
{
	return &captainStates[clientNum];
}

void BOTS_Captain_Network(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	float distance = 0.0f;
	float maxDistance = 0.0f;
	captainState_t *state = BOTS_Captain_GetState(clientNum);

	trap_Net_WriteBits((int)state->bfgMode, 4);
}

void BOTS_Captain_DropPromote(int clientNum, qboolean launch)
{
	BOTS_Common_DropKey(clientNum, launch, qfalse);
}

void BOTS_Captain_Promote(int captainNum, int playerNum)
{
	int neededPoints;
	int points;
	gentity_t *captain = g_entities + captainNum;
	gentity_t *player = g_entities + playerNum;

	if (!captain || !captain->client)
		return;
	else if (!player || !player->client)
		BOTS_Print(captainNum, "Invalid player!");
	else if (player->bots_class == CLASS_CAPTAIN)
		BOTS_Print(captainNum, "You cannot promote yourself!");
	else if (captain->bots_team != player->bots_team)
		BOTS_Print(captainNum, va("%s" S_COLOR_WHITE " is not on your team!", player->client->pers.netname));
	else if (player->client->ps.persistant[PERS_LEVEL] == 4)
		BOTS_Print(captainNum, va("%s" S_COLOR_WHITE " is already level 4!", player->client->pers.netname));
	else
	{
		neededPoints = player->client->ps.persistant[PERS_LEVEL] + 1;
		points = BOTS_GetPromotionPoints(captain->bots_team);
		
		if (points < neededPoints)
			BOTS_Print(captainNum, va("%s" S_COLOR_WHITE " needs %d promotion points to be promoted!", player->client->pers.netname, neededPoints));
		else
		{
			//promote the player
			player->client->ps.persistant[PERS_LEVEL]++;

			//sync the captain's level
			BOTS_UpdateCaptainLevel(captain->bots_team);

			//remove the promotion points from the team
			BOTS_AddPromotionPoints(captain->bots_team, -1 * neededPoints);

			//announce to the team
			BOTS_Print_Team(captain->bots_team, va("%s" S_COLOR_WHITE " was promoted to level %d!", player->client->pers.netname, player->client->ps.persistant[PERS_LEVEL]));
		}
	}
}

void BOTS_Captain_Demote(int captainNum, int playerNum)
{
	gentity_t *captain = g_entities + captainNum;
	gentity_t *player = g_entities + playerNum;

	if (!captain || !captain->client)
		return;
	else if (!player || !player->client)
		BOTS_Print(captainNum, "Invalid player!");
	else if (player->bots_class == CLASS_CAPTAIN)
		BOTS_Print(captainNum, "You cannot demote yourself!");
	else if (captain->bots_team != player->bots_team)
		BOTS_Print(captainNum, va("%s" S_COLOR_WHITE " is not on your team!", player->client->pers.netname));
	else if (player->client->ps.persistant[PERS_LEVEL] == 0)
		BOTS_Print(captainNum, va("%s" S_COLOR_WHITE " is already level 0!", player->client->pers.netname));
	else
	{
		//demote the player
		player->client->ps.persistant[PERS_LEVEL]--;

		//sync the captain's level
		BOTS_UpdateCaptainLevel(captain->bots_team);

		//add the promotion points back to the team
		BOTS_AddPromotionPoints(captain->bots_team, player->client->ps.persistant[PERS_LEVEL] + 1);

		//announce to the team
		BOTS_Print_Team(captain->bots_team, va("%s" S_COLOR_WHITE " was demoted to level %d!", player->client->pers.netname, player->client->ps.persistant[PERS_LEVEL]));
	}
}

void BOTS_CaptainSpawn(gentity_t *ent)
{
	if (BOTS_GetTeamPromotionKey(ent->bots_team) == NULL)
		ent->client->ps.stats[STAT_KEY] = ent->bots_team == TEAM_RED ? KEY_RED_PROMO : KEY_BLUE_PROMO;
}

//captain is the killed entity
void BOTS_CaptainDeath(gentity_t *killed, gentity_t *killedBy, gentity_t *killer, int damage, int meansOfDeath)
{
	team_t enemyTeam = killed->bots_team == TEAM_RED ? TEAM_BLUE : TEAM_RED;
	//award a promotion point to the enemy team if the enemy team has a captain even if the death was self inflicted
	if (BOTS_GetTeamCaptain(enemyTeam))
		BOTS_AddPromotionPoints(enemyTeam, 1);

	//drop the promotion key if the captain is carrying it
	if (BOTS_GetTeamPromotionKey(killed->bots_team) == NULL)
		BOTS_Captain_DropPromote(killed->s.clientNum, qfalse);
}

void BOTS_CaptainCommand_DropPromote(int clientNum)
{
	gentity_t*  ent = g_entities + clientNum;

	if (ent->client->ps.stats[STAT_KEY] == KEY_NONE)
		BOTS_Print(clientNum, "You're not carrying the promotion key!");
	else
		BOTS_Captain_DropPromote(clientNum, qtrue);
}

void BOTS_CaptainCommand_Promote(int clientNum)
{
	gentity_t*  ent = g_entities + clientNum;

	if (BOTS_GetTeamPromotionKey(ent->bots_team) != NULL)
		BOTS_Print(clientNum, "You're not carrying the promotion key!");
	else if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv( 1, cmd, sizeof( cmd ) );
		BOTS_Captain_Promote(clientNum, atoi( cmd ));
	}
	else
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv( 0, cmd, sizeof( cmd ) );
		BOTS_Print(clientNum, va("usage: %s playerid", cmd));
	}
}

void BOTS_CaptainCommand_Demote(int clientNum)
{
	gentity_t*  ent = g_entities + clientNum;

	if (BOTS_GetTeamPromotionKey(ent->bots_team) != NULL)
		BOTS_Print(clientNum, "You're not carrying the promotion key!");
	else if (trap_Argc() == 2)
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv( 1, cmd, sizeof( cmd ) );
		BOTS_Captain_Demote(clientNum, atoi( cmd ));
	}
	else
	{
		char cmd[MAX_TOKEN_CHARS];
		trap_Argv( 0, cmd, sizeof( cmd ) );
		BOTS_Print(clientNum, va("usage: %s playerid", cmd));
	}
}

void BOTS_Captain_SendScoutMessage(int clientNum, qboolean teamMembers) 
{
	gentity_t*  ent = g_entities + clientNum;
	char*		stats;

	stats = va("Class           Female Male\n");
	stats = va("%s-----           ------ ----\n", stats);
	stats = va("%sCaptain         %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_CAPTAIN), BOTS_CountPlayers(TEAM_RED, CLASS_CAPTAIN));
	stats = va("%sBodyguard       %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_BODYGUARD), BOTS_CountPlayers(TEAM_RED, CLASS_BODYGUARD));
	stats = va("%sSniper          %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_SNIPER), BOTS_CountPlayers(TEAM_RED, CLASS_SNIPER));
	stats = va("%sSoldier         %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_SOLDIER), BOTS_CountPlayers(TEAM_RED, CLASS_SOLDIER));
	stats = va("%sBerzerker       %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_BERZERKER), BOTS_CountPlayers(TEAM_RED, CLASS_BERZERKER));
	stats = va("%sInfiltrator     %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_INFILTRATOR), BOTS_CountPlayers(TEAM_RED, CLASS_INFILTRATOR));
	stats = va("%sKamikazee       %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_KAMIKAZEE), BOTS_CountPlayers(TEAM_RED, CLASS_KAMIKAZEE));
	stats = va("%sNurse           %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_NURSE), BOTS_CountPlayers(TEAM_RED, CLASS_NURSE));
	stats = va("%sScientist       %3i    %3i \n", stats, BOTS_CountPlayers(TEAM_BLUE, CLASS_SCIENTIST), BOTS_CountPlayers(TEAM_RED, CLASS_SCIENTIST));

	if (teamMembers == qtrue) 
	{
		BOTS_Print_Team(ent->bots_team, stats);
	} 
	else 
	{
		BOTS_Print(clientNum, stats);
	}
}

void BOTS_CaptainCommand_ScoutAll(int clientNum) 
{
	BOTS_Captain_SendScoutMessage(clientNum, qtrue);
}

void BOTS_CaptainCommand_Scout(int clientNum) 
{
	BOTS_Captain_SendScoutMessage(clientNum, qfalse);
}

void BOTS_BFG_FireSplit(gentity_t *ent, int horizontal, int vertical)
{
	gentity_t *m1, *m2, *m3;
	vec3_t	forward, right, up, forward2, forward3;
	vec3_t	muzzle;
	// set aiming directions
	AngleVectors(ent->client->ps.viewangles, forward, right, up);
	CalcMuzzlePointOrigin(ent, ent->client->oldOrigin, forward, right, up, muzzle);
	m1 = fire_bfg(ent, muzzle, forward, 900);

	VectorScale(forward, 1000, forward2);
	VectorMA(forward2, horizontal, right, forward2);
	VectorMA(forward2, vertical, up, forward2);
	m2 = fire_bfg(ent, muzzle, forward2, 900);

	VectorScale(forward, 1000, forward3);
	VectorMA(forward3, -1 * horizontal, right, forward3);
	VectorMA(forward3, vertical, up, forward3);
	m3 = fire_bfg(ent, muzzle, forward3, 900);
}

void BOTS_BFG_FireSplit1(gentity_t *ent)
{
	BOTS_BFG_FireSplit(ent, 60, -60);
}

void BOTS_BFG_FireSplit2(gentity_t *ent)
{
	BOTS_BFG_FireSplit(ent, 70, -20);
}

void BOTS_BFG_FireSplit3(gentity_t *ent)
{
	BOTS_BFG_FireSplit(ent, 95, -20);
}

typedef struct bfgModeInfo_s {
	bfgMode_t bfgMode;
	void(*handler)(gentity_t *ent);
} bfgModeInfo_t;

bfgModeInfo_t bfgModeInfos[] = {
	{ BFG_NORMAL,				NULL },
	{ BFG_SPLIT1,				BOTS_BFG_FireSplit1 },
	{ BFG_SPLIT2,				BOTS_BFG_FireSplit2 },
	{ BFG_SPLIT3,				BOTS_BFG_FireSplit3 },
	{ BFG_NUM_BFGMODES,	NULL },
};

qboolean BOTS_Captain_FireWeapon(gentity_t *ent)
{
	captainState_t *state = BOTS_Captain_GetState(ent->s.clientNum);
	bfgModeInfo_t *info = &bfgModeInfos[state->bfgMode];
	if (ent->s.weapon == WP_BFG && info->handler)
	{
		info->handler(ent);
		return qtrue;
	}
	return qfalse;
}

void BOTS_Captain_SetBFGMode(int clientNum, bfgMode_t bfgMode)
{
	captainState_t *state = BOTS_Captain_GetState(clientNum);
	if (state->bfgMode == bfgMode)
		state->bfgMode = BFG_NORMAL;
	else
		state->bfgMode = bfgMode;
}

void BOTS_CaptainCommand_Split1(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	if (pLevel < 4)
	{
		BOTS_Print(clientNum, "You must be level 4 to use split.");
		return;
	}

	BOTS_Captain_SetBFGMode(clientNum, BFG_SPLIT1);
}

void BOTS_CaptainCommand_Split2(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	if (pLevel < 4)
	{
		BOTS_Print(clientNum, "You must be level 4 to use split.");
		return;
	}

	BOTS_Captain_SetBFGMode(clientNum, BFG_SPLIT2);
}

void BOTS_CaptainCommand_Split3(int clientNum)
{
	gentity_t *ent = g_entities + clientNum;
	int pLevel = ent->client->ps.persistant[PERS_LEVEL];
	if (pLevel < 4)
	{
		BOTS_Print(clientNum, "You must be level 4 to use split.");
		return;
	}

	BOTS_Captain_SetBFGMode(clientNum, BFG_SPLIT3);
}