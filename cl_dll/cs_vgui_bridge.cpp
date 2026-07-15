// GoldSrc/VGUI1 C ABI bridge. Do not include VGUI headers here; the actual
// Panel subclasses live in cs_vgui_viewport.cpp and are compiled with the
// Microsoft C++ ABI expected by Steam's vgui.dll.

#include "hud.h"
#include "cl_util.h"
#include "cs_vgui.h"
#include "cs_localize.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_iVisibleMouse;
void IN_ResetMouse(void);
void IN_ResetRelativeMouseState(void);
extern "C" void CS16VGUI_Print(const char* text);
extern "C" void CS16VGUI_ClientCommand(const char* command);

#if defined(_CS16CLIENT_ENABLE_VGUI1)
extern "C" int CS16VGUI_ImplStartup(void* rootPanel, int width, int height);
extern "C" void CS16VGUI_ImplShutdown(void);
extern "C" void CS16VGUI_ImplSetTeam(int team);
extern "C" int CS16VGUI_ImplShowMenu(int menuId);
extern "C" void CS16VGUI_ImplHideMenu(void);
extern "C" int CS16VGUI_ImplKeyInput(int down, int keynum, const char* currentBinding);
extern "C" int CS16VGUI_ImplShowCommandMenu(void);
extern "C" void CS16VGUI_ImplReleaseCommandMenu(void);
#endif

namespace
{
bool g_vguiInitialized = false;
bool g_vguiDisabledForSession = false;
bool g_vguiCommandsRegistered = false;
bool g_commandMenuPressed = false;
float g_commandMenuOpenTime = 0.0f;

enum
{
    MAX_COMMAND_MENU_ITEMS = 128,
    MAX_COMMAND_MENU_NODES = 32,
    MAX_COMMAND_MENU_NODE_ITEMS = 32
};

struct CommandMenuItem
{
    char boundKey;
    char text[96];
    char displayText[112];
    char command[160];
    char mapName[32];
    int parentNode;
    int childNode;
    int teamOnly;
    bool toggle;
};

struct CommandMenuNode
{
    int parentNode;
    int itemIndices[MAX_COMMAND_MENU_NODE_ITEMS];
    int itemCount;
};

CommandMenuItem g_commandMenuItems[MAX_COMMAND_MENU_ITEMS];
CommandMenuNode g_commandMenuNodes[MAX_COMMAND_MENU_NODES];
int g_commandMenuItemCount = 0;
int g_commandMenuNodeCount = 0;
bool g_commandMenuLoaded = false;
bool g_commandMenuLoadedFromFile = false;

void CopyCommandMenuText(char* destination, int destinationSize, const char* source)
{
    if (!destination || destinationSize <= 0)
        return;

    if (!source)
        source = "";

    strncpy(destination, source, destinationSize);
    destination[destinationSize - 1] = '\0';
}

void CopyCommandMenuDisplayText(char* destination, int destinationSize, const char* source)
{
    if (!destination || destinationSize <= 0)
        return;

    if (!source)
        source = "";

#if defined(_WIN32)
    // Steam's resource files are commonly UTF-8/UTF-16, while the legacy
    // VGUI1 DLL renders narrow strings through an ANSI font. Passing raw
    // multi-byte UTF-8 into vgui.dll can make its glyph lookup use invalid
    // character indices. Convert valid UTF-8 labels to the matching legacy
    // code page before they reach a VGUI1 Label.
    bool hasHighByte = false;
    for (const unsigned char* cursor = (const unsigned char*)source; *cursor; ++cursor)
    {
        if (*cursor >= 0x80)
        {
            hasHighByte = true;
            break;
        }
    }

    if (hasHighByte)
    {
        wchar_t wide[256];
        const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            source, -1, wide, (int)(sizeof(wide) / sizeof(wide[0])));
        if (wideLength > 0)
        {
            UINT codePage = GetACP();
            for (int i = 0; i < wideLength; ++i)
            {
                if (wide[i] >= 0x0400 && wide[i] <= 0x052f)
                {
                    codePage = 1251;
                    break;
                }
            }

            if (WideCharToMultiByte(codePage, 0, wide, -1, destination,
                destinationSize, NULL, NULL) > 0)
            {
                destination[destinationSize - 1] = '\0';
                return;
            }
        }
    }
#endif

    CopyCommandMenuText(destination, destinationSize, source);
}

int CreateCommandMenuNode(int parentNode)
{
    if (g_commandMenuNodeCount >= MAX_COMMAND_MENU_NODES)
        return -1;

    const int node = g_commandMenuNodeCount++;
    memset(&g_commandMenuNodes[node], 0, sizeof(g_commandMenuNodes[node]));
    g_commandMenuNodes[node].parentNode = parentNode;
    return node;
}

int AddCommandMenuItem(int node, char boundKey, const char* text,
    const char* command, const char* mapName, int teamOnly, bool toggle)
{
    if (node < 0 || node >= g_commandMenuNodeCount ||
        g_commandMenuItemCount >= MAX_COMMAND_MENU_ITEMS ||
        g_commandMenuNodes[node].itemCount >= MAX_COMMAND_MENU_NODE_ITEMS)
        return -1;

    const int itemIndex = g_commandMenuItemCount++;
    CommandMenuItem& item = g_commandMenuItems[itemIndex];
    memset(&item, 0, sizeof(item));
    item.boundKey = boundKey;
    item.parentNode = node;
    item.childNode = -1;
    item.teamOnly = teamOnly;
    item.toggle = toggle;

    const char* localized = CS16_Localize(text ? text : "");
    CopyCommandMenuDisplayText(item.text, sizeof(item.text), localized);
    CopyCommandMenuText(item.command, sizeof(item.command), command);
    CopyCommandMenuText(item.mapName, sizeof(item.mapName), mapName);

    g_commandMenuNodes[node].itemIndices[g_commandMenuNodes[node].itemCount++] = itemIndex;
    return itemIndex;
}

void AddFallbackCommandMenu(void)
{
    AddCommandMenuItem(0, '1', "CHOOSE TEAM", "chooseteam", NULL, -1, false);
    AddCommandMenuItem(0, '2', "BUY MENU", "buy", NULL, -1, false);
    AddCommandMenuItem(0, '3', "MAP BRIEFING", "showbriefing", NULL, -1, false);
    AddCommandMenuItem(0, '4', "SERVER INFO", "showinfo", NULL, -1, false);
}

void ResetCommandMenuData(void)
{
    memset(g_commandMenuItems, 0, sizeof(g_commandMenuItems));
    memset(g_commandMenuNodes, 0, sizeof(g_commandMenuNodes));
    g_commandMenuItemCount = 0;
    g_commandMenuNodeCount = 0;
    g_commandMenuLoaded = false;
    g_commandMenuLoadedFromFile = false;
}

void ParseCommandMenuFile(void)
{
    if (g_commandMenuLoaded)
        return;

    ResetCommandMenuData();
    g_commandMenuLoaded = true;
    CreateCommandMenuNode(-1);

    if (!gEngfuncs.COM_LoadFile || !gEngfuncs.COM_ParseFile || !gEngfuncs.COM_FreeFile)
    {
        AddFallbackCommandMenu();
        return;
    }

    int fileLength = 0;
    byte* source = gEngfuncs.COM_LoadFile("commandmenu.txt", 5, &fileLength);
    if (!source)
    {
        AddFallbackCommandMenu();
        return;
    }

    char token[1024];
    char* cursor = (char*)source;
    if (fileLength >= 3 && source[0] == 0xef && source[1] == 0xbb && source[2] == 0xbf)
        cursor += 3;
    int currentNode = 0;
    int lastItem = -1;

    while ((cursor = gEngfuncs.COM_ParseFile(cursor, token)) != NULL && token[0])
    {
        if (!strcmp(token, "{"))
        {
            if (lastItem >= 0 && g_commandMenuItems[lastItem].childNode < 0)
            {
                const int child = CreateCommandMenuNode(currentNode);
                if (child >= 0)
                {
                    g_commandMenuItems[lastItem].childNode = child;
                    currentNode = child;
                }
            }
            continue;
        }

        if (!strcmp(token, "}"))
        {
            if (currentNode > 0)
                currentNode = g_commandMenuNodes[currentNode].parentNode;
            lastItem = -1;
            continue;
        }

        char mapName[32] = "";
        int teamOnly = -1;
        bool toggle = false;
        bool custom = false;

        if (!stricmp(token, "CUSTOM"))
        {
            custom = true;
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;
        }
        else if (!stricmp(token, "MAP"))
        {
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;
            CopyCommandMenuText(mapName, sizeof(mapName), token);

            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;
        }
        else if (!strnicmp(token, "TEAM", 4))
        {
            teamOnly = atoi(token + 4);
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;
        }
        else if (!strnicmp(token, "TOGGLE", 6))
        {
            toggle = true;
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;
        }

        const char boundKey = token[0];

        char text[256];
        cursor = gEngfuncs.COM_ParseFile(cursor, text);
        if (!cursor)
            break;

        char command[256];
        cursor = gEngfuncs.COM_ParseFile(cursor, command);
        if (!cursor)
            break;

        // The stock GoldSrc viewport implements !CHANGETEAM as a custom
        // generated submenu. Reuse CS's normal command here so the existing
        // VGUI team panel is opened instead of sending an unknown ! command.
        if (custom && !stricmp(command, "!CHANGETEAM"))
            CopyCommandMenuText(command, sizeof(command), "chooseteam");
        else if (custom && command[0] == '!')
            command[0] = '\0';

        lastItem = AddCommandMenuItem(currentNode, boundKey, text,
            !strcmp(command, "{") ? "" : command,
            mapName, teamOnly, toggle);

        if (lastItem >= 0 && !strcmp(command, "{"))
        {
            const int child = CreateCommandMenuNode(currentNode);
            if (child >= 0)
            {
                g_commandMenuItems[lastItem].childNode = child;
                currentNode = child;
            }
        }
    }

    gEngfuncs.COM_FreeFile(source);
    g_commandMenuLoadedFromFile = g_commandMenuNodes[0].itemCount > 0;
    if (!g_commandMenuLoadedFromFile)
        AddFallbackCommandMenu();
}

int CurrentCommandMenuTeam(void)
{
    int team = g_iTeamNumber;
    if (team != TEAM_TERRORIST && team != TEAM_CT)
    {
        const int player = gHUD.m_Scoreboard.m_iPlayerNum;
        if (player > 0 && player <= MAX_PLAYERS)
            team = g_PlayerExtraInfo[player].teamnumber;
    }
    return team;
}

bool CommandMenuMapMatches(const char* wantedMap)
{
    if (!wantedMap || !wantedMap[0])
        return true;

    const char* levelName = gEngfuncs.pfnGetLevelName
        ? gEngfuncs.pfnGetLevelName()
        : NULL;
    if (!levelName || !levelName[0])
        return false;

    const char* base = strrchr(levelName, '/');
    if (!base)
        base = strrchr(levelName, '\\');
    base = base ? base + 1 : levelName;

    char currentMap[64];
    CopyCommandMenuText(currentMap, sizeof(currentMap), base);
    char* extension = strrchr(currentMap, '.');
    if (extension)
        *extension = '\0';

    return !stricmp(currentMap, wantedMap);
}

bool CommandMenuItemVisible(const CommandMenuItem& item)
{
    if (item.teamOnly >= 0 && item.teamOnly != CurrentCommandMenuTeam())
        return false;
    return CommandMenuMapMatches(item.mapName);
}

int GetVisibleCommandMenuItem(int node, int visibleIndex)
{
    ParseCommandMenuFile();
    if (node < 0 || node >= g_commandMenuNodeCount || visibleIndex < 0)
        return -1;

    int visible = 0;
    const CommandMenuNode& menuNode = g_commandMenuNodes[node];
    for (int i = 0; i < menuNode.itemCount; ++i)
    {
        const int itemIndex = menuNode.itemIndices[i];
        if (!CommandMenuItemVisible(g_commandMenuItems[itemIndex]))
            continue;
        if (visible++ == visibleIndex)
            return itemIndex;
    }
    return -1;
}

void ExecuteCommandMenuItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= g_commandMenuItemCount)
        return;

    CommandMenuItem& item = g_commandMenuItems[itemIndex];
    if (!item.command[0])
        return;

    char command[224];
    if (item.toggle && gEngfuncs.pfnGetCvarPointer)
    {
        cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(item.command);
        if (cvar)
            snprintf(command, sizeof(command), "%s %d\n", item.command, cvar->value == 0.0f ? 1 : 0);
        else
            snprintf(command, sizeof(command), "%s\n", item.command);
    }
    else
    {
        snprintf(command, sizeof(command), "%s\n", item.command);
    }

    command[sizeof(command) - 1] = '\0';
    CS16VGUI_ClientCommand(command);
}

void CS16VGUI_TestTeam_f()
{
    if (!CS16VGUI_ShowMenu(2))
        CS16VGUI_Print("[CS16 GoldSrc] VGUI1 team menu is unavailable.\n");
}

void CS16VGUI_TestClassT_f()
{
    if (!CS16VGUI_ShowMenu(26))
        CS16VGUI_Print("[CS16 GoldSrc] VGUI1 terrorist class menu is unavailable.\n");
}

void CS16VGUI_TestClassCT_f()
{
    if (!CS16VGUI_ShowMenu(27))
        CS16VGUI_Print("[CS16 GoldSrc] VGUI1 counter-terrorist class menu is unavailable.\n");
}

void CS16VGUI_TestBuy_f()
{
    if (!CS16VGUI_ShowMenu(28))
        CS16VGUI_Print("[CS16 GoldSrc] VGUI1 buy menu is unavailable.\n");
}

void CS16VGUI_CommandMenuPress_f()
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    CS16_StartupTrace("VGUI1: command menu press enter");
    g_commandMenuPressed = false;
    if (!CS16VGUI_IsAvailable())
    {
        CS16_StartupTrace("VGUI1: command menu unavailable");
        return;
    }

    if (CS16VGUI_ImplShowCommandMenu())
    {
        g_commandMenuPressed = true;
        g_commandMenuOpenTime = gHUD.m_flTime;
        CS16_StartupTrace("VGUI1: command menu press complete");
    }
#endif
}

void CS16VGUI_CommandMenuRelease_f()
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (!g_commandMenuPressed)
        return;

    g_commandMenuPressed = false;
    if ((gHUD.m_flTime - g_commandMenuOpenTime) >= 0.3f)
        CS16VGUI_ImplReleaseCommandMenu();
#endif
}

void CS16VGUI_ReloadCommandMenu_f()
{
    ResetCommandMenuData();
    ParseCommandMenuFile();
    CS16VGUI_Print(g_commandMenuLoadedFromFile
        ? "[CS16 GoldSrc] commandmenu.txt reloaded.\n"
        : "[CS16 GoldSrc] commandmenu.txt unavailable; built-in fallback loaded.\n");
}

void CS16VGUI_Hide_f()
{
    CS16VGUI_HideMenu();
}
}

extern "C" int CS16VGUI_CommandMenuPrepare(void)
{
    ParseCommandMenuFile();
    return g_commandMenuNodeCount > 0 && g_commandMenuNodes[0].itemCount > 0;
}

extern "C" int CS16VGUI_CommandMenuGetCount(int node)
{
    ParseCommandMenuFile();
    if (node < 0 || node >= g_commandMenuNodeCount)
        return 0;

    int count = 0;
    const CommandMenuNode& menuNode = g_commandMenuNodes[node];
    for (int i = 0; i < menuNode.itemCount; ++i)
    {
        if (CommandMenuItemVisible(g_commandMenuItems[menuNode.itemIndices[i]]))
            ++count;
    }
    return count;
}

extern "C" int CS16VGUI_CommandMenuGetParent(int node)
{
    ParseCommandMenuFile();
    if (node < 0 || node >= g_commandMenuNodeCount)
        return -1;
    return g_commandMenuNodes[node].parentNode;
}

extern "C" int CS16VGUI_CommandMenuGetItem(int node, int visibleIndex,
    const char** displayText, int* itemIndex, int* childNode, int* boundKey)
{
    const int foundItem = GetVisibleCommandMenuItem(node, visibleIndex);
    if (foundItem < 0)
        return 0;

    CommandMenuItem& item = g_commandMenuItems[foundItem];
    snprintf(item.displayText, sizeof(item.displayText), "%c  %s%s",
        item.boundKey ? item.boundKey : ' ', item.text,
        item.childNode >= 0 ? "  >" : "");
    item.displayText[sizeof(item.displayText) - 1] = '\0';

    if (displayText)
        *displayText = item.displayText;
    if (itemIndex)
        *itemIndex = foundItem;
    if (childNode)
        *childNode = item.childNode;
    if (boundKey)
        *boundKey = (unsigned char)item.boundKey;
    return 1;
}

extern "C" void CS16VGUI_CommandMenuExecute(int itemIndex)
{
    ExecuteCommandMenuItem(itemIndex);
}

extern "C" int CS16VGUI_LocalizeResourceText(const char* text, char* output, int outputSize)
{
    if (!output || outputSize <= 0)
        return 0;

    const char* localized = CS16_Localize(text ? text : "");
    CopyCommandMenuDisplayText(output, outputSize, localized);
    return output[0] != '\0';
}

extern "C" int CS16VGUI_LoadResourceLayout(const char* filename,
    cs16_vgui_resource_control_t* controls, int maxControls)
{
    if (!filename || !filename[0] || !controls || maxControls <= 0 ||
        !gEngfuncs.COM_LoadFile || !gEngfuncs.COM_ParseFile || !gEngfuncs.COM_FreeFile)
        return 0;

    int fileLength = 0;
    byte* source = gEngfuncs.COM_LoadFile((char*)filename, 5, &fileLength);
    if (!source)
        return 0;

    char token[1024];
    char* cursor = (char*)source;
    if (fileLength >= 3 && source[0] == 0xef && source[1] == 0xbb && source[2] == 0xbf)
        cursor += 3;

    // Skip the resource name and enter its root KeyValues block.
    cursor = gEngfuncs.COM_ParseFile(cursor, token);
    if (!cursor)
    {
        gEngfuncs.COM_FreeFile(source);
        return 0;
    }
    cursor = gEngfuncs.COM_ParseFile(cursor, token);
    if (!cursor || strcmp(token, "{"))
    {
        gEngfuncs.COM_FreeFile(source);
        return 0;
    }

    int count = 0;
    while (count < maxControls && (cursor = gEngfuncs.COM_ParseFile(cursor, token)) != NULL)
    {
        if (!strcmp(token, "}"))
            break;

        cs16_vgui_resource_control_t& control = controls[count];
        memset(&control, 0, sizeof(control));
        control.visible = 1;
        control.enabled = 1;
        CopyCommandMenuText(control.fieldName, sizeof(control.fieldName), token);

        cursor = gEngfuncs.COM_ParseFile(cursor, token);
        if (!cursor || strcmp(token, "{"))
            break;

        while ((cursor = gEngfuncs.COM_ParseFile(cursor, token)) != NULL)
        {
            if (!strcmp(token, "}"))
                break;

            char key[128];
            CopyCommandMenuText(key, sizeof(key), token);
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            if (!cursor)
                break;

            if (!stricmp(key, "fieldName"))
                CopyCommandMenuText(control.fieldName, sizeof(control.fieldName), token);
            else if (!stricmp(key, "ControlName"))
                CopyCommandMenuText(control.controlName, sizeof(control.controlName), token);
            else if (!stricmp(key, "labelText"))
                CopyCommandMenuText(control.labelText, sizeof(control.labelText), token);
            else if (!stricmp(key, "command"))
                CopyCommandMenuText(control.command, sizeof(control.command), token);
            else if (!stricmp(key, "textAlignment"))
                CopyCommandMenuText(control.textAlignment, sizeof(control.textAlignment), token);
            else if (!stricmp(key, "font"))
                CopyCommandMenuText(control.font, sizeof(control.font), token);
            else if (!stricmp(key, "xpos"))
                control.xpos = atoi(token);
            else if (!stricmp(key, "ypos"))
                control.ypos = atoi(token);
            else if (!stricmp(key, "wide"))
                control.wide = atoi(token);
            else if (!stricmp(key, "tall"))
                control.tall = atoi(token);
            else if (!stricmp(key, "visible"))
                control.visible = atoi(token);
            else if (!stricmp(key, "enabled"))
                control.enabled = atoi(token);
        }

        ++count;
        if (!cursor)
            break;
    }

    gEngfuncs.COM_FreeFile(source);
    return count;
}

extern "C" void* CS16VGUI_GetRootPanel(void)
{
    return gEngfuncs.VGui_GetPanel ? gEngfuncs.VGui_GetPanel() : NULL;
}

extern "C" void CS16VGUI_ClientCommand(const char* command)
{
	if (command && !strncmp(command, "joinclass ", 10))
	{
		CS16_SetRuntimeTrace(true);
		CS16_StartupTrace("VGUI1: joinclass command");
	}

    if (command && command[0] && gEngfuncs.pfnClientCmd)
        gEngfuncs.pfnClientCmd((char*)command);
}

extern "C" void CS16VGUI_SetMouseVisible(int visible)
{
    const int next = visible ? 1 : 0;
    if (g_iVisibleMouse == next)
        return;

    if (!next)
    {
        IN_ResetMouse();
        IN_ResetRelativeMouseState();
    }

    g_iVisibleMouse = next;
}

extern "C" void CS16VGUI_Print(const char* text)
{
    if (text && gEngfuncs.Con_Printf)
        gEngfuncs.Con_Printf("%s", text);
}

extern "C" void CS16VGUI_Trace(const char* stage)
{
    if (stage)
        CS16_StartupTrace(stage);
}

extern "C" int CS16VGUI_Startup(int width, int height)
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    CS16_StartupTrace("VGUI1: startup enter");
    void* root = CS16VGUI_GetRootPanel();
    if (!root)
    {
        CS16_StartupTrace("VGUI1: root panel unavailable");
        g_vguiInitialized = false;
        return 0;
    }

    CS16_StartupTrace("VGUI1: viewport attach enter");
    g_vguiInitialized = CS16VGUI_ImplStartup(root, width, height) != 0;
    if (g_vguiInitialized)
    {
        CS16_StartupTrace("VGUI1: viewport attach complete");
        CS16VGUI_Print("[CS16 GoldSrc] VGUI1 viewport initialized.\n");
    }
    else
    {
        CS16_StartupTrace("VGUI1: viewport attach failed");
    }
    return g_vguiInitialized ? 1 : 0;
#else
    (void)width;
    (void)height;
    return 0;
#endif
}

extern "C" void CS16VGUI_Shutdown(void)
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (g_vguiInitialized)
        CS16VGUI_ImplShutdown();
#endif
    g_vguiInitialized = false;
    g_vguiDisabledForSession = false;
    CS16VGUI_SetMouseVisible(0);
}

extern "C" void CS16VGUI_ResetSession(void)
{
	CS16_SetRuntimeTrace(false);
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (!g_vguiCommandsRegistered && gEngfuncs.pfnAddCommand)
    {
        gEngfuncs.pfnAddCommand("cs_vgui_team", CS16VGUI_TestTeam_f);
        gEngfuncs.pfnAddCommand("cs_vgui_class", CS16VGUI_TestClassT_f);
        gEngfuncs.pfnAddCommand("cs_vgui_class_t", CS16VGUI_TestClassT_f);
        gEngfuncs.pfnAddCommand("cs_vgui_class_ct", CS16VGUI_TestClassCT_f);
        gEngfuncs.pfnAddCommand("cs_vgui_buy", CS16VGUI_TestBuy_f);
        gEngfuncs.pfnAddCommand("cs_vgui_hide", CS16VGUI_Hide_f);
        gEngfuncs.pfnAddCommand("cs_vgui_reload_commandmenu", CS16VGUI_ReloadCommandMenu_f);
        gEngfuncs.pfnAddCommand("+commandmenu", CS16VGUI_CommandMenuPress_f);
        gEngfuncs.pfnAddCommand("-commandmenu", CS16VGUI_CommandMenuRelease_f);
        g_vguiCommandsRegistered = true;
    }
#endif
    ResetCommandMenuData();
    g_commandMenuPressed = false;
    g_commandMenuOpenTime = 0.0f;
    g_vguiDisabledForSession = false;
    CS16VGUI_HideMenu();
}

extern "C" int CS16VGUI_IsAvailable(void)
{
    cvar_t* enabled = gEngfuncs.pfnGetCvarPointer
        ? gEngfuncs.pfnGetCvarPointer("cs_vgui_enable")
        : NULL;
    return g_vguiInitialized && !g_vguiDisabledForSession && (!enabled || enabled->value != 0.0f);
}

extern "C" int CS16VGUI_ShowMenu(int menuId)
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (menuId == 2)
        CS16_StartupTrace("VGUI1: show team menu enter");
    else if (menuId == 26)
        CS16_StartupTrace("VGUI1: show terrorist class menu enter");
    else if (menuId == 27)
        CS16_StartupTrace("VGUI1: show counter-terrorist class menu enter");
    else if (menuId >= 28 && menuId <= 34)
        CS16_StartupTrace("VGUI1: show buy menu enter");

    int team = g_iTeamNumber;
    if (team != TEAM_TERRORIST && team != TEAM_CT)
    {
        const int player = gHUD.m_Scoreboard.m_iPlayerNum;
        if (player > 0 && player <= MAX_PLAYERS)
            team = g_PlayerExtraInfo[player].teamnumber;
    }
    CS16VGUI_ImplSetTeam(team == TEAM_CT ? TEAM_CT : TEAM_TERRORIST);

    if (CS16VGUI_IsAvailable() && CS16VGUI_ImplShowMenu(menuId))
    {
        CS16_StartupTrace("VGUI1: show menu complete");
        return 1;
    }
#else
    (void)menuId;
#endif
    return 0;
}

extern "C" void CS16VGUI_HideMenu(void)
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (g_vguiInitialized)
        CS16VGUI_ImplHideMenu();
#endif
}

extern "C" void CS16VGUI_DisableForSession(void)
{
    g_vguiDisabledForSession = true;
    CS16VGUI_HideMenu();
}

extern "C" int CS16VGUI_KeyInput(int down, int keynum, const char* currentBinding)
{
#if defined(_CS16CLIENT_ENABLE_VGUI1)
    if (CS16VGUI_IsAvailable())
        return CS16VGUI_ImplKeyInput(down, keynum, currentBinding);
#else
    (void)down;
    (void)keynum;
    (void)currentBinding;
#endif
    return 0;
}
