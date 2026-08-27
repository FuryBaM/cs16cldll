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
// text_message.cpp
//
// implementation of CHudTextMessage class
//
// this class routes messages through titles.txt for localisation
//
#include <cctype>

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "ctype.h"
#include "draw_util.h"
#include "cs_localize.h"

DECLARE_MESSAGE(m_TextMessage, TextMsg);

#define MAX_TEXTMSG_STRING 512

namespace
{
char g_szCenterMessage[MAX_TEXTMSG_STRING];
float g_flCenterMessageUntil = 0.0f;
bool g_bCenterTestCommandRegistered = false;

void SetCenterMessage(const char* text)
{
	if (!text)
		text = "";

	strncpy(g_szCenterMessage, text, sizeof(g_szCenterMessage));
	g_szCenterMessage[sizeof(g_szCenterMessage) - 1] = '\0';

	// A message whose arguments never arrived must not put the raw "%s1" on
	// the screen; drop the placeholder and the gap it leaves behind, so the
	// text still reads as a plain sentence.
	CS16_StripFormatPlaceholders(g_szCenterMessage);
	const char* trimmed = g_szCenterMessage;
	while (*trimmed == ' ')
		++trimmed;
	if (trimmed != g_szCenterMessage)
		memmove(g_szCenterMessage, trimmed, strlen(trimmed) + 1);
	g_flCenterMessageUntil = gHUD.m_flTime + 3.0f;
	gHUD.m_TextMessage.m_iFlags |= HUD_DRAW;
}

// cs_test_centertext [token] [arg1..arg4]
//
// Runs a message through the same lookup and %s1 substitution the server
// path uses, so a token that renders wrong can be reproduced without a
// server: "cs_test_centertext #Cant_buy 90".
void CS16_TestCenterMessage_f()
{
	const int argc = gEngfuncs.Cmd_Argc();
	const char* token = argc > 1 ? gEngfuncs.Cmd_Argv(1) : "#Bomb_Planted";

	static char args[4][MAX_TEXTMSG_STRING];
	const char* arguments[4] = { args[0], args[1], args[2], args[3] };
	for (int i = 0; i < 4; i++)
	{
		const char* value = argc > i + 2 ? gEngfuncs.Cmd_Argv(i + 2) : "";
		strncpy(args[i], CS16_Localize(value), MAX_TEXTMSG_STRING);
		args[i][MAX_TEXTMSG_STRING - 1] = 0;
	}

	char text[MAX_TEXTMSG_STRING];
	CS16_LocalizeFormat(text, sizeof(text), CS16_Localize(token),
		arguments, 4);
	SetCenterMessage(text);
}
}

int CHudTextMessage::Init(void)
{
	HOOK_MESSAGE(TextMsg);

	gHUD.AddHudElem(this);
	m_iFlags = 0;
	g_szCenterMessage[0] = '\0';
	g_flCenterMessageUntil = 0.0f;

	if (!g_bCenterTestCommandRegistered && gEngfuncs.pfnAddCommand)
	{
		gEngfuncs.pfnAddCommand("cs_test_centertext", CS16_TestCenterMessage_f);
		g_bCenterTestCommandRegistered = true;
	}

	return 1;
}

void CHudTextMessage::Reset(void)
{
	CS16_LocalizeRetryIfEmpty();

	g_szCenterMessage[0] = '\0';
	g_flCenterMessageUntil = 0.0f;
	m_iFlags &= ~HUD_DRAW;
}

int CHudTextMessage::Draw(float flTime)
{
	if (!g_szCenterMessage[0] || flTime >= g_flCenterMessageUntil)
	{
		Reset();
		return 1;
	}

	char text[MAX_TEXTMSG_STRING];
	strncpy(text, g_szCenterMessage, sizeof(text));
	text[sizeof(text) - 1] = '\0';

	int lineCount = 1;
	for (const char* p = text; *p; ++p)
	{
		if (*p == '\n')
			++lineCount;
	}

	const int lineHeight = max(12, gHUD.GetCharHeight());
	int y = ScreenHeight / 3 - ((lineCount - 1) * lineHeight) / 2;
	char* line = text;
	while (line)
	{
		char* next = strchr(line, '\n');
		if (next)
			*next++ = '\0';

		const int width = DrawUtils::ConsoleStringLen(line);
		DrawUtils::SetConsoleTextColor(1.0f, 1.0f, 1.0f);
		DrawUtils::DrawConsoleString((ScreenWidth - width) / 2, y, line);
		y += lineHeight;
		line = next;
	}

	return 1;
}

// Searches through the string for any msg names (indicated by a '#')
// any found are looked up in titles.txt and the new message substituted
// the new value is pushed into dst_buffer
char* CHudTextMessage::LocaliseTextString(const char* msg, char* dst_buffer, int buffer_size)
{
	if (!msg || !dst_buffer || buffer_size <= 0)
		return dst_buffer;

	char* dst = dst_buffer;
	const char* src = msg;

	auto putc = [&](char c) {
		if (buffer_size > 1) { *dst++ = c; --buffer_size; }
		};

	while (*src && buffer_size > 1)
	{
		if (*src == '#')
		{
			++src;
			char word_buf[256]; int wi = 0;
			while (*src&& wi < (int)sizeof(word_buf) - 1)
			{
				unsigned char ch = (unsigned char)*src;
				if (!(std::isalnum(ch) || ch == '_')) break;
				word_buf[wi++] = *src++;
			}
			word_buf[wi] = '\0';

			if (wi == 0) { putc('#'); continue; }

			char reference[258];
			reference[0] = '#';
			strncpy(reference + 1, word_buf, sizeof(reference) - 2);
			reference[sizeof(reference) - 1] = '\0';
			const char* replacement = CS16_Localize(reference);
			for (const char* p = replacement; *p && buffer_size > 1; ++p)
				putc(*p);
		}
		else
		{
			putc(*src++);
		}
	}

	*dst = '\0';
	return dst_buffer;
}

// As above, but with a local static buffer
char* CHudTextMessage::BufferedLocaliseTextString(const char* msg)
{
	static char dst_buffer[1024];
	LocaliseTextString(msg, dst_buffer, 1024);
	return dst_buffer;
}

// Simplified version of LocaliseTextString; assumes string is only one word
char* CHudTextMessage::LookupString(char* msg, int* msg_dest)
{
	if (!msg) return (char*)"";

	if (msg[0] == '#')
	{
		client_textmessage_t* clmsg = TextMessageGet(msg + 1);
		if (clmsg && msg_dest && clmsg->effect < 0)
			*msg_dest = -clmsg->effect;

		return (char*)CS16_Localize(msg);
	}

	// обычная строка
	return msg;
}

void StripEndNewlineFromString(char* str)
{
	if (!str || !str[0])
		return;

	int s = strlen(str) - 1;
	if (str[s] == '\n' || str[s] == '\r')
		str[s] = 0;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL(char* str)
{
	for (char* ch = str; *ch != 0; ch++)
		if (*ch == '\r')
			*ch = '\n';
	return str;
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')
int CHudTextMessage::MsgFunc_TextMsg(const char* pszName, int iSize, void* pbuf)
{
	BufferReader reader(pszName, pbuf, iSize);

	int msg_dest = reader.ReadByte();
	int clientIdx = -1;

	static char szBuf[6][MAX_TEXTMSG_STRING];

	// ReadString hands back a shared static buffer, so keep the raw name before
	// the lookup overwrites it.
	static char szRawName[MAX_TEXTMSG_STRING];
	strncpy(szRawName, reader.ReadString(), MAX_TEXTMSG_STRING);
	szRawName[MAX_TEXTMSG_STRING - 1] = 0;

	char* msg_text = LookupString(szRawName, &msg_dest);
	msg_text = strncpy(szBuf[0], msg_text, MAX_TEXTMSG_STRING);
	szBuf[0][MAX_TEXTMSG_STRING - 1] = 0;

	// keep reading strings and using C format strings for substituting the strings into the localised text string
	for (int i = 1; i <= 4; i++)
	{
		char* raw = reader.ReadString();
		const char* str = i == 2 ? raw : CS16_Localize(raw);
		strncpy(szBuf[i], str, MAX_TEXTMSG_STRING);
		szBuf[i][MAX_TEXTMSG_STRING - 1] = 0;

		// these strings are meant for subsitution into the main strings, so cull the automatic end newlines
		StripEndNewlineFromString(szBuf[i]);
	}

	char* psz = szBuf[5];
	const char* arguments[4] = { szBuf[1], szBuf[2], szBuf[3], szBuf[4] };

	switch (msg_dest)
	{
	case HUD_PRINTCENTER:
	{
		CS16_LocalizeFormat(psz, MAX_TEXTMSG_STRING, msg_text, arguments, 4);

		ConvertCRtoNL(psz);
		SetCenterMessage(psz);
		break;
	}
	case HUD_PRINTNOTIFY:
		psz[0] = 1;  // mark this message to go into the notify buffer
		CS16_LocalizeFormat(psz + 1, MAX_TEXTMSG_STRING - 1, msg_text, arguments, 4);
		ConsolePrint(ConvertCRtoNL(psz));
		break;

	case HUD_PRINTTALK:
		psz[0] = 2; // mark, so SayTextPrint will color it
		CS16_LocalizeFormat(psz + 1, MAX_TEXTMSG_STRING - 1, msg_text, arguments, 4);
		gHUD.m_SayText.SayTextPrint(ConvertCRtoNL(psz), 128);
		break;

	case HUD_PRINTCONSOLE:
		CS16_LocalizeFormat(psz, MAX_TEXTMSG_STRING, msg_text, arguments, 4);
		ConsolePrint(ConvertCRtoNL(psz));
		break;

	case HUD_PRINTRADIO:
		psz[0] = 2;
		CS16_LocalizeFormat(psz + 1, MAX_TEXTMSG_STRING - 1, szBuf[1], &arguments[1], 3);

		clientIdx = atoi(szBuf[0]);
		gHUD.m_SayText.SayTextPrint(ConvertCRtoNL(psz), 128, clientIdx);
		break;
	}

	return 1;
}
