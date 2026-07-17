#include "hud.h"
#include "cl_util.h"
#include "interface.h"
#include "cs_vgui2.h"

#include <string.h>

namespace
{
struct VGui2Interface
{
    const char* versionName;
    void* instance;
};

// These are the exact interface versions exposed by Steam GoldSrc VGUI2.
VGui2Interface g_interfaces[] =
{
    { "VGUI_ivgui006", NULL },
    { "VGUI_Panel007", NULL },
    { "VGUI_Surface026", NULL },
    { "VGUI_Input004", NULL },
    { "VGUI_Scheme009", NULL },
    { "VGUI_System009", NULL },
    { "VGUI_Localize003", NULL }
};

CSysModule* g_vgui2Module = NULL;
CreateInterfaceFn g_vgui2Factory = NULL;
bool g_vgui2Attempted = false;
bool g_vgui2Available = false;
bool g_vgui2CommandsRegistered = false;

void CS16VGUI2_ClearInterfaces(void)
{
    for (int i = 0; i < ARRAYSIZE(g_interfaces); ++i)
        g_interfaces[i].instance = NULL;

    g_vgui2Factory = NULL;
    g_vgui2Available = false;
}

void CS16VGUI2_Status_f(void)
{
    if (!g_vgui2Attempted)
        CS16VGUI2_Startup();

    if (!gEngfuncs.Con_Printf)
        return;

    gEngfuncs.Con_Printf("[CS16 VGUI2] module=%s factory=%s status=%s viewport=%s\n",
        g_vgui2Module ? "loaded" : "missing",
        g_vgui2Factory ? "found" : "missing",
        g_vgui2Available ? "ready" : "unavailable",
        CS16VGUI2_IsViewportReady() ? "ready" : "not-initialized");

    for (int i = 0; i < ARRAYSIZE(g_interfaces); ++i)
    {
        gEngfuncs.Con_Printf("[CS16 VGUI2] %-20s %s\n",
            g_interfaces[i].versionName,
            g_interfaces[i].instance ? "OK" : "MISSING");
    }
}
}

extern "C" int CS16VGUI2_Startup(void)
{
    if (g_vgui2Attempted)
        return g_vgui2Available ? 1 : 0;

    g_vgui2Attempted = true;
    CS16VGUI2_ClearInterfaces();

#if defined(_WIN32)
    // Dynamic loading keeps client.dll usable on engines that do not ship
    // VGUI2. Steam GoldSrc loads this module from the Half-Life directory.
    g_vgui2Module = Sys_LoadModule("vgui2.dll");
#else
    g_vgui2Module = Sys_LoadModule("vgui2.so");
#endif
    if (!g_vgui2Module)
        return 0;

    g_vgui2Factory = Sys_GetFactory(g_vgui2Module);
    if (!g_vgui2Factory)
    {
        Sys_UnloadModule(g_vgui2Module);
        g_vgui2Module = NULL;
        return 0;
    }

    g_vgui2Available = true;
    for (int i = 0; i < ARRAYSIZE(g_interfaces); ++i)
    {
        int returnCode = IFACE_FAILED;
        g_interfaces[i].instance = g_vgui2Factory(
            g_interfaces[i].versionName, &returnCode);
        if (!g_interfaces[i].instance || returnCode != IFACE_OK)
            g_vgui2Available = false;
    }

    if (gEngfuncs.Con_Printf)
    {
        gEngfuncs.Con_Printf("[CS16 VGUI2] interface bootstrap %s. Run cs_vgui2_status for details.\n",
            g_vgui2Available ? "ready" : "incomplete");
    }

    return g_vgui2Available ? 1 : 0;
}

extern "C" void CS16VGUI2_Shutdown(void)
{
    CS16VGUI2_ClearInterfaces();

    if (g_vgui2Module)
    {
        Sys_UnloadModule(g_vgui2Module);
        g_vgui2Module = NULL;
    }

    g_vgui2Attempted = false;
}

extern "C" void CS16VGUI2_RegisterCommands(void)
{
    if (g_vgui2CommandsRegistered || !gEngfuncs.pfnAddCommand)
        return;

    gEngfuncs.pfnAddCommand("cs_vgui2_status", CS16VGUI2_Status_f);
    g_vgui2CommandsRegistered = true;
}

extern "C" int CS16VGUI2_IsAvailable(void)
{
    return g_vgui2Available ? 1 : 0;
}

extern "C" void* CS16VGUI2_GetInterface(const char* versionName)
{
    if (!versionName || !versionName[0])
        return NULL;

    if (!g_vgui2Attempted)
        CS16VGUI2_Startup();

    for (int i = 0; i < ARRAYSIZE(g_interfaces); ++i)
    {
        if (!strcmp(versionName, g_interfaces[i].versionName))
            return g_interfaces[i].instance;
    }

    if (!g_vgui2Factory)
        return NULL;

    int returnCode = IFACE_FAILED;
    void* instance = g_vgui2Factory(versionName, &returnCode);
    return returnCode == IFACE_OK ? instance : NULL;
}
