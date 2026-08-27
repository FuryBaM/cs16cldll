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
// menu.cpp
//
// generic menu handler
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cs_vgui.h"
#include "cs_vgui2.h"
#include "draw_util.h"

#include <string.h>
#include <stdio.h>

#define MAX_MENU_STRING	512
char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];
static int g_pendingClassMenu = 0;
static int g_pendingSelectionMenuRequest = 0;

int KB_ConvertString(char* in, char** ppout);

DECLARE_MESSAGE(m_Menu, ShowMenu);
DECLARE_MESSAGE(m_Menu, VGUIMenu);
DECLARE_MESSAGE(m_Menu, BuyClose);
DECLARE_MESSAGE(m_Menu, AllowSpec);

int CHudMenu::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(ShowMenu);
	HOOK_MESSAGE(VGUIMenu);
	HOOK_MESSAGE(BuyClose);
	HOOK_MESSAGE(AllowSpec);

	InitHUDData();

	return 1;
}

void CHudMenu::InitHUDData(void)
{
	g_pendingClassMenu = 0;
	g_pendingSelectionMenuRequest = 0;
	m_fMenuDisplayed = 0;
	m_bAllowSpec = true;
	m_bitsValidSlots = 0;
	Reset();
}

void CHudMenu::Reset(void)
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = FALSE;
}

int CHudMenu::VidInit(void)
{
	return 1;
}


/*=================================
  ParseEscapeToken

  Interprets the given escape token (backslash followed by a letter). The
  first character of the token must be a backslash.  The second character
  specifies the operation to perform:

   \w : White text (this is the default)
   \d : Dim (gray) text
   \y : Yellow text
   \r : Red text
   \R : Right-align (just for the remainder of the current line)
=================================*/

static int menu_r, menu_g, menu_b, menu_x, menu_ralign;

static inline const char* ParseEscapeToken(const char* token)
{
	if (*token != '\\')
		return token;

	token++;

	switch (*token)
	{
	case '\0':
		return token;

	case 'w':
		menu_r = 255;
		menu_g = 255;
		menu_b = 255;
		break;

	case 'd':
		menu_r = 100;
		menu_g = 100;
		menu_b = 100;
		break;

	case 'y':
		menu_r = 255;
		menu_g = 210;
		menu_b = 64;
		break;

	case 'r':
		menu_r = 210;
		menu_g = 24;
		menu_b = 0;
		break;

	case 'R':
		menu_x = ScreenWidth / 2;
		menu_ralign = TRUE;
		break;
	}

	return ++token;
}


int CHudMenu::Draw(float flTime)
{
	// check for if menu is set to disappear
	if (m_flShutoffTime > 0)
	{
		if (m_flShutoffTime <= gHUD.m_flTime)
		{  // times up, shutoff
			m_fMenuDisplayed = 0;
			m_iFlags &= ~HUD_ACTIVE;
			return 1;
		}
	}

	//// don't draw the menu if the scoreboard is being shown
	//if (gViewPort && gViewPort->IsScoreBoardVisible())
	//	return 1;


	// draw the menu, along the left-hand side of the screen

	// count the number of newlines
	int nlc = 0;
	int i;
	for (i = 0; i < MAX_MENU_STRING && g_szMenuString[i] != '\0'; i++)
	{
		if (g_szMenuString[i] == '\n')
			nlc++;
	}

	// DrawUtils picks the font for HUD strings, so take the line height from
	// whichever one it ended up using.
	const int nFontHeight = max(12, DrawUtils::HudTextTall());

	// center it
	int y = (ScreenHeight / 2) - ((nlc / 2) * nFontHeight) - (3 * nFontHeight + nFontHeight / 3); // make sure it is above the say text

	menu_r = 255;
	menu_g = 255;
	menu_b = 255;
	menu_x = 20;
	menu_ralign = FALSE;

	const char* sptr = g_szMenuString;

	while (*sptr != '\0')
	{
		if (*sptr == '\\')
		{
			sptr = ParseEscapeToken(sptr);
		}
		else if (*sptr == '\n')
		{
			menu_ralign = FALSE;
			menu_x = 20;
			y += nFontHeight;

			sptr++;
		}
		else
		{
			char menubuf[80];
			const char* ptr = sptr;
			while (*sptr != '\0' && *sptr != '\n' && *sptr != '\\')
			{
				sptr++;
			}
			strncpy(menubuf, ptr, min((sptr - ptr), (int)sizeof(menubuf)));
			menubuf[min((sptr - ptr), (int)(sizeof(menubuf) - 1))] = '\0';
			if (menu_ralign)
			{
				// IMPORTANT: Right-to-left rendered text does not parse escape tokens!
				menu_x = gHUD.DrawHudStringReverse(menu_x, y, 0,
					menubuf, menu_r, menu_g, menu_b);
			}
			else
			{
				menu_x = gHUD.DrawHudString(menu_x, y, ScreenWidth / 2,
					menubuf, menu_r, menu_g, menu_b);
			}
		}
	}

	return 1;
}

namespace
{
double g_lastHandledMenuEscape = -1.0;
const double kDuplicateEscapeWindow = 0.1;

double CS16_EscapeClock(void)
{
	return gEngfuncs.GetAbsoluteTime
		? gEngfuncs.GetAbsoluteTime()
		: (double)gHUD.m_flTime;
}
}

void CS16_SetClassSelectionPending(int menuId)
{
	g_pendingClassMenu = (menuId == 26 || menuId == 27) ? menuId : 0;
}

void CS16_TrackSelectionMenuCommand(const char* command)
{
	if (!command)
		return;

	// A class choice completes this flow. A jointeam request starts a fresh
	// server-driven flow, whose VGUIMenu message will set it pending again when
	// a class choice is required.
	if (!strncmp(command, "joinclass ", 10) ||
		!strncmp(command, "jointeam ", 9))
		g_pendingClassMenu = 0;
}

bool CS16_ReopenPendingSelectionMenu(const char* currentBinding)
{
	if (!g_pendingClassMenu || !currentBinding ||
		g_pendingSelectionMenuRequest || CS16VGUI_IsMenuVisible() ||
		gHUD.m_Menu.m_fMenuDisplayed)
		return false;

	const char* command = strstr(currentBinding, "chooseteam");
	if (!command)
		return false;

	const char before = command == currentBinding ? '\0' : command[-1];
	const char after = command[10];
	const bool validBefore = before == '\0' || before == ';' ||
		before == ' ' || before == '\t';
	const bool validAfter = after == '\0' || after == ';' ||
		after == ' ' || after == '\t';
	if (!validBefore || !validAfter)
		return false;

	g_pendingSelectionMenuRequest = g_pendingClassMenu;
	return true;
}

int CS16_ConsumePendingSelectionMenuRequest(void)
{
	const int menuId = g_pendingSelectionMenuRequest;
	g_pendingSelectionMenuRequest = 0;
	return menuId;
}

void CS16_MarkMenuEscapeHandled(void)
{
	g_lastHandledMenuEscape = CS16_EscapeClock();
}

bool CS16_ConsumeMenuEscapeHandled(void)
{
	if (g_lastHandledMenuEscape < 0.0)
		return false;

	const double elapsed = CS16_EscapeClock() - g_lastHandledMenuEscape;
	g_lastHandledMenuEscape = -1.0;
	const bool consumed = elapsed >= 0.0 && elapsed <= kDuplicateEscapeWindow;
	return consumed;
}
bool CS16_CloseTopmostMenu(void)
{
	if (CS16VGUI_IsMenuVisible())
	{
		CS16VGUI_HideMenu();
		CS16_MarkMenuEscapeHandled();
		return true;
	}

	if (gHUD.m_Menu.m_fMenuDisplayed)
	{
		gHUD.m_Menu.m_fMenuDisplayed = 0;
		gHUD.m_Menu.m_iFlags &= ~HUD_ACTIVE;
		gHUD.m_Menu.m_flShutoffTime = -1;
		CS16_MarkMenuEscapeHandled();
		return true;
	}

	return false;
}

// selects an item from the menu
void CHudMenu::SelectMenuItem(int menu_item)
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ((menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item - 1))))
	{
		char szbuf[32];
		sprintf(szbuf, "menuselect %d\n", menu_item);
		ClientCmd(szbuf);

		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
	}
}


// Message handler for ShowMenu message
// takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, FALSE if it's the last string
//		string: menu string to display
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudMenu::MsgFunc_ShowMenu(const char* pszName, int iSize, void* pbuf)
{
	CS16VGUI_HideMenu();

	char* temp = NULL;
	BufferReader reader(pszName, pbuf, iSize);

	m_bitsValidSlots = reader.ReadShort();
	int DisplayTime = reader.ReadChar();
	int NeedMore = reader.ReadByte();

	if (DisplayTime > 0)
		m_flShutoffTime = DisplayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if (m_bitsValidSlots)
	{
		if (!m_fWaitingForMore) // this is the start of a new menu
		{
			strncpy(g_szPrelocalisedMenuString, reader.ReadString(), MAX_MENU_STRING);
		}
		else
		{  // append to the current menu string
			strncat(g_szPrelocalisedMenuString, reader.ReadString(), MAX_MENU_STRING - strlen(g_szPrelocalisedMenuString));
		}
		g_szPrelocalisedMenuString[MAX_MENU_STRING - 1] = 0;  // ensure null termination (strncat/strncpy does not)

		if (!NeedMore)
		{  // we have the whole string, so we can localise it now
			strncpy(g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString(g_szPrelocalisedMenuString), MAX_MENU_STRING);
			g_szMenuString[MAX_MENU_STRING - 1] = '\0';

			// Swap in characters
			if (KB_ConvertString(g_szMenuString, &temp))
			{
				strncpy(g_szMenuString, temp, MAX_MENU_STRING);
				g_szMenuString[MAX_MENU_STRING - 1] = '\0';
				free(temp);
			}
		}

		m_fMenuDisplayed = 1;
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		m_fMenuDisplayed = 0; // no valid slots means that the menu should be turned off
		m_iFlags &= ~HUD_ACTIVE;
	}

	m_fWaitingForMore = NeedMore;

	return 1;
}

// Route supported stock CS menu IDs to the native VGUI2/VGUI1 viewport. If a menu is
// not implemented yet, switch this session back to ShowMenu instead of leaving
// the player stuck behind an invisible panel.
int CHudMenu::MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf)
{
	BufferReader reader(pszName, pbuf, iSize);
	const int menu = iSize > 0 ? reader.ReadByte() : -1;
	if (iSize >= 3)
		m_bitsValidSlots = reader.ReadShort();
	if (CS16VGUI_ShowMenu(menu))
	{
		if (menu == 26 || menu == 27)
			CS16_SetClassSelectionPending(menu);
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}

	CS16VGUI_DisableForSession();
	gEngfuncs.Cvar_SetValue("_vgui_menus", 0.0f);
	gEngfuncs.Con_Printf(
		"[CS16 GoldSrc] Rejected VGUIMenu %d; text menus are now enabled. Reopen the menu.\n",
		menu);

	return 1;
}

int CHudMenu::MsgFunc_BuyClose(const char*, int, void*)
{
	CS16VGUI_HideMenu();
	m_fMenuDisplayed = 0;
	m_iFlags &= ~HUD_ACTIVE;
	return 1;
}

int CHudMenu::MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf)
{
	BufferReader reader(pszName, pbuf, iSize);
	m_bAllowSpec = iSize > 0 ? reader.ReadByte() != 0 : true;
	return 1;
}
