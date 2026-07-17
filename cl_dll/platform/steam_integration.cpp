#include "steam_integration.h"

#include "../hud.h"
#include "../cl_util.h"
#include "../ui/vgui2/cs_vgui2.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace
{
typedef void* (__cdecl* SteamInterfaceFn)();
typedef int (__cdecl* GetSmallFriendAvatarFn)(void*, unsigned long long);
typedef bool (__cdecl* RequestUserInformationFn)(void*, unsigned long long, bool);
typedef bool (__cdecl* GetImageSizeFn)(void*, int, unsigned int*, unsigned int*);
typedef bool (__cdecl* GetImageRGBAFn)(void*, int, unsigned char*, int);
typedef bool (__cdecl* SetRichPresenceFn)(void*, const char*, const char*);
typedef void (__cdecl* ClearRichPresenceFn)(void*);

struct SteamApi
{
    HMODULE module;
    void* friends;
    void* utils;
    GetSmallFriendAvatarFn getSmallFriendAvatar;
    RequestUserInformationFn requestUserInformation;
    GetImageSizeFn getImageSize;
    GetImageRGBAFn getImageRGBA;
    SetRichPresenceFn setRichPresence;
    ClearRichPresenceFn clearRichPresence;
    bool ready;
};

struct AvatarCache
{
    unsigned long long steamId;
    float nextRetry;
    int wide, tall;
    bool valid;
    unsigned char rgba[64 * 64 * 4];
};

SteamApi g_steam;
AvatarCache g_avatars[33];
cvar_t* g_steamPresence = NULL;
double g_nextPresenceUpdate = 0.0;
char g_lastStatus[128];

template <typename T>
T SteamProc(const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(g_steam.module, name));
}

bool ResolveSteamApi()
{
    if (g_steam.ready)
        return true;

    g_steam.module = GetModuleHandleA("steam_api.dll");
    if (!g_steam.module)
        return false;

    SteamInterfaceFn getFriends = SteamProc<SteamInterfaceFn>("SteamAPI_SteamFriends_v017");
    SteamInterfaceFn getUtils = SteamProc<SteamInterfaceFn>("SteamAPI_SteamUtils_v010");
    g_steam.getSmallFriendAvatar = SteamProc<GetSmallFriendAvatarFn>(
        "SteamAPI_ISteamFriends_GetSmallFriendAvatar");
    g_steam.requestUserInformation = SteamProc<RequestUserInformationFn>(
        "SteamAPI_ISteamFriends_RequestUserInformation");
    g_steam.getImageSize = SteamProc<GetImageSizeFn>("SteamAPI_ISteamUtils_GetImageSize");
    g_steam.getImageRGBA = SteamProc<GetImageRGBAFn>("SteamAPI_ISteamUtils_GetImageRGBA");
    g_steam.setRichPresence = SteamProc<SetRichPresenceFn>(
        "SteamAPI_ISteamFriends_SetRichPresence");
    g_steam.clearRichPresence = SteamProc<ClearRichPresenceFn>(
        "SteamAPI_ISteamFriends_ClearRichPresence");

    if (!getFriends || !getUtils || !g_steam.getSmallFriendAvatar ||
        !g_steam.getImageSize || !g_steam.getImageRGBA)
        return false;

    g_steam.friends = getFriends();
    g_steam.utils = getUtils();
    g_steam.ready = g_steam.friends != NULL && g_steam.utils != NULL;
    return g_steam.ready;
}

const char* CurrentMapName()
{
    const char* level = gEngfuncs.pfnGetLevelName();
    if (!level || !level[0])
        return NULL;
    const char* name = strrchr(level, '/');
    if (!name)
        name = strrchr(level, '\\');
    return name ? name + 1 : level;
}
}

void CS16Steam_Init(void)
{
    memset(&g_steam, 0, sizeof(g_steam));
    memset(g_avatars, 0, sizeof(g_avatars));
    g_lastStatus[0] = '\0';
    g_nextPresenceUpdate = 0.0;
    g_steamPresence = CVAR_CREATE("cl_steam_rich_presence", "1", FCVAR_ARCHIVE);
}

void CS16Steam_QueueAvatar(int playerIndex, unsigned long long steamId,
    int x, int y, int size, int alpha, float time)
{
    if (playerIndex <= 0 || playerIndex >= 33 || steamId == 0 ||
        !CS16VGUI2_IsViewportReady())
        return;

    AvatarCache& avatar = g_avatars[playerIndex];
    if (avatar.steamId != steamId)
    {
        memset(&avatar, 0, sizeof(avatar));
        avatar.steamId = steamId;
    }

    if (!avatar.valid && time >= avatar.nextRetry && ResolveSteamApi())
    {
        const int image = g_steam.getSmallFriendAvatar(g_steam.friends, steamId);
        if (image <= 0)
        {
            if (g_steam.requestUserInformation)
                g_steam.requestUserInformation(g_steam.friends, steamId, false);
            avatar.nextRetry = time + 1.0f;
        }
        else
        {
            unsigned int wide = 0, tall = 0;
            if (g_steam.getImageSize(g_steam.utils, image, &wide, &tall) &&
                wide > 0 && tall > 0 && wide <= 64 && tall <= 64 &&
                g_steam.getImageRGBA(g_steam.utils, image, avatar.rgba,
                    static_cast<int>(wide * tall * 4)))
            {
                avatar.wide = static_cast<int>(wide);
                avatar.tall = static_cast<int>(tall);
                avatar.valid = true;
                CS16VGUI2_SetHudAvatar(playerIndex, steamId, avatar.rgba,
                    avatar.wide, avatar.tall);
            }
            else
            {
                avatar.nextRetry = time + 2.0f;
            }
        }
    }

    if (avatar.valid)
        CS16VGUI2_DrawHudAvatar(playerIndex, x, y, size, alpha);
}

void CS16Steam_Frame(double time)
{
    if (!g_steamPresence || g_steamPresence->value == 0.0f ||
        time < g_nextPresenceUpdate || !ResolveSteamApi() ||
        !g_steam.setRichPresence)
        return;

    g_nextPresenceUpdate = time + 15.0;
    char status[128];
    const char* map = CurrentMapName();
    if (map)
    {
        char cleanMap[64];
        strncpy(cleanMap, map, sizeof(cleanMap));
        cleanMap[sizeof(cleanMap) - 1] = '\0';
        char* extension = strrchr(cleanMap, '.');
        if (extension)
            *extension = '\0';
        sprintf(status, "Counter-Strike 1.6 - %s", cleanMap);
    }
    else
    {
        strcpy(status, "Counter-Strike 1.6");
    }

    if (strcmp(status, g_lastStatus))
    {
        g_steam.setRichPresence(g_steam.friends, "status", status);
        strncpy(g_lastStatus, status, sizeof(g_lastStatus));
        g_lastStatus[sizeof(g_lastStatus) - 1] = '\0';
    }
}

void CS16Steam_Shutdown(void)
{
    if (g_steam.ready && g_steam.clearRichPresence && g_steam.friends)
        g_steam.clearRichPresence(g_steam.friends);
    memset(&g_steam, 0, sizeof(g_steam));
    memset(g_avatars, 0, sizeof(g_avatars));
}
