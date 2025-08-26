// spectator_gui.cpp (GoldSrc-only, без Xash/Mobile)

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "draw_util.h"

#define XPOS(x)      ((x) / 16.0f)
#define YPOS(y)      ((y) / 10.0f)
#define INT_XPOS(x)  int(XPOS(x) * ScreenWidth)
#define INT_YPOS(y)  int(YPOS(y) * ScreenHeight)

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

    // верх/низ тёмные полосы
    FillRGBABlend(0, 0, ScreenWidth, INT_YPOS(2), 0, 0, 0, 153);
    FillRGBABlend(0, ScreenHeight - INT_YPOS(2), ScreenWidth, INT_YPOS(2), 0, 0, 0, 153);

    int r = 255, g = 140, b = 0;

    // разделитель и подписи справа
    FillRGBABlend(INT_XPOS(12.5), INT_YPOS(0.5), 1, INT_YPOS(1.0), r, g, b, 255);
    DrawUtils::DrawHudString(INT_XPOS(12.5) + 10, INT_YPOS(0.25), ScreenWidth, label.m_szMap, r, g, b);

    if (!m_bBombPlanted)
        DrawUtils::DrawHudString(INT_XPOS(12.5) + 10, INT_YPOS(0.5), ScreenWidth, label.m_szTimer, r, g, b);

    // счёт команд
    int len = DrawUtils::HudStringLen("Counter-Terrorists:");
    DrawUtils::DrawHudString(INT_XPOS(12.5) - len - 50, INT_YPOS(0.25), INT_XPOS(12.5) - 50, "Counter-Terrorists:", r, g, b);
    DrawUtils::DrawHudString(INT_XPOS(12.5) - len - 50, INT_YPOS(0.5), INT_XPOS(12.5) - 50, "Terrorists:", r, g, b);
    DrawUtils::DrawHudNumberString(INT_XPOS(12.5) - 10, INT_YPOS(0.25), INT_XPOS(12.5) - 50, label.m_iCounterTerrorists, r, g, b);
    DrawUtils::DrawHudNumberString(INT_XPOS(12.5) - 10, INT_YPOS(0.5), INT_XPOS(12.5) - 50, label.m_iTerrorists, r, g, b);

    // имя/хп наблюдаемого
    int cr, cg, cb;
    GetTeamColor(cr, cg, cb, g_PlayerExtraInfo[g_iUser2].teamnumber);
    int nameLen = DrawUtils::HudStringLen(label.m_szNameAndHealth);
    DrawUtils::DrawHudString(ScreenWidth * 0.5f - nameLen * 0.5f, INT_YPOS(9) - gHUD.GetCharHeight() * 0.5f,
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