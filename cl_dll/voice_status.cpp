#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "entity_types.h"
#include "draw_util.h"
#include "ui/common/hud_style.h"
#include "platform/steam_integration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int cam_thirdperson;

static CVoiceStatus g_VoiceStatus;
static CVoiceStatus *g_InternalVoiceStatus = NULL;

CVoiceStatus *GetClientVoiceMgr()
{
	return &g_VoiceStatus;
}

int __MsgFunc_VoiceMask(const char *, int size, void *buffer)
{
	CS16_StartupTrace("Message VoiceMask: enter");
	if (g_InternalVoiceStatus)
		g_InternalVoiceStatus->HandleVoiceMaskMsg(size, buffer);
	CS16_StartupTrace("Message VoiceMask: complete");
	return 1;
}

int __MsgFunc_ReqState(const char *, int size, void *buffer)
{
	CS16_StartupTrace("Message ReqState: enter");
	if (g_InternalVoiceStatus)
		g_InternalVoiceStatus->HandleReqStateMsg(size, buffer);
	CS16_StartupTrace("Message ReqState: complete");
	return 1;
}

static int g_BannedPlayerPrintCount;

static void PrintBannedPlayer(char id[16])
{
	char text[256];
	_snprintf(text, sizeof(text),
		"Ban %d: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		g_BannedPlayerPrintCount++,
		(unsigned char)id[0], (unsigned char)id[1],
		(unsigned char)id[2], (unsigned char)id[3],
		(unsigned char)id[4], (unsigned char)id[5],
		(unsigned char)id[6], (unsigned char)id[7],
		(unsigned char)id[8], (unsigned char)id[9],
		(unsigned char)id[10], (unsigned char)id[11],
		(unsigned char)id[12], (unsigned char)id[13],
		(unsigned char)id[14], (unsigned char)id[15]);
	text[sizeof(text) - 1] = '\0';
	gEngfuncs.pfnConsolePrint(text);
}

static void ShowBannedPlayers()
{
	if (!g_InternalVoiceStatus)
		return;

	g_BannedPlayerPrintCount = 0;
	gEngfuncs.pfnConsolePrint("------- BANNED PLAYERS -------\n");
	g_InternalVoiceStatus->m_BanMgr.ForEachBannedPlayer(PrintBannedPlayer);
	gEngfuncs.pfnConsolePrint("------------------------------\n");
}

CVoiceStatus::CVoiceStatus()
	: m_pHelper(NULL),
	  m_VoiceHeadModel(0),
	  m_VoiceHeadModelHeight(45.0f),
	  m_LastUpdateServerState(0.0f),
	  m_ServerModEnable(-1),
	  m_InSquelchMode(false),
	  m_Talking(false),
	  m_ServerAcked(false),
	  m_Initialized(false),
	  m_BanMgrInitialized(false),
	  m_GameDir(NULL)
{
	memset(m_VoiceHeadModels, 0, sizeof(m_VoiceHeadModels));
}

CVoiceStatus::~CVoiceStatus()
{
	Shutdown();
	g_InternalVoiceStatus = NULL;
	free(m_GameDir);
	m_GameDir = NULL;
}

int CVoiceStatus::Init(IVoiceStatusHelper *helper)
{
	if (m_Initialized)
		return 1;

	m_pHelper = helper;
	g_InternalVoiceStatus = this;
	m_InSquelchMode = false;
	m_iFlags = HUD_ACTIVE;

	gEngfuncs.pfnRegisterVariable("voice_modenable", "1", FCVAR_ARCHIVE);
	gEngfuncs.pfnRegisterVariable("voice_clientdebug", "0", 0);
	gEngfuncs.pfnAddCommand("voice_showbanned", ShowBannedPlayers);
	HOOK_MESSAGE(VoiceMask);
	HOOK_MESSAGE(ReqState);

	const char *gameDir = gEngfuncs.pfnGetGameDirectory();
	if (gameDir && gameDir[0])
	{
		m_BanMgrInitialized = m_BanMgr.Init(gameDir);
		m_GameDir = (char *)malloc(strlen(gameDir) + 1);
		if (m_GameDir)
			strcpy(m_GameDir, gameDir);
	}

	gHUD.AddHudElem(this);
	m_Initialized = true;
	return 1;
}

int CVoiceStatus::VidInit()
{
	m_VoicePlayers.Init();
	m_Talking = false;
	m_ServerAcked = false;
	m_VoiceHeadModelHeight = 45.0f;
	char *file = (char *)gEngfuncs.COM_LoadFile("scripts/voicemodel.txt", 5, NULL);
	if (file)
	{
		char token[4096];
		gEngfuncs.COM_ParseFile(file, token);
		if (token[0] >= '0' && token[0] <= '9')
			m_VoiceHeadModelHeight = (float)atof(token);
		gEngfuncs.COM_FreeFile(file);
	}

	m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/voiceicon.spr");
	return 1;
}

int CVoiceStatus::Draw(float)
{
	if (!m_pHelper || !m_pHelper->CanShowSpeakerLabels())
		return 1;

	if (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL)
		return 1;

	const bool modern = CS16_HudStyleModern(CS16_HUD_VOICE);
	const int iconWidth = m_VoiceHeadModel ? SPR_Width(m_VoiceHeadModel) : 0;
	const int iconHeight = m_VoiceHeadModel ? SPR_Height(m_VoiceHeadModel) : 0;
	const int rowHeight = max(max(iconHeight, gHUD.m_iFontHeight), 16) + 4;
	const int iconGap = iconWidth ? 4 : 0;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	const int localIndex = localPlayer ? localPlayer->index : 0;
	int y = ScreenHeight / 2;

	for (int clientIndex = 0; clientIndex < VOICE_MAX_PLAYERS; ++clientIndex)
	{
		if (!m_VoicePlayers[clientIndex])
			continue;

		const int playerIndex = clientIndex + 1;
		if (playerIndex == localIndex || playerIndex > gEngfuncs.GetMaxClients())
			continue;

		hud_player_info_t info;
		memset(&info, 0, sizeof(info));
		gEngfuncs.pfnGetPlayerInfo(playerIndex, &info);
		if (!info.name || !info.name[0])
			continue;

		int teamColor[3] = { 255, 255, 255 };
		m_pHelper->GetPlayerTextColor(playerIndex, teamColor);

		const int textWidth = DrawUtils::HudStringLen(info.name);
		int iconX;
		int rowWidth;
		int x;

		if (modern)
		{
			const int avatarSize = max(12, rowHeight - 4);
			const int avatarGap = 4;
			rowWidth = 7 + avatarSize + avatarGap + iconWidth + iconGap +
				textWidth + 6;
			x = max(0, ScreenWidth - rowWidth - 8);

			FillRGBABlend(x, y, rowWidth, rowHeight, 0, 0, 0, 150);
			FillRGBABlend(x, y, 3, rowHeight,
				teamColor[0], teamColor[1], teamColor[2], 230);
			const int avatarX = x + 5;
			const int avatarY = y + (rowHeight - avatarSize) / 2;
			FillRGBABlend(avatarX, avatarY, avatarSize, avatarSize,
				teamColor[0], teamColor[1], teamColor[2], 90);
			CS16Steam_QueueAvatar(playerIndex, info.m_nSteamID, avatarX, avatarY,
				avatarSize, 255, gHUD.m_flTime);
			iconX = avatarX + avatarSize + avatarGap;
		}
		else
		{
			// The stock label is a plain bar filled with the speaker's team
			// colour, with the icon and the white name sitting on top of it.
			const int padding = 4;
			rowWidth = padding * 2 + iconWidth + iconGap + textWidth;
			x = max(0, ScreenWidth - rowWidth - 8);

			FillRGBABlend(x, y, rowWidth, rowHeight,
				teamColor[0], teamColor[1], teamColor[2], 160);
			iconX = x + padding;
		}

		if (m_VoiceHeadModel)
		{
			SPR_Set(m_VoiceHeadModel, 255, 255, 255);
			SPR_DrawAdditive(0, iconX, y + (rowHeight - iconHeight) / 2, NULL);
		}

		DrawUtils::DrawHudString(iconX + iconWidth + iconGap,
			y + (rowHeight - gHUD.m_iFontHeight) / 2,
			ScreenWidth - 8, info.name, 255, 255, 255);
		y += rowHeight + 2;
	}

	// Match the original voice acknowledgement: while the local microphone is
	// active, keep a small icon above the lower-right HUD counters.
	if (m_Talking && m_VoiceHeadModel)
	{
		const int x = ScreenWidth - iconWidth - 10;
		const int y = ScreenHeight - m_pHelper->GetAckIconHeight() - iconHeight;
		SPR_Set(m_VoiceHeadModel, 255, 255, 255);
		SPR_DrawAdditive(0, x, y, NULL);
	}

	return 1;
}

void CVoiceStatus::Shutdown()
{
	if (m_BanMgrInitialized && m_GameDir)
	{
		m_BanMgr.SaveState(m_GameDir);
		m_BanMgrInitialized = false;
	}
}

void CVoiceStatus::Frame(double)
{
	if (gEngfuncs.GetClientTime() - m_LastUpdateServerState > 1.0f)
	{
		static bool tracedFirstServerUpdate = false;
		const char *levelName = gEngfuncs.pfnGetLevelName();
		const bool traceThisUpdate = !tracedFirstServerUpdate && levelName && levelName[0];
		if (traceThisUpdate)
			CS16_StartupTrace("Voice UpdateServerState: first enter");
		UpdateServerState(false);
		if (traceThisUpdate)
		{
			CS16_StartupTrace("Voice UpdateServerState: first complete");
			tracedFirstServerUpdate = true;
		}
	}
}

void CVoiceStatus::CreateEntities()
{
	if (!m_VoiceHeadModel)
		return;

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	if (!localPlayer)
		return;

	int outputIndex = 0;
	for (int i = 0; i < VOICE_MAX_PLAYERS && outputIndex < VOICE_MAX_PLAYERS; ++i)
	{
		if (!m_VoicePlayers[i])
			continue;

		cl_entity_s *player = gEngfuncs.GetEntityByIndex(i + 1);
		if (!player || player->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;
		if (player->curstate.effects & EF_NODRAW)
			continue;
		if (player == localPlayer && !cam_thirdperson)
			continue;

		cl_entity_s *icon = &m_VoiceHeadModels[outputIndex++];
		memset(icon, 0, sizeof(*icon));
		icon->curstate.rendermode = kRenderTransAdd;
		icon->curstate.renderamt = 255;
		icon->baseline.renderamt = 255;
		icon->curstate.renderfx = kRenderFxNoDissipation;
		icon->curstate.framerate = 1.0f;
		icon->model = (model_s *)gEngfuncs.GetSpritePointer(m_VoiceHeadModel);
		icon->curstate.scale = 0.5f;
		icon->origin[0] = player->origin[0];
		icon->origin[1] = player->origin[1];
		icon->origin[2] = player->origin[2] + m_VoiceHeadModelHeight;
		gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, icon);
	}
}

void CVoiceStatus::UpdateSpeakerStatus(int entindex, qboolean talking)
{
	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		char text[160];
		_snprintf(text, sizeof(text), "voice: entity %d talking=%d\n", entindex, !!talking);
		text[sizeof(text) - 1] = '\0';
		gEngfuncs.pfnConsolePrint(text);
	}

	if (entindex == -1)
	{
		m_Talking = !!talking;
		if (talking)
			gEngfuncs.pfnClientCmd("voice_modenable 1");

		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
		if (!localPlayer)
			return;
		entindex = localPlayer->index;
	}
	else if (entindex == -2)
	{
		m_ServerAcked = !!talking;
		return;
	}

	if (entindex < 1 || entindex > VOICE_MAX_PLAYERS)
		return;

	const int clientIndex = entindex - 1;
	m_VoicePlayers[clientIndex] = !!talking;
	if (talking)
		m_VoiceEnabledPlayers[clientIndex] = true;
}

void CVoiceStatus::UpdateServerState(bool force)
{
	const char *levelName = gEngfuncs.pfnGetLevelName();
	if (!levelName || !levelName[0])
		return;

	const int modEnabled = !!gEngfuncs.pfnGetCvarFloat("voice_modenable");
	if (force || m_ServerModEnable != modEnabled)
	{
		m_ServerModEnable = modEnabled;
		char command[64];
		_snprintf(command, sizeof(command), "VModEnable %d", m_ServerModEnable);
		command[sizeof(command) - 1] = '\0';
		ServerCmd(command);
	}

	if (!gEngfuncs.GetPlayerUniqueID || !gEngfuncs.pfnServerCmdUnreliable)
		return;

	char command[128] = "vban";
	bool changed = false;
	for (int word = 0; word < VOICE_MAX_PLAYERS_DW; ++word)
	{
		uint32 serverMask = 0;
		uint32 desiredMask = 0;
		for (int bit = 0; bit < 32; ++bit)
		{
			const int playerIndex = word * 32 + bit + 1;
			if (playerIndex > VOICE_MAX_PLAYERS)
				break;

			char playerId[16];
			if (gEngfuncs.GetPlayerUniqueID(playerIndex, playerId) && m_BanMgr.GetPlayerBan(playerId))
				desiredMask |= (uint32)1 << bit;
			if (m_ServerBannedPlayers[word * 32 + bit])
				serverMask |= (uint32)1 << bit;
		}

		changed = changed || serverMask != desiredMask;
		char number[16];
		_snprintf(number, sizeof(number), " %x", desiredMask);
		number[sizeof(number) - 1] = '\0';
		strncat(command, number, sizeof(command) - strlen(command) - 1);
	}

	if (force || changed)
		gEngfuncs.pfnServerCmdUnreliable(command);

	m_LastUpdateServerState = gEngfuncs.GetClientTime();
}

void CVoiceStatus::HandleVoiceMaskMsg(int size, void *buffer)
{
	BufferReader reader(buffer, size);
	for (int word = 0; word < VOICE_MAX_PLAYERS_DW; ++word)
	{
		m_AudiblePlayers.SetDWord(word, (uint32)reader.ReadLong());
		m_ServerBannedPlayers.SetDWord(word, (uint32)reader.ReadLong());
	}
	m_ServerModEnable = reader.ReadByte();
}

void CVoiceStatus::HandleReqStateMsg(int, void *)
{
	UpdateServerState(true);
}

void CVoiceStatus::StartSquelchMode()
{
	if (m_InSquelchMode)
		return;
	m_InSquelchMode = true;
	if (m_pHelper)
		m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_InSquelchMode = false;
	if (m_pHelper)
		m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode() const
{
	return m_InSquelchMode;
}

bool CVoiceStatus::IsPlayerBlocked(int playerIndex)
{
	if (playerIndex < 1 || playerIndex > VOICE_MAX_PLAYERS || !gEngfuncs.GetPlayerUniqueID)
		return false;
	char playerId[16];
	return gEngfuncs.GetPlayerUniqueID(playerIndex, playerId)
		&& m_BanMgr.GetPlayerBan(playerId);
}

bool CVoiceStatus::IsPlayerAudible(int playerIndex)
{
	return playerIndex >= 1 && playerIndex <= VOICE_MAX_PLAYERS
		&& !!m_AudiblePlayers[playerIndex - 1];
}

void CVoiceStatus::SetPlayerBlockedState(int playerIndex, bool blocked)
{
	if (playerIndex < 1 || playerIndex > VOICE_MAX_PLAYERS || !gEngfuncs.GetPlayerUniqueID)
		return;

	char playerId[16];
	if (!gEngfuncs.GetPlayerUniqueID(playerIndex, playerId))
		return;
	m_BanMgr.SetPlayerBan(playerId, blocked);
	UpdateServerState(false);
}
