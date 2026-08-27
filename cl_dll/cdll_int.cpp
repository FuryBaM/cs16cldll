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
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(_WIN32) && defined(_CS16CLIENT_STARTUP_TRACE)
#include <stdio.h>
#endif
#include "interface.h" // not used here
#include "Exports.h"
#include "cs_vgui.h"
#include "cs_vgui2.h"
#include "platform/steam_integration.h"
#include "platform/discord_rpc.h"

cl_enginefunc_t		gEngfuncs = { };
CHud gHUD;

#include "particleman.h"
CSysModule* g_hParticleManModule = NULL;
IParticleMan* g_pParticleMan = NULL;

#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
namespace
{
PVOID g_cs16EngineCompatHandler = NULL;

LONG CALLBACK CS16_EngineCompatException(EXCEPTION_POINTERS* exceptionInfo)
{
	if (!exceptionInfo || !exceptionInfo->ExceptionRecord ||
		!exceptionInfo->ContextRecord ||
		exceptionInfo->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
		exceptionInfo->ExceptionRecord->NumberParameters < 2 ||
		exceptionInfo->ExceptionRecord->ExceptionInformation[0] != 0 ||
		exceptionInfo->ExceptionRecord->ExceptionInformation[1] != 0x17C)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	BYTE* engine = reinterpret_cast<BYTE*>(GetModuleHandleA("hw.dll"));
	if (!engine || exceptionInfo->ContextRecord->Eip !=
		reinterpret_cast<DWORD_PTR>(engine + 0x247D03) ||
		exceptionInfo->ContextRecord->Eax != 0)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(engine);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return EXCEPTION_CONTINUE_SEARCH;
	const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(engine + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE ||
		nt->FileHeader.TimeDateStamp != 0x670493DA)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Steam GoldSrc build 10210 reads cl.worldmodel->visdata here without
	// checking cl.worldmodel after an essential remote resource fails.  Verify
	// the exact instruction before resuming in the function's existing
	// no-visdata branch; unknown engine builds keep their normal exception path.
	static const BYTE expected[] =
		{ 0x83, 0xB8, 0x7C, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84 };
	if (memcmp(engine + 0x247D03, expected, sizeof(expected)) != 0)
		return EXCEPTION_CONTINUE_SEARCH;

	exceptionInfo->ContextRecord->Eip =
		reinterpret_cast<DWORD_PTR>(engine + 0x247DDF);
	return EXCEPTION_CONTINUE_EXECUTION;
}

void CS16_InstallEngineCompatHandler()
{
	if (!g_cs16EngineCompatHandler)
		g_cs16EngineCompatHandler = AddVectoredExceptionHandler(1, CS16_EngineCompatException);
}
}

void CS16_RemoveEngineCompatHandler()
{
	if (g_cs16EngineCompatHandler &&
		RemoveVectoredExceptionHandler(g_cs16EngineCompatHandler))
	{
		g_cs16EngineCompatHandler = NULL;
	}
}
#else
void CS16_InstallEngineCompatHandler() {}
void CS16_RemoveEngineCompatHandler() {}
#endif

#if defined(_WIN32) && defined(_CS16CLIENT_STARTUP_TRACE)
static bool g_cs16RuntimeTrace = false;
static PVOID g_cs16ExceptionHandler = NULL;
static LONG g_cs16TraceSequence = 0;

void CS16_StartupTrace(const char* stage, bool reset)
{
	if (!stage)
		return;

	if (reset)
		InterlockedExchange(&g_cs16TraceSequence, 0);

	char path[MAX_PATH];
	const char filename[] = "cs16_goldsrc_startup.log";
	const DWORD length = GetTempPathA(sizeof(path), path);
	if (!length || length + sizeof(filename) > sizeof(path))
		return;

	memcpy(path + length, filename, sizeof(filename));
	HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, reset ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;

	SetFilePointer(file, 0, NULL, FILE_END);
	char line[768];
	const LONG sequence = InterlockedIncrement(&g_cs16TraceSequence);
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[%06ld ms=%lu tid=%lu] %s",
		(long)sequence, (unsigned long)GetTickCount(),
		(unsigned long)GetCurrentThreadId(), stage);

	DWORD written = 0;
	WriteFile(file, line, (DWORD)strlen(line), &written, NULL);
	WriteFile(file, "\r\n", 2, &written, NULL);
	CloseHandle(file);
}

void CS16_SetRuntimeTrace(bool enabled)
{
	g_cs16RuntimeTrace = enabled;
}

bool CS16_RuntimeTraceEnabled(void)
{
	return g_cs16RuntimeTrace;
}

static LONG CALLBACK CS16_ExceptionTrace(EXCEPTION_POINTERS* exceptionInfo)
{
	// OutputDebugString raises these informational exceptions when a debugger
	// is absent. They are handled by Windows and are not game crashes.
	if (exceptionInfo && exceptionInfo->ExceptionRecord &&
		(exceptionInfo->ExceptionRecord->ExceptionCode == 0x40010006 ||
		 exceptionInfo->ExceptionRecord->ExceptionCode == 0x4001000A))
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	static LONG handlingException = 0;
	if (!exceptionInfo || !exceptionInfo->ExceptionRecord || !exceptionInfo->ContextRecord ||
		InterlockedExchange(&handlingException, 1) != 0)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	const ULONG_PTR address = (ULONG_PTR)exceptionInfo->ExceptionRecord->ExceptionAddress;
	MEMORY_BASIC_INFORMATION memoryInfo = {};
	VirtualQuery((LPCVOID)address, &memoryInfo, sizeof(memoryInfo));
	const ULONG_PTR moduleBase = (ULONG_PTR)memoryInfo.AllocationBase;

	char modulePath[MAX_PATH] = "unknown";
	if (moduleBase)
		GetModuleFileNameA((HMODULE)moduleBase, modulePath, sizeof(modulePath));

	char stage[512];
	snprintf(stage, sizeof(stage),
		"EXCEPTION code=0x%08lX address=0x%08lX module_base=0x%08lX rva=0x%08lX module=%s",
		(unsigned long)exceptionInfo->ExceptionRecord->ExceptionCode,
		(unsigned long)address,
		(unsigned long)moduleBase,
		(unsigned long)(moduleBase ? address - moduleBase : 0),
		modulePath);
	CS16_StartupTrace(stage);

#if defined(_M_IX86) || defined(__i386__)
	snprintf(stage, sizeof(stage),
		"CONTEXT eip=0x%08lX esp=0x%08lX ebp=0x%08lX eax=0x%08lX ebx=0x%08lX ecx=0x%08lX edx=0x%08lX",
		(unsigned long)exceptionInfo->ContextRecord->Eip,
		(unsigned long)exceptionInfo->ContextRecord->Esp,
		(unsigned long)exceptionInfo->ContextRecord->Ebp,
		(unsigned long)exceptionInfo->ContextRecord->Eax,
		(unsigned long)exceptionInfo->ContextRecord->Ebx,
		(unsigned long)exceptionInfo->ContextRecord->Ecx,
		(unsigned long)exceptionInfo->ContextRecord->Edx);
	CS16_StartupTrace(stage);
#endif

	InterlockedExchange(&handlingException, 0);
	return EXCEPTION_CONTINUE_SEARCH;
}

static void CS16_InstallExceptionTrace(void)
{
	if (!g_cs16ExceptionHandler)
		g_cs16ExceptionHandler = AddVectoredExceptionHandler(1, CS16_ExceptionTrace);
}

void CS16_RemoveExceptionTrace(void)
{
	if (g_cs16ExceptionHandler && RemoveVectoredExceptionHandler(g_cs16ExceptionHandler))
		g_cs16ExceptionHandler = NULL;
}
#else
void CS16_StartupTrace(const char*, bool) {}
void CS16_SetRuntimeTrace(bool) {}
bool CS16_RuntimeTraceEnabled(void) { return false; }
void CS16_RemoveExceptionTrace(void) {}
#endif

void InitInput(void);
void Game_HookEvents(void);
void IN_Commands(void);
void Input_Shutdown(void);
void CL_LoadParticleMan(void);

/*
==========================
	Initialize

Called when the DLL is first loaded.
==========================
*/
int CL_DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	CS16_StartupTrace("Initialize: enter", true);
#if defined(_WIN32) && defined(_CS16CLIENT_STARTUP_TRACE)
	CS16_InstallExceptionTrace();
#endif
	if (iVersion != CLDLL_INTERFACE_VERSION)
	{
		CS16_StartupTrace("Initialize: incompatible interface version");
		return 0;
	}

	gEngfuncs = *pEnginefuncs;
	CS16_StartupTrace("Initialize: engine table copied");
	CS16_InstallEngineCompatHandler();

	Game_HookEvents();
	CS16_StartupTrace("Initialize: events hooked");
	CL_LoadParticleMan();
	CS16VGUI2_Startup();
	CS16_StartupTrace("Initialize: complete");
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
	CS16_StartupTrace("HUD_PlayerMoveInit: enter");
	PM_Init(ppmove);
	CS16_StartupTrace("HUD_PlayerMoveInit: complete");
}

char CL_DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	return PM_FindTextureType(name);
}

void CL_DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	static bool tracedFirstMove = false;
	const bool traceThisCall = !tracedFirstMove || CS16_RuntimeTraceEnabled();
	if (traceThisCall)
		CS16_StartupTrace("HUD_PlayerMove: enter");
	PM_Move(ppmove, server);
	if (traceThisCall)
	{
		CS16_StartupTrace("HUD_PlayerMove: complete");
		tracedFirstMove = true;
	}
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
	CS16_StartupTrace("HUD_VidInit: enter");
	gHUD.VidInit();
	CS16VGUI_Startup(ScreenWidth, ScreenHeight);
	CS16_StartupTrace("HUD_VidInit: complete");

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
	CS16_StartupTrace("HUD_Init: enter");
	// Match Valve's GoldSrc order: input cvars and commands must exist before
	// individual HUD elements initialize.
	InitInput();
	CS16_StartupTrace("HUD_Init: input initialized");
	gHUD.Init();
	CS16Steam_Init();
	CS16Discord_Init();
	CS16VGUI2_RegisterCommands();
	CS16VGUI_ResetSession();
	gEngfuncs.Cvar_SetValue("_vgui_menus", CS16VGUI_IsAvailable() ? 1.0f : 0.0f);
	CS16_StartupTrace("HUD_Init: complete");
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
	static bool tracedFirstLevelRedraw = false;
	const char* levelName = gEngfuncs.pfnGetLevelName();
	const bool traceThisCall = ((!tracedFirstLevelRedraw && levelName && levelName[0]) ||
		CS16_RuntimeTraceEnabled());
	if (traceThisCall)
		CS16_StartupTrace("HUD_Redraw: enter");
	gHUD.Redraw(time, intermission);
	if (traceThisCall)
	{
		CS16_StartupTrace("HUD_Redraw: complete");
		tracedFirstLevelRedraw = true;
	}

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
	static bool tracedFirstLevelClientData = false;
	const char* levelName = gEngfuncs.pfnGetLevelName();
	const bool traceThisCall = ((!tracedFirstLevelClientData && levelName && levelName[0]) ||
		CS16_RuntimeTraceEnabled());
	if (traceThisCall)
		CS16_StartupTrace("HUD_UpdateClientData: enter");
	IN_Commands();

	const int result = gHUD.UpdateClientData(pcldata, flTime);
	if (traceThisCall)
	{
		CS16_StartupTrace("HUD_UpdateClientData: complete");
		tracedFirstLevelClientData = true;
	}
	return result;
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
	if (CS16_ConsumeMenuEscapeHookRequest())
		CS16_CloseTopmostMenu();

	const int pendingSelectionMenu = CS16_ConsumePendingSelectionMenuRequest();
	if (pendingSelectionMenu)
		CS16VGUI_ShowMenu(pendingSelectionMenu);

	static bool tracedFirstFrame = false;
	if (!tracedFirstFrame)
	{
		CS16_StartupTrace("HUD_Frame: first frame");
		tracedFirstFrame = true;
	}
	else if (CS16_RuntimeTraceEnabled())
	{
		CS16_StartupTrace("HUD_Frame: enter");
	}

#ifdef _CS16CLIENT_ENABLE_GSRC_SUPPORT
	// Advertise VGUI menus only after a native GoldSrc viewport has attached
	// successfully. Unsupported menu IDs disable it for the current session and
	// automatically restore the ShowMenu protocol.
	const float wantedVguiMenus = CS16VGUI_IsAvailable() ? 1.0f : 0.0f;
	if (CVAR_GET_FLOAT("_vgui_menus") != wantedVguiMenus)
	{
		if (wantedVguiMenus == 0.0f)
			CS16VGUI_HideMenu();
		gEngfuncs.Cvar_SetValue("_vgui_menus", wantedVguiMenus);
	}

	// The custom viewport paints its own menu panels. Asking GoldSrc to paint
	// the legacy viewport background every frame covers the 3D scene with an
	// opaque black layer even after the menu has been dismissed.
#endif

	GetClientVoiceMgr()->Frame(time);
	CS16Steam_Frame(time);
	CS16Discord_Frame(time);
	if (tracedFirstFrame && CS16_RuntimeTraceEnabled())
		CS16_StartupTrace("HUD_Frame: complete");
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

	if (entindex > 0 && entindex <= gEngfuncs.GetMaxClients())
	{
		g_PlayerExtraInfo[entindex].talking = bTalking != 0;
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

	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, bTalking);
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

void CL_UnloadParticleMan(void)
{
	if (g_hParticleManModule)
		Sys_UnloadModule(g_hParticleManModule);

	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}

void CL_LoadParticleMan(void)
{
#if !defined(_MSC_VER)
	// particleman.dll exposes an MSVC C++ vtable. MinGW can safely use the
	// engine's C function tables, but it cannot call this cross-module ABI.
	CS16_StartupTrace("Initialize: particleman skipped (non-MSVC ABI)");
	return;
#else
	char szPDir[512];

	if (gEngfuncs.COM_ExpandFilename(PARTICLEMAN_DLLNAME, szPDir, sizeof(szPDir)) == FALSE)
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_hParticleManModule = Sys_LoadModule(szPDir);
	if (!g_hParticleManModule)
	{
		g_pParticleMan = NULL;
		return;
	}

	CreateInterfaceFn particleManFactory = Sys_GetFactory(g_hParticleManModule);

	if (particleManFactory == NULL)
	{
		Sys_UnloadModule(g_hParticleManModule);
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_pParticleMan = (IParticleMan*)particleManFactory(PARTICLEMAN_INTERFACE, NULL);

	if (g_pParticleMan)
	{
		g_pParticleMan->SetUp(&gEngfuncs);

		// Add custom particle classes here BEFORE calling anything else or you will die.
		g_pParticleMan->AddCustomParticleClassSize(sizeof(CBaseParticle));
	}
	else
	{
		Sys_UnloadModule(g_hParticleManModule);
		g_hParticleManModule = NULL;
	}
#endif
}

cldll_func_dst_t* g_pcldstAddrs;

extern "C" void CL_DLLEXPORT HUD_ChatInputPosition(int* x, int* y)
{
}

extern "C" int CL_DLLEXPORT HUD_GetPlayerTeam(int iplayer)
{
	if (iplayer > 0 && iplayer <= MAX_PLAYERS)
		return g_PlayerExtraInfo[iplayer].teamnumber;
	return 0;
}

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
	HUD_GetPlayerTeam,
	(CLIENTFACTORY)Sys_GetFactoryThis()
	};

	*pcldll_func = cldll_func;
}

#if defined(_MSC_VER)
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
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);

		return false;
	}

	virtual void MutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	virtual void UnmutePlayerGameVoice(int playerIndex)
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION)
#endif
