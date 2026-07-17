// spectator_gui.cpp (GoldSrc-only, без Xash/Mobile)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "draw_util.h"

#define XPOS(x)      ((x) / 16.0f)
#define INT_XPOS(x)  int(XPOS(x) * ScreenWidth)

DECLARE_MESSAGE(m_SpectatorGui, SpecHealth)
DECLARE_MESSAGE(m_SpectatorGui, SpecHealth2)

int CHudSpectatorGui::Init()
{
    // только сообщения; никаких команд и мобильных кнопок
    HOOK_MESSAGE(SpecHealth);
    HOOK_MESSAGE(SpecHealth2);

    gHUD.AddHudElem(this);
    m_iFlags = HUD_DRAW;
    m_bBombPlanted = false;
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

// spectator_gui.cpp
void CHudSpectatorGui::UserCmd_ToggleSpectatorMenu() {}

int CHudSpectatorGui::Draw(float flTime)
{
    if (!g_iUser1)  // не в режиме спектатора
        return 1;

    CalcAllNeededData();

    // Velaron/Xash uses a logical scaled HUD, where INT_YPOS(2) and the font
    // grow together. Steam GoldSrc reports physical resolution but keeps its
    // console font nearly pixel-sized; at 1080p the old formula produced two
    // mostly empty 216 px bars around 13 px text. Size the bars from the real
    // font metrics instead.
    const int textTall = max(gHUD.GetCharHeight(), 13);
    const int lineGap = max(textTall / 3, 3);
    const int topPadding = max(textTall, 10);
    const int topBarTall = max(64,
        topPadding * 2 + textTall * 2 + lineGap);
    const int bottomBarTall = max(48, textTall * 3);
    const int firstLineY = topPadding;
    const int secondLineY = firstLineY + textTall + lineGap;

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
    label.m_szMap[0] = '\0';
}

void CHudSpectatorGui::Reset()
{
    m_bBombPlanted = false;
    if (m_menuFlags & ROOT_MENU)
    {
        UserCmd_ToggleSpectatorMenu(); // this will remove any submenus;
        m_menuFlags = 0;
    }
}
