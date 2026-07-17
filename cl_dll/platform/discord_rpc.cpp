#include "discord_rpc.h"

#include "../hud.h"
#include "../cl_util.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace
{
enum DiscordOpcode
{
    DISCORD_HANDSHAKE = 0,
    DISCORD_FRAME = 1
};

HANDLE g_pipe = INVALID_HANDLE_VALUE;
cvar_t* g_enabled = NULL;
cvar_t* g_applicationId = NULL;
double g_nextConnect = 0.0;
double g_nextUpdate = 0.0;
unsigned int g_nonce = 0;
__time64_t g_startedAt = 0;
char g_lastMap[64];
bool g_ready = false;

bool ValidApplicationId(const char* value)
{
    if (!value || !value[0])
        return false;
    const size_t length = strlen(value);
    return length >= 15 && length <= 24 &&
        strspn(value, "0123456789") == length;
}

void ClosePipe()
{
    if (g_pipe != INVALID_HANDLE_VALUE)
        CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
    g_ready = false;
}

bool SendFrame(int opcode, const char* json)
{
    if (g_pipe == INVALID_HANDLE_VALUE || !json)
        return false;
    const unsigned int length = static_cast<unsigned int>(strlen(json));
    if (length > 8000)
        return false;

    unsigned char frame[8008];
    memcpy(frame, &opcode, 4);
    memcpy(frame + 4, &length, 4);
    memcpy(frame + 8, json, length);
    DWORD written = 0;
    if (!WriteFile(g_pipe, frame, length + 8, &written, NULL) ||
        written != length + 8)
    {
        ClosePipe();
        return false;
    }
    return true;
}

bool Connect(const char* applicationId)
{
    if (g_pipe != INVALID_HANDLE_VALUE)
        return true;

    char pipeName[64];
    for (int index = 0; index < 10; ++index)
    {
        _snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\discord-ipc-%d", index);
        pipeName[sizeof(pipeName) - 1] = '\0';
        g_pipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
            OPEN_EXISTING, 0, NULL);
        if (g_pipe != INVALID_HANDLE_VALUE)
            break;
    }
    if (g_pipe == INVALID_HANDLE_VALUE)
        return false;

    char handshake[128];
    _snprintf(handshake, sizeof(handshake),
        "{\"v\":1,\"client_id\":\"%s\"}", applicationId);
    handshake[sizeof(handshake) - 1] = '\0';
    return SendFrame(DISCORD_HANDSHAKE, handshake);
}

void DrainPipe()
{
    if (g_pipe == INVALID_HANDLE_VALUE)
        return;

    unsigned char frame[8009];
    for (;;)
    {
        DWORD available = 0, peeked = 0;
        unsigned char header[8];
        if (!PeekNamedPipe(g_pipe, header, sizeof(header), &peeked,
            &available, NULL))
        {
            ClosePipe();
            return;
        }
        if (available < sizeof(header) || peeked < sizeof(header))
            return;

        int opcode = 0;
        unsigned int length = 0;
        memcpy(&opcode, header, 4);
        memcpy(&length, header + 4, 4);
        if (length > 8000)
        {
            ClosePipe();
            return;
        }
        const DWORD frameSize = length + 8;
        if (available < frameSize)
            return;

        DWORD totalRead = 0;
        while (totalRead < frameSize)
        {
            DWORD read = 0;
            if (!ReadFile(g_pipe, frame + totalRead, frameSize - totalRead,
                &read, NULL) || read == 0)
            {
                ClosePipe();
                return;
            }
            totalRead += read;
        }

        frame[frameSize] = '\0';
        const char* json = reinterpret_cast<const char*>(frame + 8);
        if (opcode == DISCORD_FRAME && strstr(json, "\"evt\":\"READY\""))
            g_ready = true;
        else if (opcode == 2)
        {
            ClosePipe();
            return;
        }
    }
}

void CleanMapName(char* output, int outputSize)
{
    output[0] = '\0';
    const char* level = gEngfuncs.pfnGetLevelName();
    if (!level || !level[0])
        return;
    const char* name = strrchr(level, '/');
    if (!name)
        name = strrchr(level, '\\');
    name = name ? name + 1 : level;
    _snprintf(output, outputSize, "%s", name);
    output[outputSize - 1] = '\0';
    char* extension = strrchr(output, '.');
    if (extension)
        *extension = '\0';
}
}

void CS16Discord_Init(void)
{
    g_enabled = CVAR_CREATE("cl_discord_rpc", "0", FCVAR_ARCHIVE);
    g_applicationId = CVAR_CREATE("cl_discord_appid", "", FCVAR_ARCHIVE);
    g_nextConnect = 0.0;
    g_nextUpdate = 0.0;
    g_nonce = 0;
    g_startedAt = _time64(NULL);
    g_lastMap[0] = '\0';
    g_ready = false;
}

void CS16Discord_Frame(double time)
{
    const char* applicationId = g_applicationId ? g_applicationId->string : NULL;
    if (!g_enabled || g_enabled->value == 0.0f ||
        !ValidApplicationId(applicationId))
    {
        ClosePipe();
        return;
    }

    if (g_pipe == INVALID_HANDLE_VALUE)
    {
        if (time < g_nextConnect)
            return;
        g_nextConnect = time + 5.0;
        if (!Connect(applicationId))
            return;
        g_nextUpdate = 0.0;
    }

    DrainPipe();
    if (g_pipe == INVALID_HANDLE_VALUE || !g_ready)
        return;

    char map[64];
    CleanMapName(map, sizeof(map));
    const bool activityChanged = strcmp(map, g_lastMap) != 0;
    if (!activityChanged && time < g_nextUpdate)
        return;
    g_nextUpdate = time + 15.0;

    char activity[1024];
    _snprintf(activity, sizeof(activity),
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,"
        "\"activity\":{\"details\":\"Counter-Strike 1.6\","
        "\"state\":\"%s\",\"timestamps\":{\"start\":%lld}}},"
        "\"nonce\":\"%u\"}",
        GetCurrentProcessId(), map[0] ? map : "Main menu",
        static_cast<long long>(g_startedAt), ++g_nonce);
    activity[sizeof(activity) - 1] = '\0';
    if (SendFrame(DISCORD_FRAME, activity))
    {
        strncpy(g_lastMap, map, sizeof(g_lastMap));
        g_lastMap[sizeof(g_lastMap) - 1] = '\0';
    }
}

void CS16Discord_Shutdown(void)
{
    if (g_pipe != INVALID_HANDLE_VALUE)
    {
        char activity[256];
        _snprintf(activity, sizeof(activity),
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,"
            "\"activity\":null},\"nonce\":\"%u\"}",
            GetCurrentProcessId(), ++g_nonce);
        activity[sizeof(activity) - 1] = '\0';
        SendFrame(DISCORD_FRAME, activity);
    }
    ClosePipe();
}
