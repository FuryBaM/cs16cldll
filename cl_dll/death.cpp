/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
//
// death notice
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>
#include "draw_util.h"
#include "ui/common/hud_style.h"

float color[3];

struct DeathNoticeItem {
	char szKiller[MAX_PLAYER_NAME_LENGTH * 2];
	char szVictim[MAX_PLAYER_NAME_LENGTH * 2];
	int iId;	// the index number of the associated sprite
	bool bSuicide;
	bool bTeamKill;
	bool bNonPlayerKill;
	float flDisplayTime;
	float* KillerColor;
	float* VictimColor;
	int iHeadShotId;
	int iKiller;
	int iVictim;
	float flStartTime;
};

#define MAX_DEATHNOTICES	6
static int DEATHNOTICE_DISPLAY_TIME = 6;

// The stock feed keeps four notices in a 20 px row starting 32 units down; the
// reworked one is taller and holds more.
#define DEATHNOTICE_TOP		38
#define DEATHNOTICE_ROW_HEIGHT 24
#define VANILLA_DEATHNOTICES 4
#define VANILLA_DEATHNOTICE_TOP 32
#define VANILLA_ROW_HEIGHT 20
// Keep the feed off the very edge of the screen; XRES scales it with the
// resolution so the gap looks the same at 640 and at 1920.
#define DEATHNOTICE_RIGHT_MARGIN 8

static int DeathNoticeCapacity(void)
{
	return CS16_HudStyleModern(CS16_HUD_KILLFEED) ? MAX_DEATHNOTICES
		: VANILLA_DEATHNOTICES;
}

DeathNoticeItem rgDeathNoticeList[MAX_DEATHNOTICES + 1];

DECLARE_MESSAGE(m_DeathNotice, DeathMsg);

int CHudDeathNotice::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(DeathMsg);

	hud_deathnotice_time = CVAR_CREATE("hud_deathnotice_time", "6", FCVAR_ARCHIVE);
	m_iFlags = 0;

	return 1;
}


void CHudDeathNotice::InitHUDData(void)
{
	memset(rgDeathNoticeList, 0, sizeof(rgDeathNoticeList));
}


int CHudDeathNotice::VidInit(void)
{
	m_HUD_d_skull = gHUD.GetSpriteIndex("d_skull");
	m_HUD_d_headshot = gHUD.GetSpriteIndex("d_headshot");

	return 1;
}

int CHudDeathNotice::Draw(float flTime)
{
	int x, y, r, g, b, i;

	const bool modern = CS16_HudStyleModern(CS16_HUD_KILLFEED);
	const int capacity = DeathNoticeCapacity();
	const int rowHeight = modern ? DEATHNOTICE_ROW_HEIGHT : VANILLA_ROW_HEIGHT;
	const int topOffset = modern ? DEATHNOTICE_TOP : VANILLA_DEATHNOTICE_TOP;

	for (i = 0; i < capacity; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break;  // we've gone through them all

		if (rgDeathNoticeList[i].flDisplayTime < flTime)
		{ // display time has expired
			// remove the current item from the list
			memmove(&rgDeathNoticeList[i], &rgDeathNoticeList[i + 1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i));
			i--;  // continue on the next item;  stop the counter getting incremented
			continue;
		}

		DeathNoticeItem& notice = rgDeathNoticeList[i];
		float fade = 1.0f;

		if (modern)
		{
			const float remaining = notice.flDisplayTime - flTime;
			if (remaining < 0.75f)
				fade = max(0.0f, remaining / 0.75f);
			const float age = flTime - notice.flStartTime;
			if (age < 0.12f)
				fade *= max(0.0f, age / 0.12f);
		}
		else
		{
			notice.flDisplayTime = min(notice.flDisplayTime,
				flTime + DEATHNOTICE_DISPLAY_TIME);
		}

		{
			if (!g_iUser1)
				y = YRES(topOffset) + 2 + (rowHeight * i);
			else
				y = ScreenHeight / 5 + 2 + (rowHeight * i);

			int id = (notice.iId == -1) ? m_HUD_d_skull : notice.iId;
			const int victimWidth = notice.bNonPlayerKill ? 0 :
				DrawUtils::ConsoleStringLen(notice.szVictim);
			const int killerWidth = notice.bSuicide ? 0 :
				DrawUtils::ConsoleStringLen(notice.szKiller);
			const int weaponWidth = gHUD.GetSpriteRect(id).Width();
			const int headshotWidth = notice.iHeadShotId ?
				gHUD.GetSpriteRect(m_HUD_d_headshot).Width() : 0;
			const int contentWidth = killerWidth + weaponWidth + headshotWidth +
				victimWidth + (notice.bSuicide ? 0 : 5);
			x = ScreenWidth - contentWidth - XRES( DEATHNOTICE_RIGHT_MARGIN );

			const bool localEvent = notice.iKiller == gHUD.m_Scoreboard.m_iPlayerNum ||
				notice.iVictim == gHUD.m_Scoreboard.m_iPlayerNum;

			if (modern)
			{
				const int panelAlpha = static_cast<int>((localEvent ? 150 : 105) * fade);
				FillRGBABlend(x - 7, y - 3, contentWidth + 13,
					rowHeight - 2, 0, 0, 0, panelAlpha);
				if (localEvent)
					FillRGBABlend(x - 7, y - 3, 3, rowHeight - 2,
						255, 160, 0, static_cast<int>(230 * fade));
			}

			if (!notice.bSuicide)
			{
				if (notice.KillerColor)
					DrawUtils::SetConsoleTextColor(notice.KillerColor[0] * fade,
						notice.KillerColor[1] * fade, notice.KillerColor[2] * fade);
				x = 5 + DrawUtils::DrawConsoleString(x, y, notice.szKiller);
			}

			if (modern)
			{
				r = localEvent ? 255 : 235; g = localEvent ? 175 : 110; b = 25;
				if (notice.bTeamKill)
				{
					r = 255; g = 45; b = 45;
				}
			}
			else
			{
				r = 255; g = 80; b = 0;
				if (notice.bTeamKill)
				{
					r = 10; g = 240; b = 10;  // display it in sickly green
				}
			}
			r = static_cast<int>(r * fade);
			g = static_cast<int>(g * fade);
			b = static_cast<int>(b * fade);

			SPR_Set(gHUD.GetSprite(id), r, g, b);
			SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(id));
			x += weaponWidth;

			if (notice.iHeadShotId)
			{
				SPR_Set(gHUD.GetSprite(m_HUD_d_headshot), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_d_headshot));
				x += (gHUD.GetSpriteRect(m_HUD_d_headshot).Width());
			}

			if (!notice.bNonPlayerKill)
			{
				if (notice.VictimColor)
					DrawUtils::SetConsoleTextColor(notice.VictimColor[0] * fade,
						notice.VictimColor[1] * fade, notice.VictimColor[2] * fade);
				x = DrawUtils::DrawConsoleString(x, y, notice.szVictim);
			}
		}
	}

	if (i == 0)
		m_iFlags &= ~HUD_DRAW; // disable hud item

	return 1;
}

// This message handler may be better off elsewhere
int CHudDeathNotice::MsgFunc_DeathMsg(const char* pszName, int iSize, void* pbuf)
{
	m_iFlags |= HUD_DRAW;

	BufferReader reader(pszName, pbuf, iSize);

	int killer = reader.ReadByte();
	int victim = reader.ReadByte();
	int headshot = reader.ReadByte();

	char killedwith[32];
	strncpy(killedwith, "d_", sizeof(killedwith));
	strncat(killedwith, reader.ReadString(), sizeof(killedwith) - 2);

	//if (gViewPort)
	//	gViewPort->DeathMsg( killer, victim );
	gHUD.m_Scoreboard.DeathMsg(killer, victim);

	gHUD.m_Spectator.DeathMessage(victim);
	const int capacity = DeathNoticeCapacity();
	int i;
	for (i = 0; i < capacity; i++)
	{
		if (rgDeathNoticeList[i].iId == 0)
			break;
	}
	if (i == capacity)
	{ // move the rest of the list forward to make room for this item
		memmove(rgDeathNoticeList, rgDeathNoticeList + 1, sizeof(DeathNoticeItem) * capacity);
		i = capacity - 1;
	}
	memset(&rgDeathNoticeList[i], 0, sizeof(rgDeathNoticeList[i]));

	//if (gViewPort)
		//gViewPort->GetAllPlayersInfo();
	gHUD.m_Scoreboard.GetAllPlayersInfo();

	// Get the Killer's name
	const char* killer_name = killer >= 1 && killer <= MAX_PLAYERS ?
		g_PlayerInfoList[killer].name : NULL;
	if (!killer_name)
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].KillerColor = GetClientColor(killer);
		strncpy(rgDeathNoticeList[i].szKiller, killer_name, MAX_PLAYER_NAME_LENGTH);
		rgDeathNoticeList[i].szKiller[MAX_PLAYER_NAME_LENGTH - 1] = 0;
	}

	// Get the Victim's name
	const char* victim_name = NULL;
	// If victim is -1, the killer killed a specific, non-player object (like a sentrygun)
	if (victim >= 1 && victim <= MAX_PLAYERS)
		victim_name = g_PlayerInfoList[victim].name;
	if (!victim_name)
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].VictimColor = GetClientColor(victim);
		strncpy(rgDeathNoticeList[i].szVictim, victim_name, MAX_PLAYER_NAME_LENGTH);
		rgDeathNoticeList[i].szVictim[MAX_PLAYER_NAME_LENGTH - 1] = 0;
	}

	// Is it a non-player object kill?
	if (victim == 255)
	{
		rgDeathNoticeList[i].bNonPlayerKill = true;

		// Store the object's name in the Victim slot (skip the d_ bit)
		strncpy(rgDeathNoticeList[i].szVictim, killedwith + 2, sizeof(killedwith));
	}
	else
	{
		if (killer == victim || killer == 0)
			rgDeathNoticeList[i].bSuicide = true;

		if (!strncmp(killedwith, "d_teammate", sizeof(killedwith)))
			rgDeathNoticeList[i].bTeamKill = true;
	}

	rgDeathNoticeList[i].iHeadShotId = headshot;
	rgDeathNoticeList[i].iKiller = killer;
	rgDeathNoticeList[i].iVictim = victim;
	rgDeathNoticeList[i].flStartTime = gHUD.m_flTime;

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex(killedwith);

	rgDeathNoticeList[i].iId = spr;

	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + hud_deathnotice_time->value;


	if (rgDeathNoticeList[i].bNonPlayerKill)
	{
		ConsolePrint(rgDeathNoticeList[i].szKiller);
		ConsolePrint(" killed a ");
		ConsolePrint(rgDeathNoticeList[i].szVictim);
		ConsolePrint("\n");
	}
	else
	{
		// record the death notice in the console
		if (rgDeathNoticeList[i].bSuicide)
		{
			ConsolePrint(rgDeathNoticeList[i].szVictim);

			if (!strncmp(killedwith, "d_world", sizeof(killedwith)))
			{
				ConsolePrint(" died");
			}
			else
			{
				ConsolePrint(" killed self");
			}
		}
		else if (rgDeathNoticeList[i].bTeamKill)
		{
			ConsolePrint(rgDeathNoticeList[i].szKiller);
			ConsolePrint(" killed his teammate ");
			ConsolePrint(rgDeathNoticeList[i].szVictim);
		}
		else
		{
			if (headshot)
				ConsolePrint("*** ");
			ConsolePrint(rgDeathNoticeList[i].szKiller);
			ConsolePrint(" killed ");
			ConsolePrint(rgDeathNoticeList[i].szVictim);
		}

		if (*killedwith && (*killedwith > 13) && strncmp(killedwith, "d_world", sizeof(killedwith)) && !rgDeathNoticeList[i].bTeamKill)
		{
			if (headshot)
				ConsolePrint(" with a headshot from ");
			else
				ConsolePrint(" with ");

			ConsolePrint(killedwith + 2); // skip over the "d_" part
		}

		if (headshot) ConsolePrint(" ***");
		ConsolePrint("\n");
	}

	return 1;
}




