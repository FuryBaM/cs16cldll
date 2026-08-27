// spectator_gui.cpp (GoldSrc-only, без Xash/Mobile)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "draw_util.h"
#include "ammo.h"
#include "pm_shared.h"
#include "platform/steam_integration.h"
#include "ui/common/hud_style.h"

#include <ctype.h>

#define XPOS(x)      ((x) / 16.0f)
#define INT_XPOS(x)  int(XPOS(x) * ScreenWidth)

namespace
{
void DrawCenteredHudString(int centerX, int y, const char* text,
    int r, int g, int b)
{
    const int width = DrawUtils::HudStringLen(text);
    DrawUtils::DrawHudString(centerX - width / 2, y,
        centerX + width / 2 + 4, text, r, g, b);
}

int CollectTeamPlayers(int team, int* players, int capacity)
{
    int count = 0;
    for (int index = 1; index <= MAX_PLAYERS && count < capacity; ++index)
    {
        if (!g_PlayerInfoList[index].name ||
            !g_PlayerInfoList[index].name[0] ||
            g_PlayerExtraInfo[index].teamnumber != team)
            continue;
        players[count++] = index;
    }
    return count;
}

void DrawRosterPlayer(int playerIndex, int x, int y, int size, int team,
    bool observed)
{
    int r, g, b;
    GetTeamColor(r, g, b, team);
    const bool dead = g_PlayerExtraInfo[playerIndex].dead;
    const int frameAlpha = observed ? 255 : (dead ? 55 : 145);
    const int avatarAlpha = dead ? 65 : 255;

    if (observed)
        FillRGBABlend(x - 3, y - 3, size + 6, size + 8,
            255, 175, 40, 240);
    else
        FillRGBABlend(x - 2, y - 2, size + 4, size + 6,
            r, g, b, frameAlpha);
    FillRGBABlend(x, y, size, size, r, g, b, dead ? 25 : 75);
    CS16Steam_QueueAvatar(playerIndex, g_PlayerInfoList[playerIndex].m_nSteamID,
        x, y, size, avatarAlpha, gHUD.m_flTime);

    const int health = g_PlayerExtraInfo[playerIndex].health;
    if (!dead && health > 0)
    {
        const int barWide = min(size, max(1, health * size / 100));
        FillRGBABlend(x, y + size + 2, size, 2, 20, 20, 20, 190);
        FillRGBABlend(x, y + size + 2, barWide, 2,
            health <= 25 ? 255 : 80, health <= 25 ? 55 : 220, 60, 230);
    }
    if (g_PlayerExtraInfo[playerIndex].talking)
        FillRGBABlend(x + size - 5, y + size - 5, 6, 6,
            60, 255, 90, 255);
}

void DrawTeamRoster(int team, bool rightSide, int centerPanelWide, int y)
{
    int players[MAX_PLAYERS];
    const int count = CollectTeamPlayers(team, players, MAX_PLAYERS);
    if (!count)
        return;

    const int desired = ScreenHeight >= 1000 ? 32 :
        (ScreenHeight >= 720 ? 28 : 22);
    const int available = max(100, ScreenWidth / 2 - centerPanelWide / 2 - 14);
    const int size = min(desired, max(12, available / count - 5));
    const int step = size + 5;
    const int edge = ScreenWidth / 2 + (rightSide ? centerPanelWide / 2 + 8 :
        -centerPanelWide / 2 - 8);

    for (int ordinal = 0; ordinal < count; ++ordinal)
    {
        const int x = rightSide ? edge + ordinal * step :
            edge - size - ordinal * step;
        DrawRosterPlayer(players[ordinal], x, y, size, team,
            players[ordinal] == g_iUser2);
    }
}

void FormatWeaponName(const WEAPON* weapon, char* output, int outputSize)
{
    if (!weapon || !weapon->szName[0])
    {
        strncpy(output, "NO WEAPON", outputSize);
        output[outputSize - 1] = '\0';
        return;
    }
    const char* source = weapon->szName;
    if (!strnicmp(source, "weapon_", 7))
        source += 7;
    int written = 0;
    while (*source && written < outputSize - 1)
    {
        output[written++] = (char)toupper((unsigned char)*source++);
    }
    output[written] = '\0';
}

void DrawObservedPlayerCard(int target, int textTall)
{
    if (target <= 0 || target > MAX_PLAYERS ||
        !g_PlayerInfoList[target].name)
        return;

    const int panelWide = min(570, ScreenWidth - 30);
    const int panelTall = max(76, textTall * 4 + 12);
    const int panelX = (ScreenWidth - panelWide) / 2;
    const int panelY = ScreenHeight - panelTall - 10;
    const int avatarSize = min(54, panelTall - 18);
    const int avatarX = panelX + 12;
    const int avatarY = panelY + (panelTall - avatarSize) / 2;
    int r, g, b;
    GetTeamColor(r, g, b, g_PlayerExtraInfo[target].teamnumber);

    FillRGBABlend(panelX, panelY, panelWide, panelTall, 0, 0, 0, 185);
    FillRGBABlend(panelX, panelY, 5, panelTall, r, g, b, 245);
    FillRGBABlend(panelX + 5, panelY, panelWide - 5, 1, r, g, b, 175);
    FillRGBABlend(avatarX, avatarY, avatarSize, avatarSize, r, g, b, 80);
    CS16Steam_QueueAvatar(target, g_PlayerInfoList[target].m_nSteamID,
        avatarX, avatarY, avatarSize, 255, gHUD.m_flTime);

    const int infoX = avatarX + avatarSize + 12;
    DrawUtils::DrawHudString(infoX, panelY + 10, panelX + panelWide - 190,
        g_PlayerInfoList[target].name, r, g, b);

    const WEAPON* weapon = g_iUser1 == OBS_IN_EYE ?
        gHUD.m_Ammo.GetCurrentWeapon() : NULL;
    char weaponName[64];
    FormatWeaponName(weapon, weaponName, sizeof(weaponName));
    DrawUtils::DrawHudString(infoX, panelY + 12 + textTall,
        panelX + panelWide - 190, weaponName, 205, 205, 205);

    const int health = max(0, g_PlayerExtraInfo[target].health);
    const int barY = panelY + panelTall - 13;
    const int barWide = min(180, panelWide / 3);
    FillRGBABlend(infoX, barY, barWide, 4, 35, 35, 35, 230);
    FillRGBABlend(infoX, barY, min(100, health) * barWide / 100, 4,
        health <= 25 ? 255 : 75, health <= 25 ? 55 : 220, 55, 245);

    const int statsX = panelX + panelWide - 175;
    FillRGBABlend(statsX - 12, panelY + 9, 1, panelTall - 18,
        255, 255, 255, 45);
    char stats[32];
    snprintf(stats, sizeof(stats), "HP %d", health);
    DrawUtils::DrawHudString(statsX, panelY + 11,
        panelX + panelWide - 8, stats, 235, 235, 235);
}
}

DECLARE_MESSAGE(m_SpectatorGui, SpecHealth)
DECLARE_MESSAGE(m_SpectatorGui, SpecHealth2)
DECLARE_COMMAND(m_SpectatorGui, ToggleSpectatorMenu)

int CHudSpectatorGui::Init()
{
    // только сообщения; никаких команд и мобильных кнопок
    HOOK_MESSAGE(SpecHealth);
    HOOK_MESSAGE(SpecHealth2);

    // Duck (CTRL by default) toggles the spectator panels, like it does in
    // the stock client. The same command is exposed to the console so it can
    // be bound to another key.
    HOOK_COMMAND("_spec_toggle_menu", ToggleSpectatorMenu);

    gHUD.AddHudElem(this);
    CS16_HudStyleInit();
    m_iFlags = HUD_DRAW;
    m_bBombPlanted = false;
    m_menuFlags = ROOT_MENU;
    label.m_szMap[0] = '\0';
    return 1;
}

int CHudSpectatorGui::VidInit()
{
    // без текстур/RenderAPI
    return 1;
}

void CHudSpectatorGui::Shutdown()
{
    // ничего
}

// Shows and hides the spectator panels. The stock client opens its VGUI
// spectator menu here; this port draws the panels itself, so duck simply
// switches them off for an unobstructed view and back on again.
void CHudSpectatorGui::UserCmd_ToggleSpectatorMenu()
{
    m_menuFlags ^= ROOT_MENU;
}

int CHudSpectatorGui::Draw(float flTime)
{
    if (!g_iUser1)  // не в режиме спектатора
        return 1;

    if (!(m_menuFlags & ROOT_MENU))  // panels hidden with duck
        return 1;

    CalcAllNeededData();
    gHUD.m_Scoreboard.GetAllPlayersInfo();

    // Velaron/Xash uses a logical scaled HUD, where INT_YPOS(2) and the font
    // grow together. Steam GoldSrc reports physical resolution but keeps its
    // console font nearly pixel-sized; at 1080p the old formula produced two
    // mostly empty 216 px bars around 13 px text. Size the bars from the real
    // font metrics instead.
    const int textTall = max(DrawUtils::HudTextTall(), 13);
    const int lineGap = max(textTall / 3, 3);
    const int topPadding = max(textTall, 10);
    const int topBarTall = max(64,
        topPadding * 2 + textTall * 2 + lineGap);
    const int bottomBarTall = max(48, textTall * 3);
    const int firstLineY = topPadding;
    const int secondLineY = firstLineY + textTall + lineGap;

    if (CS16_HudStyleModern(CS16_HUD_SPECTATOR))
    {
        const int centerPanelWide = ScreenWidth >= 1000 ? 180 : 140;
        const int topBarTall = max(58, textTall * 3 + 14);
        FillRGBABlend(0, 0, ScreenWidth, topBarTall, 0, 0, 0, 165);
        FillRGBABlend(0, topBarTall - 2, ScreenWidth / 2, 2,
            210, 70, 55, 180);
        FillRGBABlend(ScreenWidth / 2, topBarTall - 2,
            ScreenWidth - ScreenWidth / 2, 2, 70, 145, 235, 180);

        DrawTeamRoster(TEAM_TERRORIST, false, centerPanelWide, 7);
        DrawTeamRoster(TEAM_CT, true, centerPanelWide, 7);

        DrawCenteredHudString(ScreenWidth / 2, 5, label.m_szMap,
            220, 220, 220);
        char score[64];
        snprintf(score, sizeof(score), "T  %d   :   %d  CT",
            label.m_iTerrorists, label.m_iCounterTerrorists);
        DrawCenteredHudString(ScreenWidth / 2, 7 + textTall, score,
            255, 160, 35);
        DrawCenteredHudString(ScreenWidth / 2, 9 + textTall * 2,
            m_bBombPlanted ? "C4 PLANTED" : label.m_szTimer,
            m_bBombPlanted ? 255 : 220, m_bBombPlanted ? 60 : 220,
            m_bBombPlanted ? 40 : 220);

        DrawObservedPlayerCard(g_iUser2, textTall);
        return 1;
    }

    FillRGBABlend(0, 0, ScreenWidth, topBarTall, 0, 0, 0, 153);
    FillRGBABlend(0, ScreenHeight - bottomBarTall,
        ScreenWidth, bottomBarTall, 0, 0, 0, 153);

    int r = 255, g = 140, b = 0;

    // разделитель и подписи справа
    const int dividerTop = max(firstLineY - lineGap, 0);
    const int dividerTall = secondLineY + textTall + lineGap - dividerTop;
    FillRGBABlend(INT_XPOS(12.5), dividerTop, 1, dividerTall,
        r, g, b, 255);
    DrawUtils::DrawHudString(INT_XPOS(12.5) + 10, firstLineY,
        ScreenWidth, label.m_szMap, r, g, b);

    if (!m_bBombPlanted)
        DrawUtils::DrawHudString(INT_XPOS(12.5) + 10, secondLineY,
            ScreenWidth, label.m_szTimer, r, g, b);

    // счёт команд
    int len = DrawUtils::HudStringLen("Counter-Terrorists:");
    DrawUtils::DrawHudString(INT_XPOS(12.5) - len - 50, firstLineY,
        INT_XPOS(12.5) - 50, "Counter-Terrorists:", r, g, b);
    DrawUtils::DrawHudString(INT_XPOS(12.5) - len - 50, secondLineY,
        INT_XPOS(12.5) - 50, "Terrorists:", r, g, b);
    DrawUtils::DrawHudNumberString(INT_XPOS(12.5) - 10, firstLineY,
        INT_XPOS(12.5) - 50, label.m_iCounterTerrorists, r, g, b);
    DrawUtils::DrawHudNumberString(INT_XPOS(12.5) - 10, secondLineY,
        INT_XPOS(12.5) - 50, label.m_iTerrorists, r, g, b);

    // имя/хп наблюдаемого
    int cr, cg, cb;
    GetTeamColor(cr, cg, cb, g_PlayerExtraInfo[g_iUser2].teamnumber);
    int nameLen = 0, nameTall = textTall;
    DrawUtils::ConsoleStringSize(label.m_szNameAndHealth, &nameLen, &nameTall);
    const int nameY = ScreenHeight - bottomBarTall +
        (bottomBarTall - nameTall) / 2;
    DrawUtils::DrawHudString(ScreenWidth * 0.5f - nameLen * 0.5f, nameY,
        ScreenWidth, label.m_szNameAndHealth, cr, cg, cb);
    return 1;
}

void CHudSpectatorGui::CalcAllNeededData()
{
    // карта
    if (!label.m_szMap[0]) {
        static char stripped[55];
        const char* lvl = gEngfuncs.pfnGetLevelName(); // "maps/%s.bsp"
        strncpy(stripped, lvl + 5, sizeof(stripped));
        stripped[sizeof(stripped) - 1] = 0;
        size_t L = strlen(stripped);
        if (L >= 4) stripped[L - 4] = 0;
        snprintf(label.m_szMap, sizeof(label.m_szMap), "Map: %s", stripped);
    }

    // счёт берём из g_TeamInfo (как у тебя)
    label.m_iCounterTerrorists = 0;
    label.m_iTerrorists = 0;
    for (int i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; ++i) {
        switch (g_TeamInfo[i].teamnumber) {
        case TEAM_CT:        label.m_iCounterTerrorists = g_TeamInfo[i].frags; break;
        case TEAM_TERRORIST: label.m_iTerrorists = g_TeamInfo[i].frags; break;
        }
    }

    // таймер
    if (!m_bBombPlanted) {
        const int remain = max(0, (int)(gHUD.m_Timer.m_iTime + gHUD.m_Timer.m_fStartTime - gHUD.m_flTime));
        const int mm = remain / 60, ss = remain % 60;
        snprintf(label.m_szTimer, sizeof(label.m_szTimer), "%d:%02d", mm, ss);
    }

    // текущий игрок
    if (g_iUser2 > 0 && g_iUser2 < MAX_PLAYERS) {
        hud_player_info_t info; GetPlayerInfo(g_iUser2, &info);
        snprintf(label.m_szNameAndHealth, sizeof(label.m_szNameAndHealth), "%s (%d)",
            info.name ? info.name : "", g_PlayerExtraInfo[g_iUser2].health);
    }
    else {
        label.m_szNameAndHealth[0] = 0;
    }
}

int CHudSpectatorGui::MsgFunc_SpecHealth(const char* name, int size, void* buf)
{
    BufferReader r(name, buf, size);
    g_PlayerExtraInfo[g_iUser2].health = r.ReadByte();
    m_iPlayerLastPointedAt = g_iUser2;
    return 1;
}

int CHudSpectatorGui::MsgFunc_SpecHealth2(const char* name, int size, void* buf)
{
    BufferReader r(name, buf, size);
    int hp = r.ReadByte();
    int cl = r.ReadByte();
    g_PlayerExtraInfo[cl].health = hp;
    m_iPlayerLastPointedAt = g_iUser2;
    return 1;
}

void CHudSpectatorGui::InitHUDData()
{
    m_bBombPlanted = false;
    m_menuFlags = ROOT_MENU;
    label.m_szMap[0] = '\0';
}

void CHudSpectatorGui::Reset()
{
    m_bBombPlanted = false;
    m_menuFlags = ROOT_MENU; // drop any submenu, panels come back visible
}
