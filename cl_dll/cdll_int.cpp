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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//
#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "pmtrace.h"

#include "pm_shared.h"

#include <string.h>
#include "interface.h" // not used here
#include "Exports.h"

cl_enginefunc_t		gEngfuncs = { };
CHud gHUD;

void InitInput(void);
void Game_HookEvents(void);
void IN_Commands(void);
void Input_Shutdown(void);

/*
==========================
	Initialize

Called when the DLL is first loaded.
==========================
*/
int CL_DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	gEngfuncs = *pEnginefuncs;

	Game_HookEvents();
	return 1;
}


/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int CL_DLLEXPORT HUD_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	int iret = 0;

	switch (hullnumber)
	{
	case 0:				// Normal player
		Vector(-16, -16, -36).CopyToArray(mins);
		Vector(16, 16, 36).CopyToArray(maxs);
		iret = 1;
		break;
	case 1:				// Crouched player
		Vector(-16, -16, -18).CopyToArray(mins);
		Vector(16, 16, 18).CopyToArray(maxs);
		iret = 1;
		break;
	case 2:				// Point based hull
		Vector(0, 0, 0).CopyToArray(mins);
		Vector(0, 0, 0).CopyToArray(maxs);
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	CL_DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	// Parse stuff from args
	// int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void CL_DLLEXPORT HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	PM_Init(ppmove);
}

char CL_DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	return PM_FindTextureType(name);
}

void CL_DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	PM_Move(ppmove, server);
}

#ifdef _CS16CLIENT_ENABLE_GSRC_SUPPORT
/*
=================
HUD_GetRect

VGui stub
=================
*/
int* HUD_GetRect(void)
{
	static int extent[4];

	extent[0] = gEngfuncs.GetWindowCenterX() - ScreenWidth / 2;
	extent[1] = gEngfuncs.GetWindowCenterY() - ScreenHeight / 2;
	extent[2] = gEngfuncs.GetWindowCenterX() + ScreenWidth / 2;
	extent[3] = gEngfuncs.GetWindowCenterY() + ScreenHeight / 2;

	return extent;
}
#endif

/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

int CL_DLLEXPORT HUD_VidInit(void)
{
	gHUD.VidInit();

	//VGui_Startup();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all
the hud variables.
==========================
*/
void CL_DLLEXPORT HUD_Init(void)
{
	gHUD.Init();
	InitInput();
	//Scheme_Init();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int CL_DLLEXPORT HUD_Redraw(float time, int intermission)
{
	gHUD.Redraw(time, intermission);

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int CL_DLLEXPORT HUD_UpdateClientData(client_data_t* pcldata, float flTime)
{
	IN_Commands();

	return gHUD.UpdateClientData(pcldata, flTime);
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void CL_DLLEXPORT HUD_Reset(void)
{
	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void CL_DLLEXPORT HUD_Frame(double time)
{
#ifdef _CS16CLIENT_ENABLE_GSRC_SUPPORT
	gEngfuncs.VGui_ViewportPaintBackground(HUD_GetRect());
#endif

	GetClientVoice()->Frame(time);
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void CL_DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	// gHUD.m_Radio.Voice( entindex, bTalking );

	if (entindex >= 0 && entindex < gEngfuncs.GetMaxClients())
	{
		if (bTalking)
		{
			g_PlayerExtraInfo[entindex].radarflashtime = gHUD.m_flTime;
			g_PlayerExtraInfo[entindex].radarflashes = 99999;
		}
		else
		{
			g_PlayerExtraInfo[entindex].radarflashtime = -1.0f;
			g_PlayerExtraInfo[entindex].radarflashes = 0;
		}
	}

	GetClientVoice()->UpdateSpeakerStatus(entindex, bTalking);
}

/*
==========================
HUD_DirectorEvent

Called when a director event message was received
==========================
*/

void CL_DLLEXPORT HUD_DirectorMessage(int iSize, void* pbuf)
{
	gHUD.m_Spectator.DirectorMessage(iSize, pbuf);
}

extern "C" void CL_DLLEXPORT HUD_ChatInputPosition(int* x, int* y)
{
}

extern "C" int CL_DLLEXPORT HUD_GetPlayerTeam(int iplayer)
{
	if (iplayer <= MAX_PLAYERS)
		return g_PlayerExtraInfo[iplayer].teamnumber;
	return 0;
}

#include "APIProxy.h"

cldll_func_dst_t* g_pcldstAddrs;

extern "C" void CL_DLLEXPORT F(void* pv)
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;

	// Hack!
	g_pcldstAddrs = ((cldll_func_dst_t*)pcldll_func->pHudVidInitFunc);

	cldll_func_t cldll_func =
	{
	Initialize,
	HUD_Init,
	HUD_VidInit,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_Reset,
	HUD_PlayerMove,
	HUD_PlayerMoveInit,
	HUD_PlayerMoveTexture,
	IN_ActivateMouse,
	IN_DeactivateMouse,
	IN_MouseEvent,
	IN_ClearStates,
	IN_Accumulate,
	CL_CreateMove,
	CL_IsThirdPerson,
	CL_CameraOffset,
	KB_Find,
	CAM_Think,
	V_CalcRefdef,
	HUD_AddEntity,
	HUD_CreateEntities,
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
	HUD_StudioEvent,
	HUD_PostRunCmd,
	HUD_Shutdown,
	HUD_TxferLocalOverrides,
	HUD_ProcessPlayerState,
	HUD_TxferPredictionData,
	Demo_ReadBuffer,
	HUD_ConnectionlessPacket,
	HUD_GetHullBounds,
	HUD_Frame,
	HUD_Key_Event,
	HUD_TempEntUpdate,
	HUD_GetUserEntity,
	HUD_VoiceStatus,
	HUD_DirectorMessage,
	HUD_GetStudioModelInterface,
	HUD_ChatInputPosition,
	};

	*pcldll_func = cldll_func;
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char* GetServerHostName()
	{
		return gHUD.m_szServerName;
	}

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		if (GetClientVoice())
			return GetClientVoice()->IsPlayerBlocked(playerIndex);

		return false;
	}

	virtual void MutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoice())
		{
			GetClientVoice()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	virtual void UnmutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoice())
		{
			GetClientVoice()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION)

