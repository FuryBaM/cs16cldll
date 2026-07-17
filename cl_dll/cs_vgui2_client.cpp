#include "cs_vgui2.h"
#include "cs_vgui2_client.h"
#include "cs_vgui.h"

#include <vgui/IClientPanel.h>
#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <tier1/KeyValues.h>

#include <math.h>
#include <string.h>

extern "C" void CS16VGUI_ClientCommand(const char* command);
extern "C" void CS16VGUI_SetMouseVisible(int visible);
extern "C" void CS16VGUI_Print(const char* text);
extern "C" void CS16VGUI_Trace(const char* stage);

namespace
{
enum
{
    CS_MENU_TEAM = 2,
    CS_MENU_CLASS_T = 26,
    CS_MENU_CLASS_CT = 27,
    CS_MENU_BUY = 28,
    CS_MENU_BUY_PISTOL = 29,
    CS_MENU_BUY_SHOTGUN = 30,
    CS_MENU_BUY_RIFLE = 31,
    CS_MENU_BUY_SMG = 32,
    CS_MENU_BUY_MACHINEGUN = 33,
    CS_MENU_BUY_EQUIPMENT = 34,
    CS_TEAM_T = 1,
    CS_TEAM_CT = 2,
    MAX_MENU_ENTRIES = 16,
    MAX_DECORATIONS = 8,
    MAX_RESOURCE_LABELS = 8,
    // HudText effects are emitted character-by-character. Several active
    // server messages plus ShowMenu can exceed 128 entries in one frame.
    MAX_HUD_TEXT_DRAWS = 2048
};

struct HudTextDraw
{
    int x, y;
    int r, g, b, a;
    char text[512];
};

class CCS16VGUI2Viewport;

class CCS16HudTextPanel : public vgui2::IClientPanel
{
public:
    explicit CCS16HudTextPanel(CCS16VGUI2Viewport* owner)
        : m_owner(owner), m_vpanel(0), m_panel(NULL), m_surface(NULL) {}

    void Attach(vgui2::VPANEL vpanel, vgui2::IPanel* panel,
        vgui2::ISurface* surface)
    {
        m_vpanel = vpanel; m_panel = panel; m_surface = surface;
    }
    vgui2::VPANEL GetVPanel() override { return m_vpanel; }
    void Think() override {}
    void PerformApplySchemeSettings() override {}
    void PaintTraverse(bool, bool) override;
    void Repaint() override
    {
        // Called by IPanel::Repaint. Calling it back from here recurses until
        // stack overflow, so only invalidate the surface in this callback.
        if (m_surface && m_vpanel) m_surface->Invalidate(m_vpanel);
    }
    vgui2::VPANEL IsWithinTraverse(int, int, bool) override { return 0; }
    void GetInset(int& top, int& left, int& right, int& bottom) override
    { top = left = right = bottom = 0; }
    void GetClipRect(int& x0, int& y0, int& x1, int& y1) override
    {
        if (m_panel && m_vpanel) m_panel->GetClipRect(m_vpanel, x0, y0, x1, y1);
        else x0 = y0 = x1 = y1 = 0;
    }
    void OnChildAdded(vgui2::VPANEL) override {}
    void OnSizeChanged(int, int) override {}
    void InternalFocusChanged(bool) override {}
    bool RequestInfo(KeyValues*) override { return false; }
    void RequestFocus(int) override {}
    bool RequestFocusPrev(vgui2::VPANEL) override { return false; }
    bool RequestFocusNext(vgui2::VPANEL) override { return false; }
    void OnMessage(const KeyValues*, vgui2::VPANEL) override {}
    vgui2::VPANEL GetCurrentKeyFocus() override { return 0; }
    int GetTabPosition() override { return 0; }
    const char* GetName() override { return "CS16HudTextPanel"; }
    const char* GetClassName() override { return "CS16HudTextPanel"; }
    vgui2::HScheme GetScheme() override { return 0; }
    bool IsProportional() override { return false; }
    bool IsAutoDeleteSet() override { return false; }
    void DeletePanel() override {}
    void* QueryInterface(vgui2::EInterfaceID id) override
    { return id == vgui2::ICLIENTPANEL_STANDARD_INTERFACE ? this : NULL; }
    vgui2::Panel* GetPanel() override { return NULL; }
    const char* GetModuleName() override { return "CS16CLIENT"; }

private:
    CCS16VGUI2Viewport* m_owner;
    vgui2::VPANEL m_vpanel;
    vgui2::IPanel* m_panel;
    vgui2::ISurface* m_surface;
};

struct MenuEntry
{
    char label[128];
    char fieldName[64];
    char previewPath[128];
    const char* command;
    int targetMenu;
    int key;
    int x, y, wide, tall;
    bool visible, enabled;
};

struct Decoration
{
    int x, y, wide, tall;
};

struct ResourceLabel
{
    char text[128];
    int x, y, wide, tall;
    bool centered;
};

struct BuyEntry
{
    const char* label;
    const char* command;
};

const BuyEntry g_pistolsT[] =
{
    { "1  GLOCK 18", "glock\n" }, { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" }, { "4  DESERT EAGLE", "deagle\n" },
    { "5  DUAL ELITES", "elites\n" }
};
const BuyEntry g_pistolsCT[] =
{
    { "1  GLOCK 18", "glock\n" }, { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" }, { "4  DESERT EAGLE", "deagle\n" },
    { "5  FIVE-SEVEN", "fiveseven\n" }
};
const BuyEntry g_shotguns[] =
{
    { "1  M3 SUPER 90", "m3\n" }, { "2  XM1014", "xm1014\n" }
};
const BuyEntry g_smgsT[] =
{
    { "1  MAC-10", "mac10\n" }, { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" }, { "4  P90", "p90\n" }
};
const BuyEntry g_smgsCT[] =
{
    { "1  TMP", "tmp\n" }, { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" }, { "4  P90", "p90\n" }
};
const BuyEntry g_riflesT[] =
{
    { "1  GALIL", "galil\n" }, { "2  AK-47", "ak47\n" },
    { "3  SCOUT", "scout\n" }, { "4  SG-552", "sg552\n" },
    { "5  AWP", "awp\n" }, { "6  G3/SG-1", "g3sg1\n" }
};
const BuyEntry g_riflesCT[] =
{
    { "1  FAMAS", "famas\n" }, { "2  SCOUT", "scout\n" },
    { "3  M4A1", "m4a1\n" }, { "4  AUG", "aug\n" },
    { "5  SG-550", "sg550\n" }, { "6  AWP", "awp\n" }
};
const BuyEntry g_machineGuns[] = { { "1  M249", "m249\n" } };
const BuyEntry g_equipmentT[] =
{
    { "1  KEVLAR", "vest\n" }, { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" }, { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" }, { "6  NIGHTVISION", "nvgs\n" }
};
const BuyEntry g_equipmentCT[] =
{
    { "1  KEVLAR", "vest\n" }, { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" }, { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" }, { "6  DEFUSE KIT", "defuser\n" },
    { "7  NIGHTVISION", "nvgs\n" }, { "8  TACTICAL SHIELD", "shield\n" }
};

class CCS16VGUI2Viewport : public vgui2::IClientPanel
{
public:
    CCS16VGUI2Viewport()
        : m_ivgui(NULL), m_panel(NULL), m_surface(NULL), m_input(NULL),
          m_vpanel(0), m_hudVPanel(0), m_hudPanelClient(this),
          m_visible(false), m_legacyCursorVisible(false),
          m_currentMenu(0), m_team(CS_TEAM_T),
          m_entryCount(0), m_hoveredEntry(-1), m_selectedEntry(-1),
          m_hudTextDrawCount(0),
          m_hudFont(vgui2::INVALID_FONT), m_font(vgui2::INVALID_FONT),
          m_titleFont(vgui2::INVALID_FONT),
          m_infoFont(vgui2::INVALID_FONT),
          m_previewTexture(0), m_loadedPreviewEntry(-1),
          m_logoTexture(0), m_logoLoaded(false)
    {
        m_title[0] = '\0';
        ResetLayout();
    }

    bool Initialize(CreateInterfaceFn* factories, int count)
    {
        if (!factories || count <= 0)
            return false;

        // The engine factory is deliberately first. Its Surface026 instance
        // is wired to the active game window; querying vgui2.dll directly
        // returns the same implementation before IVGui::Init has supplied its
        // IPanel dependency, which crashes in Surface::PushMakeCurrent.
        m_ivgui = Interface<vgui2::IVGui>(factories, count, "VGUI_ivgui006");
        if (!m_ivgui || !m_ivgui->Init(factories, count))
            return false;

        m_panel = Interface<vgui2::IPanel>(factories, count, "VGUI_Panel007");
        m_surface = Interface<vgui2::ISurface>(factories, count, "VGUI_Surface026");
        m_input = Interface<vgui2::IInput>(factories, count, "VGUI_Input004");
        if (!m_ivgui || !m_panel || !m_surface || !m_input)
            return false;

        m_vpanel = m_ivgui->AllocPanel();
        if (!m_vpanel)
            return false;

        m_panel->Init(m_vpanel, this);
        // A popup is registered in ISurface's popup list. GoldSrc derives
        // cursor visibility from that list every frame; a merely visible root
        // panel is painted but its mouse cursor is forced back to dc_none.
        m_surface->CreatePopup(m_vpanel, false, false, false, false, false);
        m_panel->SetKeyBoardInputEnabled(m_vpanel, false);
        m_panel->SetMouseInputEnabled(m_vpanel, false);
        // HUD text is drawn synchronously from HUD_Redraw and does not need a
        // visible popup. Keep this hidden until an actual client menu opens,
        // otherwise it interferes with GameUI's Escape-menu cursor.
        m_panel->SetVisible(m_vpanel, false);

        // Unicode HUD text has its own ordinary child panel. It is not a
        // popup and never accepts input, so it cannot affect GameUI's cursor.
        m_hudVPanel = m_ivgui->AllocPanel();
        if (!m_hudVPanel)
        {
            m_ivgui->FreePanel(m_vpanel);
            m_vpanel = 0;
            return false;
        }
        m_hudPanelClient.Attach(m_hudVPanel, m_panel, m_surface);
        m_panel->Init(m_hudVPanel, &m_hudPanelClient);
        m_panel->SetKeyBoardInputEnabled(m_hudVPanel, false);
        m_panel->SetMouseInputEnabled(m_hudVPanel, false);
        m_panel->SetVisible(m_hudVPanel, true);
        ResizeToScreen();
        return true;
    }

    void Shutdown()
    {
        HideMenu();
        if (m_hudVPanel && m_ivgui)
            m_ivgui->FreePanel(m_hudVPanel);
        if (m_vpanel && m_ivgui)
            m_ivgui->FreePanel(m_vpanel);

        m_hudVPanel = 0;
        m_vpanel = 0;
        m_ivgui = NULL;
        m_panel = NULL;
        m_surface = NULL;
        m_input = NULL;
        m_hudFont = vgui2::INVALID_FONT;
        m_font = vgui2::INVALID_FONT;
        m_titleFont = vgui2::INVALID_FONT;
        m_infoFont = vgui2::INVALID_FONT;
        m_hudTextDrawCount = 0;
        m_previewTexture = 0;
        m_loadedPreviewEntry = -1;
        m_logoTexture = 0;
        m_logoLoaded = false;
    }

    bool IsReady() const { return m_vpanel != 0; }

    void SetParent(vgui2::VPANEL parent)
    {
        if (m_vpanel && m_panel)
        {
            m_panel->SetParent(m_vpanel, parent);
            m_panel->SetVisible(m_vpanel, m_visible || m_legacyCursorVisible);
            if (m_hudVPanel)
            {
                m_panel->SetParent(m_hudVPanel, parent);
                m_panel->SetVisible(m_hudVPanel, true);
            }
        }
    }

    void ResizeToScreen()
    {
        if (!m_vpanel || !m_panel || !m_surface)
            return;

        int wide = 0;
        int tall = 0;
        m_surface->GetScreenSize(wide, tall);
        m_panel->SetPos(m_vpanel, 0, 0);
        m_panel->SetSize(m_vpanel, wide, tall);
        if (m_hudVPanel)
        {
            m_panel->SetPos(m_hudVPanel, 0, 0);
            m_panel->SetSize(m_hudVPanel, wide, tall);
        }
    }

    void SetTeam(int team)
    {
        m_team = team == CS_TEAM_CT ? CS_TEAM_CT : CS_TEAM_T;
    }

    void BeginHudTextFrame()
    {
        // Keep the completed list after painting. GoldSrc may traverse VGUI
        // before or after HUD_Redraw, so the preceding complete frame remains
        // available until the next HUD frame starts here.
        m_hudTextDrawCount = 0;
        if (m_hudVPanel && m_panel)
        {
            // IPanel::Repaint is what schedules IClientPanel::PaintTraverse.
            // ISurface::Invalidate alone is not reliable for an ordinary
            // non-popup child on Steam GoldSrc.
            m_panel->Repaint(m_hudVPanel);
            if (m_surface)
                m_surface->Invalidate(m_hudVPanel);
        }
    }

    int DrawHudString(int x, int y, const char* text,
        int r, int g, int b, int a)
    {
        if (!m_surface || !m_vpanel || !text)
            return -1;

        EnsureFonts();
        if (m_hudFont == vgui2::INVALID_FONT)
            return -1;

        wchar_t wideText[1024];
        const int length = ConvertText(text, wideText, ARRAYSIZE(wideText));
        int textWide = 0, textTall = 0;
        m_surface->GetTextSize(m_hudFont, wideText, textWide, textTall);

        // GoldSrc HUD text is additive: lowering RGB fades a glyph toward
        // invisibility. Convert that intensity into alpha for VGUI so dark
        // fade stages do not become opaque black text.
        r = max(0, min(r, 255));
        g = max(0, min(g, 255));
        b = max(0, min(b, 255));
        a = max(0, min(a, 255));
        const int intensity = max(r, max(g, b));
        if (intensity <= 0)
        {
            r = g = b = 255;
            a = 0;
        }
        else if (intensity < 255)
        {
            r = r * 255 / intensity;
            g = g * 255 / intensity;
            b = b * 255 / intensity;
            a = a * intensity / 255;
        }

        if (m_hudTextDrawCount < MAX_HUD_TEXT_DRAWS)
        {
            HudTextDraw& draw = m_hudTextDraws[m_hudTextDrawCount++];
            draw.x = x; draw.y = y;
            draw.r = r; draw.g = g; draw.b = b; draw.a = a;
            strncpy(draw.text, text, sizeof(draw.text));
            draw.text[sizeof(draw.text) - 1] = '\0';
            if (m_hudTextDrawCount == 1 && m_hudVPanel)
            {
                m_panel->Repaint(m_hudVPanel);
                m_surface->Invalidate(m_hudVPanel);
            }
        }
        (void)length;
        return textWide;
    }

    bool GetHudStringSize(const char* text, int& wide, int& tall)
    {
        wide = tall = 0;
        if (!m_surface || !text)
            return false;

        EnsureFonts();
        if (m_hudFont == vgui2::INVALID_FONT)
            return false;

        wchar_t wideText[1024];
        ConvertText(text, wideText, ARRAYSIZE(wideText));
        m_surface->GetTextSize(m_hudFont, wideText, wide, tall);
        return true;
    }

    void PaintHudText()
    {
        if (!m_surface || !m_hudVPanel || m_hudTextDrawCount == 0)
            return;

        EnsureFonts();
        if (m_hudFont == vgui2::INVALID_FONT)
            return;

        m_surface->PushMakeCurrent(m_hudVPanel, false);
        for (int i = 0; i < m_hudTextDrawCount; ++i)
        {
            const HudTextDraw& draw = m_hudTextDraws[i];
            wchar_t wide[512];
            const int length = ConvertText(draw.text, wide, ARRAYSIZE(wide));
            m_surface->DrawSetTextFont(m_hudFont);
            m_surface->DrawSetTextColor(draw.r, draw.g, draw.b, draw.a);
            m_surface->DrawSetTextPos(draw.x, draw.y);
            m_surface->DrawPrintText(wide, length);
        }
        m_surface->DrawFlushText();
        m_surface->PopMakeCurrent(m_hudVPanel);
    }

    bool ShowMenu(int menuId)
    {
        const bool wasVisible = m_visible;
        ClearMenu(menuId);
        switch (menuId)
        {
        case CS_MENU_TEAM:
            SetTitle("SELECT A TEAM");
            Add("1  TERRORIST FORCES", "jointeam 1\n", 0, 1);
            Add("2  CT FORCES", "jointeam 2\n", 0, 2);
            Add("3  VIP", "jointeam 3\n", 0, 3);
            Add("5  AUTO-SELECT", "jointeam 5\n", 0, 5);
            Add("6  SPECTATE", "jointeam 6\n", 0, 6);
            break;
        case CS_MENU_CLASS_T:
            SetTitle("CHOOSE A TERRORIST CLASS");
            AddClasses();
            break;
        case CS_MENU_CLASS_CT:
            SetTitle("CHOOSE A COUNTER-TERRORIST CLASS");
            AddClasses();
            break;
        case CS_MENU_BUY:
            SetTitle("BUY MENU");
            Add("1  PISTOLS", NULL, CS_MENU_BUY_PISTOL, 1);
            Add("2  SHOTGUNS", NULL, CS_MENU_BUY_SHOTGUN, 2);
            Add("3  SMG", NULL, CS_MENU_BUY_SMG, 3);
            Add("4  RIFLES", NULL, CS_MENU_BUY_RIFLE, 4);
            Add("5  MACHINE GUNS", NULL, CS_MENU_BUY_MACHINEGUN, 5);
            Add("6  PRIMARY AMMO", "primammo\n", 0, 6);
            Add("7  SECONDARY AMMO", "secammo\n", 0, 7);
            Add("8  EQUIPMENT", NULL, CS_MENU_BUY_EQUIPMENT, 8);
            // MainBuyMenu.res places Cancel before the two right-column
            // buttons, so keep the logical order identical to the resource.
            Add("0  CANCEL", NULL, 0, 0);
            Add("A  AUTO-BUY", "autobuy\n", 0, 'A');
            Add("R  RE-BUY PREVIOUS", "rebuy\n", 0, 'R');
            break;
        case CS_MENU_BUY_PISTOL:
            SetTitle("BUY PISTOLS");
            AddBuyList(m_team == CS_TEAM_CT ? g_pistolsCT : g_pistolsT,
                m_team == CS_TEAM_CT ? Count(g_pistolsCT) : Count(g_pistolsT));
            break;
        case CS_MENU_BUY_SHOTGUN:
            SetTitle("BUY SHOTGUNS");
            AddBuyList(g_shotguns, Count(g_shotguns));
            break;
        case CS_MENU_BUY_RIFLE:
            SetTitle("BUY RIFLES");
            AddBuyList(m_team == CS_TEAM_CT ? g_riflesCT : g_riflesT,
                m_team == CS_TEAM_CT ? Count(g_riflesCT) : Count(g_riflesT));
            break;
        case CS_MENU_BUY_SMG:
            SetTitle("BUY SMG");
            AddBuyList(m_team == CS_TEAM_CT ? g_smgsCT : g_smgsT,
                m_team == CS_TEAM_CT ? Count(g_smgsCT) : Count(g_smgsT));
            break;
        case CS_MENU_BUY_MACHINEGUN:
            SetTitle("BUY MACHINE GUN");
            AddBuyList(g_machineGuns, Count(g_machineGuns));
            break;
        case CS_MENU_BUY_EQUIPMENT:
            SetTitle("BUY EQUIPMENT");
            AddBuyList(m_team == CS_TEAM_CT ? g_equipmentCT : g_equipmentT,
                m_team == CS_TEAM_CT ? Count(g_equipmentCT) : Count(g_equipmentT));
            break;
        default:
            m_currentMenu = 0;
            return false;
        }

        if (menuId != CS_MENU_BUY)
            Add("0  CANCEL", NULL, 0, 0);
        LoadMenuResource(menuId);
        ConfigureInformationPanel(menuId);
        ApplyMenuRules(menuId);
        if (!wasVisible)
        {
            ResizeToScreen();
            m_visible = true;
            m_legacyCursorVisible = false;
            m_panel->SetKeyBoardInputEnabled(m_vpanel, true);
            m_panel->SetMouseInputEnabled(m_vpanel, true);
            m_panel->SetVisible(m_vpanel, true);
            m_panel->MoveToFront(m_vpanel);
            m_input->SetMouseFocus(m_vpanel);
            m_surface->UnlockCursor();
            m_surface->CalculateMouseVisible();
            m_surface->SetCursor(vgui2::dc_arrow);
            CS16VGUI_SetMouseVisible(1);
        }
        UpdateHoveredEntry();
        Repaint();
        return true;
    }

    void HideMenu()
    {
        m_currentMenu = 0;
        m_visible = false;
        m_hoveredEntry = -1;
        m_selectedEntry = -1;
        if (m_vpanel && m_panel)
        {
            m_panel->SetKeyBoardInputEnabled(m_vpanel, false);
            m_panel->SetMouseInputEnabled(m_vpanel, false);
            m_panel->SetVisible(m_vpanel, m_legacyCursorVisible);
            if (!m_legacyCursorVisible)
                m_surface->CalculateMouseVisible();
        }
        CS16VGUI_SetMouseVisible(0);
    }

    void SetLegacyCursorVisible(bool visible)
    {
        m_legacyCursorVisible = visible && !m_visible;
        if (!m_vpanel || !m_panel || !m_surface)
            return;

        if (m_visible || m_legacyCursorVisible)
        {
            m_panel->SetVisible(m_vpanel, true);
            m_surface->UnlockCursor();
            m_surface->CalculateMouseVisible();
            m_surface->SetCursor(vgui2::dc_arrow);
        }
        else
        {
            m_panel->SetVisible(m_vpanel, false);
            // Cursor state is shared by every VGUI popup, including GameUI's
            // Escape/pause menu.  Recalculate it instead of globally forcing
            // dc_none, otherwise the pause menu is left without a cursor.
            m_surface->CalculateMouseVisible();
        }
    }

    bool KeyInput(int keynum)
    {
        if (!m_visible)
            return false;
        if (keynum == 27)
        {
            HideMenu();
            return true;
        }
        if (keynum >= 'a' && keynum <= 'z')
            keynum -= 'a' - 'A';

        // Stock buy submenus do not display Back in their .res, but GoldSrc
        // still accepts slot 9 as the keyboard shortcut to the root buy menu.
        if (keynum == '9' && m_currentMenu >= CS_MENU_BUY_PISTOL &&
            m_currentMenu <= CS_MENU_BUY_EQUIPMENT)
        {
            ShowMenu(CS_MENU_BUY);
            return true;
        }

        for (int i = 0; i < m_entryCount; ++i)
        {
            if (!m_entries[i].visible || !m_entries[i].enabled)
                continue;
            const int entryKey = m_entries[i].key;
            if ((entryKey >= 0 && entryKey <= 9 && keynum == '0' + entryKey) ||
                keynum == entryKey)
            {
                Execute(i);
                return true;
            }
        }
        return false;
    }

    vgui2::VPANEL GetVPanel() override { return m_vpanel; }

    void Think() override
    {
        if ((!m_visible && !m_legacyCursorVisible) || !m_surface)
            return;

        // GoldSrc may restore its gameplay cursor lock after the client menu
        // opens. Keep the platform cursor released while this popup is active.
        m_surface->UnlockCursor();
        m_surface->SetCursor(vgui2::dc_arrow);
        if (m_visible && m_input)
            UpdateHoveredEntry();
    }

    void PerformApplySchemeSettings() override {}

    void PaintTraverse(bool, bool) override
    {
        if (!m_visible || !m_surface || !m_vpanel)
            return;

        m_surface->PushMakeCurrent(m_vpanel, false);
        EnsureFonts();

        int left = 0, top = 0;
        GetMenuOrigin(left, top);
        DrawViewportBackground();
        m_surface->DrawSetColor(255, 140, 0, 225);
        for (int i = 0; i < m_decorationCount; ++i)
        {
            const Decoration& decoration = m_decorations[i];
            m_surface->DrawFilledRect(ScaleX(left + decoration.x),
                ScaleY(top + decoration.y),
                ScaleX(left + decoration.x + decoration.wide),
                ScaleY(top + decoration.y + decoration.tall));
        }

        const int titleY = ScaleY(top + m_titleY) +
            (ScaleY(m_titleTall) - m_surface->GetFontTall(m_titleFont)) / 2;
        DrawText(ScaleX(left + m_titleX), titleY, m_title,
            m_titleFont, 255, 140, 0, 255);
        for (int i = 0; i < m_resourceLabelCount; ++i)
            DrawResourceLabel(left, top, m_resourceLabels[i]);
        for (int i = 0; i < m_entryCount; ++i)
        {
            const MenuEntry& entry = m_entries[i];
            if (!entry.visible)
                continue;
            const int x0 = ScaleX(left + entry.x);
            const int y0 = ScaleY(top + entry.y);
            const int x1 = ScaleX(left + entry.x + entry.wide);
            const int y1 = ScaleY(top + entry.y + entry.tall);
            if (i == m_hoveredEntry)
            {
                m_surface->DrawSetColor(120, 65, 0, 165);
                m_surface->DrawFilledRect(x0, y0, x1, y1);
                DrawText(ScaleX(left + entry.x + 6),
                    y0 + (y1 - y0 - m_surface->GetFontTall(m_font)) / 2,
                    entry.label, m_font, 255, 220, 120, 255);
            }
            else if (i == m_selectedEntry)
            {
                m_surface->DrawSetColor(120, 65, 0, 115);
                m_surface->DrawFilledRect(x0, y0, x1, y1);
                DrawText(ScaleX(left + entry.x + 6),
                    y0 + (y1 - y0 - m_surface->GetFontTall(m_font)) / 2,
                    entry.label, m_font, 255, 190, 70, 255);
            }
            else
            {
                DrawText(ScaleX(left + entry.x + 6),
                    y0 + (y1 - y0 - m_surface->GetFontTall(m_font)) / 2,
                    entry.label, m_font, entry.enabled ? 255 : 140,
                    entry.enabled ? 165 : 100, entry.enabled ? 35 : 30, 255);
            }
            m_surface->DrawSetColor(188, 112, 0,
                (i == m_hoveredEntry || i == m_selectedEntry) ? 230 : 128);
            m_surface->DrawOutlinedRect(x0, y0, x1, y1);
        }
        DrawInformationPanel(left, top);
        m_surface->PopMakeCurrent(m_vpanel);
    }

    void Repaint() override
    {
        if (m_surface && m_vpanel)
            m_surface->Invalidate(m_vpanel);
    }

    vgui2::VPANEL IsWithinTraverse(int x, int y, bool) override
    {
        if (!m_visible || !m_panel || !m_vpanel)
            return 0;
        int wide = 0;
        int tall = 0;
        m_panel->GetSize(m_vpanel, wide, tall);
        return x >= 0 && y >= 0 && x < wide && y < tall ? m_vpanel : 0;
    }

    void GetInset(int& top, int& left, int& right, int& bottom) override
    {
        top = left = right = bottom = 0;
    }

    void GetClipRect(int& x0, int& y0, int& x1, int& y1) override
    {
        if (m_panel && m_vpanel)
            m_panel->GetClipRect(m_vpanel, x0, y0, x1, y1);
        else
            x0 = y0 = x1 = y1 = 0;
    }

    void OnChildAdded(vgui2::VPANEL) override {}
    void OnSizeChanged(int, int) override {}
    void InternalFocusChanged(bool) override {}
    bool RequestInfo(KeyValues*) override { return false; }
    void RequestFocus(int) override { if (m_input && m_vpanel) m_input->SetMouseFocus(m_vpanel); }
    bool RequestFocusPrev(vgui2::VPANEL) override { return false; }
    bool RequestFocusNext(vgui2::VPANEL) override { return false; }
    void OnMessage(const KeyValues* params, vgui2::VPANEL) override
    {
        if (!m_visible || !params)
            return;

        const char* name = params->GetName();
        if (name && (!_stricmp(name, "MousePressed") ||
            !_stricmp(name, "MouseDoublePressed")) &&
            const_cast<KeyValues*>(params)->GetInt("code", vgui2::MOUSE_LEFT) == vgui2::MOUSE_LEFT)
        {
            ExecuteEntryUnderCursor();
        }
        else if (name && !_stricmp(name, "KeyCodePressed"))
        {
            const int code = const_cast<KeyValues*>(params)->GetInt("code", vgui2::KEY_NONE);
            if (code >= vgui2::KEY_0 && code <= vgui2::KEY_9)
                KeyInput('0' + code - vgui2::KEY_0);
            else if (code >= vgui2::KEY_PAD_0 && code <= vgui2::KEY_PAD_9)
                KeyInput('0' + code - vgui2::KEY_PAD_0);
            else if (code >= vgui2::KEY_A && code <= vgui2::KEY_Z)
                KeyInput('A' + code - vgui2::KEY_A);
            else if (code == vgui2::KEY_ESCAPE)
                KeyInput(27);
        }
    }
    vgui2::VPANEL GetCurrentKeyFocus() override { return m_visible ? m_vpanel : 0; }
    int GetTabPosition() override { return 0; }
    const char* GetName() override { return "CS16VGUI2Viewport"; }
    const char* GetClassName() override { return "CS16VGUI2Viewport"; }
    vgui2::HScheme GetScheme() override { return 0; }
    bool IsProportional() override { return false; }
    bool IsAutoDeleteSet() override { return false; }
    void DeletePanel() override { HideMenu(); }
    void* QueryInterface(vgui2::EInterfaceID id) override
    {
        return id == vgui2::ICLIENTPANEL_STANDARD_INTERFACE ? this : NULL;
    }
    vgui2::Panel* GetPanel() override { return NULL; }
    const char* GetModuleName() override { return "CS16CLIENT"; }

private:
    template <typename T>
    static T* Interface(CreateInterfaceFn* factories, int count, const char* name)
    {
        for (int i = 0; i < count; ++i)
        {
            if (!factories[i])
                continue;
            void* instance = factories[i](name, NULL);
            if (instance)
                return static_cast<T*>(instance);
        }
        return NULL;
    }

    template <typename T, int N>
    static int Count(const T (&)[N]) { return N; }

    int ScaleX(int value) const
    {
        int wide = 640, tall = 480;
        if (m_surface) m_surface->GetScreenSize(wide, tall);
        return value * wide / 640;
    }

    int ScaleY(int value) const
    {
        int wide = 640, tall = 480;
        if (m_surface) m_surface->GetScreenSize(wide, tall);
        return value * tall / 480;
    }

    void DrawRoundedFilledRect(int x0, int y0, int x1, int y1,
        int radiusX, int radiusY, int r, int g, int b, int a)
    {
        if (!m_surface || x1 <= x0 || y1 <= y0)
            return;

        radiusX = min(radiusX, (x1 - x0) / 2);
        radiusY = min(radiusY, (y1 - y0) / 2);
        if (radiusX <= 0 || radiusY <= 0)
        {
            m_surface->DrawSetColor(r, g, b, a);
            m_surface->DrawFilledRect(x0, y0, x1, y1);
            return;
        }

        m_surface->DrawSetColor(r, g, b, a);

        // The middle and the rounded rows never overlap. This is important
        // for translucent colors: overlapping rectangles made the old corners
        // look like several dark square layers.
        m_surface->DrawFilledRect(x0, y0 + radiusY, x1, y1 - radiusY);
        for (int row = 0; row < radiusY; ++row)
        {
            const float dy = (radiusY - row - 0.5f) / (float)radiusY;
            const float arc = sqrtf(max(0.0f, 1.0f - dy * dy));
            const int inset = (int)(radiusX * (1.0f - arc) + 0.5f);

            m_surface->DrawFilledRect(x0 + inset, y0 + row,
                x1 - inset, y0 + row + 1);
            m_surface->DrawFilledRect(x0 + inset, y1 - row - 1,
                x1 - inset, y1 - row);
        }
    }

    void DrawViewportBackground()
    {
        const int x0 = ScaleX(20), y0 = ScaleY(20);
        const int x1 = ScaleX(620), y1 = ScaleY(460);
        const int radiusX = ScaleX(10), radiusY = ScaleY(10);
        DrawRoundedFilledRect(x0, y0, x1, y1,
            radiusX, radiusY, 0, 0, 0, 188);

        m_surface->DrawSetColor(180, 180, 180, 115);
        m_surface->DrawFilledRect(ScaleX(20), ScaleY(72),
            ScaleX(620), ScaleY(73));

        if (!m_logoLoaded)
        {
            if (!m_logoTexture)
                m_logoTexture = m_surface->CreateNewTextureID(false);
            int width = 0, height = 0;
            if (m_logoTexture && CS16VGUI_LoadTGA("gfx/vgui/CS_logo.tga",
                m_previewPixels, sizeof(m_previewPixels), &width, &height))
            {
                m_surface->DrawSetTextureRGBA(m_logoTexture, m_previewPixels,
                    width, height, 1, true);
                m_logoLoaded = true;
            }
        }
        if (m_logoLoaded)
        {
            m_surface->DrawSetColor(255, 174, 0, 255);
            m_surface->DrawSetTexture(m_logoTexture);
            m_surface->DrawTexturedRect(ScaleX(26), ScaleY(26),
                ScaleX(66), ScaleY(66));
        }

        char roundTime[32];
        if (CS16VGUI_GetRoundTime(roundTime, sizeof(roundTime)))
        {
            const int textWide = TextWidth(roundTime, m_titleFont);
            DrawText((ScaleX(640) - textWide) / 2, ScaleY(33), roundTime,
                m_titleFont, 188, 112, 0, 55);
        }
    }

    void EnsureFonts()
    {
        if (m_hudFont == vgui2::INVALID_FONT)
        {
            m_hudFont = m_surface->CreateFont();
            if (m_hudFont != vgui2::INVALID_FONT)
                m_surface->AddGlyphSetToFont(m_hudFont, "Verdana", 12, 500, 0, 0,
                    vgui2::ISurface::FONTFLAG_ANTIALIAS |
                    vgui2::ISurface::FONTFLAG_ADDITIVE, 0x0000, 0x04ff);
        }
        if (m_font == vgui2::INVALID_FONT)
        {
            m_font = m_surface->CreateFont();
            if (m_font != vgui2::INVALID_FONT)
                m_surface->AddGlyphSetToFont(m_font, "Verdana", 12, 500, 0, 0,
                    vgui2::ISurface::FONTFLAG_ANTIALIAS, 0x0000, 0x04ff);
        }
        if (m_titleFont == vgui2::INVALID_FONT)
        {
            m_titleFont = m_surface->CreateFont();
            if (m_titleFont != vgui2::INVALID_FONT)
                m_surface->AddGlyphSetToFont(m_titleFont, "Verdana Bold", 18, 500, 0, 0,
                vgui2::ISurface::FONTFLAG_ANTIALIAS, 0x0000, 0x04ff);
        }
        if (m_infoFont == vgui2::INVALID_FONT)
        {
            m_infoFont = m_surface->CreateFont();
            if (m_infoFont != vgui2::INVALID_FONT)
                m_surface->AddGlyphSetToFont(m_infoFont, "Verdana", 12, 500, 0, 0,
                    vgui2::ISurface::FONTFLAG_ANTIALIAS, 0x0000, 0x04ff);
        }
    }

    static int ConvertText(const char* text, wchar_t* wide, int capacity)
    {
        if (!wide || capacity <= 0)
            return 0;
        if (!text)
            text = "";

        int length = 0;
        while (text[length] && length < capacity - 1)
        {
            const unsigned char ch = (unsigned char)text[length];
            if (ch >= 0xc0)
                wide[length] = 0x0410 + ch - 0xc0;
            else if (ch == 0xa8)
                wide[length] = 0x0401;
            else if (ch == 0xb8)
                wide[length] = 0x0451;
            else
                wide[length] = ch;
            ++length;
        }
        wide[length] = 0;
        return length;
    }

    int TextWidth(const char* text, vgui2::HFont font) const
    {
        if (!m_surface || font == vgui2::INVALID_FONT)
            return 0;
        wchar_t wide[256];
        ConvertText(text, wide, sizeof(wide) / sizeof(wide[0]));
        int width = 0, height = 0;
        m_surface->GetTextSize(font, wide, width, height);
        return width;
    }

    void DrawText(int x, int y, const char* text, vgui2::HFont font,
        int r, int g, int b, int a)
    {
        if (!text || font == vgui2::INVALID_FONT)
            return;
        wchar_t wide[512];
        const int length = ConvertText(text, wide,
            sizeof(wide) / sizeof(wide[0]));

        m_surface->DrawSetTextFont(font);
        m_surface->DrawSetTextColor(r, g, b, a);
        m_surface->DrawSetTextPos(x, y);
        m_surface->DrawPrintText(wide, length);
    }

    void DrawWrappedText(int x, int y, int width, int height, const char* text,
        int r, int g, int b, int a)
    {
        if (!text || !text[0] || width <= 0 || height <= 0)
            return;

        const vgui2::HFont font = m_infoFont != vgui2::INVALID_FONT
            ? m_infoFont : m_font;
        const int lineTall = m_surface->GetFontTall(font) + 2;
        char line[256];
        int lineLength = 0;
        line[0] = '\0';
        const char* cursor = text;
        while (*cursor && y + lineTall <= height)
        {
            if (*cursor == '\r')
            {
                ++cursor;
                continue;
            }
            if (*cursor == '\n')
            {
                if (lineLength > 0)
                    DrawText(x, y, line, font, r, g, b, a);
                lineLength = 0;
                line[0] = '\0';
                y += lineTall;
                ++cursor;
                continue;
            }

            while (*cursor == ' ')
                ++cursor;
            char word[96];
            int wordLength = 0;
            while (*cursor && *cursor != ' ' && *cursor != '\r' &&
                *cursor != '\n' && wordLength < (int)sizeof(word) - 1)
                word[wordLength++] = *cursor++;
            word[wordLength] = '\0';
            if (!wordLength)
                continue;

            char candidate[256];
            if (lineLength)
                _snprintf(candidate, sizeof(candidate), "%s %s", line, word);
            else
                _snprintf(candidate, sizeof(candidate), "%s", word);
            candidate[sizeof(candidate) - 1] = '\0';
            if (lineLength && TextWidth(candidate, font) > width)
            {
                DrawText(x, y, line, font, r, g, b, a);
                y += lineTall;
                CopyText(line, sizeof(line), word);
                lineLength = (int)strlen(line);
            }
            else
            {
                CopyText(line, sizeof(line), candidate);
                lineLength = (int)strlen(line);
            }
        }
        if (lineLength > 0 && y + lineTall <= height)
            DrawText(x, y, line, font, r, g, b, a);
    }

    void ExecuteEntryUnderCursor()
    {
        if (!m_input)
            return;

        int x = 0;
        int y = 0;
        m_input->GetCursorPos(x, y);
        const int index = EntryIndexAt(x, y);
        if (index >= 0 && index < m_entryCount)
            Execute(index);
    }

    int EntryIndexAt(int x, int y) const
    {
        int left = 0, top = 0;
        GetMenuOrigin(left, top);
        for (int i = 0; i < m_entryCount; ++i)
        {
            const MenuEntry& entry = m_entries[i];
            if (entry.visible && entry.enabled &&
                x >= ScaleX(left + entry.x) &&
                x < ScaleX(left + entry.x + entry.wide) &&
                y >= ScaleY(top + entry.y) &&
                y < ScaleY(top + entry.y + entry.tall))
                return i;
        }
        return -1;
    }

    void UpdateHoveredEntry()
    {
        if (!m_visible || !m_input)
            return;
        int x = 0;
        int y = 0;
        m_input->GetCursorPos(x, y);
        const int hovered = EntryIndexAt(x, y);
        if (hovered != m_hoveredEntry)
        {
            m_hoveredEntry = hovered;
            if (hovered >= 0)
                m_selectedEntry = hovered;
            Repaint();
        }
    }

    void ClearMenu(int menuId)
    {
        m_currentMenu = menuId;
        m_entryCount = 0;
        m_hoveredEntry = -1;
        m_selectedEntry = -1;
        m_title[0] = '\0';
        ResetLayout();
    }

    static void CopyText(char* destination, int size, const char* source)
    {
        if (!destination || size <= 0)
            return;
        strncpy(destination, source ? source : "", size);
        destination[size - 1] = '\0';
    }

    static void RemoveAccelerators(char* text)
    {
        if (!text)
            return;
        char* output = text;
        for (const char* input = text; *input; ++input)
        {
            if (*input == '&' && input[1])
                continue;
            *output++ = *input;
        }
        *output = '\0';
    }

    void SetTitle(const char* title) { CopyText(m_title, sizeof(m_title), title); }

    static const char* PreviewImageName(const char* fieldName)
    {
        if (!fieldName || !fieldName[0] || !_stricmp(fieldName, "CancelButton"))
            return NULL;
        if (!_stricmp(fieldName, "autoselect_t") || !_stricmp(fieldName, "militia"))
            return "t_random";
        if (!_stricmp(fieldName, "autoselect_ct") || !_stricmp(fieldName, "spetsnaz"))
            return "ct_random";
        return fieldName;
    }

    static void BuildPreviewPath(MenuEntry& entry)
    {
        const char* imageName = PreviewImageName(entry.fieldName);
        if (!imageName)
        {
            entry.previewPath[0] = '\0';
            return;
        }

        char lower[64];
        CopyText(lower, sizeof(lower), imageName);
        for (char* cursor = lower; *cursor; ++cursor)
        {
            if (*cursor >= 'A' && *cursor <= 'Z')
                *cursor += 'a' - 'A';
        }
        _snprintf(entry.previewPath, sizeof(entry.previewPath),
            "gfx/vgui/%s.tga", lower);
        entry.previewPath[sizeof(entry.previewPath) - 1] = '\0';
    }

    void ResetLayout()
    {
        m_layoutOffsetX = 0;
        m_layoutOffsetY = 0;
        m_rootX = 0;
        m_rootY = 0;
        m_rootWide = 640;
        m_rootTall = 88;
        m_titleX = 20;
        m_titleY = 14;
        m_titleTall = 36;
        m_decorationCount = 1;
        m_decorations[0].x = 18;
        m_decorations[0].y = 56;
        m_decorations[0].wide = 604;
        m_decorations[0].tall = 1;
        m_resourceLabelCount = 0;
        m_infoPanelAvailable = false;
        m_mapInfoAvailable = false;
        m_infoX = m_infoY = m_infoWide = m_infoTall = 0;
        m_infoBoxWide = m_infoBoxTall = m_infoTextGap = 0;
        m_mapInfoX = m_mapInfoY = m_mapInfoWide = m_mapInfoTall = 0;
        m_mapDescription[0] = '\0';
        m_loadedPreviewEntry = -1;
        m_previewWide = m_previewTall = 0;
        m_infoText[0] = '\0';
    }

    void Add(const char* label, const char* command, int targetMenu, int key)
    {
        if (m_entryCount >= MAX_MENU_ENTRIES)
            return;
        MenuEntry& entry = m_entries[m_entryCount++];
        CopyText(entry.label, sizeof(entry.label), label);
        entry.fieldName[0] = '\0';
        entry.previewPath[0] = '\0';
        entry.command = command;
        entry.targetMenu = targetMenu;
        entry.key = key;
        entry.x = 20;
        entry.y = 60 + (m_entryCount - 1) * 30;
        entry.wide = 604;
        entry.tall = 30;
        entry.visible = true;
        entry.enabled = true;
        m_rootTall = 88 + m_entryCount * 30;
    }

    void AddClasses()
    {
        static const char* labels[] =
        {
            "1  CLASS 1", "2  CLASS 2", "3  CLASS 3",
            "4  CLASS 4", "5  AUTO-SELECT", "6  CLASS 6"
        };
        static const char* commands[] =
        {
            "joinclass 1\n", "joinclass 2\n", "joinclass 3\n",
            "joinclass 4\n", "joinclass 5\n", "joinclass 6\n"
        };
        for (int i = 0; i < 6; ++i)
            Add(labels[i], commands[i], 0, i + 1);
    }

    void AddBuyList(const BuyEntry* entries, int count)
    {
        for (int i = 0; i < count; ++i)
            Add(entries[i].label, entries[i].command, 0, i + 1);
    }

    const char* ResourceForMenu(int menuId) const
    {
        switch (menuId)
        {
        case CS_MENU_TEAM: return "resource/UI/Teammenu.res";
        case CS_MENU_CLASS_T: return "resource/UI/Classmenu_TER.res";
        case CS_MENU_CLASS_CT: return "resource/UI/Classmenu_CT.res";
        case CS_MENU_BUY: return "resource/UI/MainBuyMenu.res";
        case CS_MENU_BUY_PISTOL: return m_team == CS_TEAM_CT
            ? "resource/UI/BuyPistols_CT.res" : "resource/UI/BuyPistols_TER.res";
        case CS_MENU_BUY_SHOTGUN: return m_team == CS_TEAM_CT
            ? "resource/UI/BuyShotguns_CT.res" : "resource/UI/BuyShotguns_TER.res";
        case CS_MENU_BUY_RIFLE: return m_team == CS_TEAM_CT
            ? "resource/UI/BuyRifles_CT.res" : "resource/UI/BuyRifles_TER.res";
        case CS_MENU_BUY_SMG: return m_team == CS_TEAM_CT
            ? "resource/UI/BuySubMachineguns_CT.res" : "resource/UI/BuySubMachineguns_TER.res";
        case CS_MENU_BUY_MACHINEGUN: return m_team == CS_TEAM_CT
            ? "resource/UI/BuyMachineguns_CT.res" : "resource/UI/BuyMachineguns_TER.res";
        case CS_MENU_BUY_EQUIPMENT: return m_team == CS_TEAM_CT
            ? "resource/UI/BuyEquipment_CT.res" : "resource/UI/BuyEquipment_TER.res";
        default: return NULL;
        }
    }

    bool LoadMenuResource(int menuId)
    {
        const char* filename = ResourceForMenu(menuId);
        if (!filename)
            return false;

        cs16_vgui_resource_control_t controls[CS16_VGUI_MAX_RESOURCE_CONTROLS];
        const int count = CS16VGUI_LoadResourceLayout(filename, controls,
            CS16_VGUI_MAX_RESOURCE_CONTROLS);
        if (count <= 0)
            return false;

        bool hasRoot = false;
        bool hasTitle = false;
        int resourceButton = 0;
        m_decorationCount = 0;

        // Frame/WizardSubPanel coordinates are offsets inside CS's common
        // 640-wide viewport, not independent background rectangles.
        for (int i = 0; i < count; ++i)
        {
            const cs16_vgui_resource_control_t& control = controls[i];
            if ((!_stricmp(control.controlName, "Frame") ||
                 !_stricmp(control.controlName, "WizardSubPanel")) &&
                control.wide > 0 && control.tall > 0)
            {
                // Frame children are positioned relative to the frame. The
                // stock Buy Wizard resources, despite declaring xpos/ypos on
                // their root, place their children directly in the common
                // viewport (confirmed by the original client rendering).
                if (!_stricmp(control.controlName, "Frame"))
                {
                    m_layoutOffsetX = control.xpos;
                    m_layoutOffsetY = control.ypos;
                }
                hasRoot = true;
                break;
            }
        }
        m_rootX = 0;
        m_rootY = 0;
        m_rootWide = 640;
        m_rootTall = 448;

        // Resolve the custom information targets before assigning buttons.
        // MouseOverPanelButton controls point at these panels indirectly.
        for (int i = 0; i < count; ++i)
        {
            const cs16_vgui_resource_control_t& control = controls[i];
            if (!_stricmp(control.controlName, "Panel") &&
                (!_stricmp(control.fieldName, "ClassInfo") ||
                 !_stricmp(control.fieldName, "ItemInfo")) &&
                control.wide > 0 && control.tall > 0 &&
                control.xpos >= 0 && control.xpos < 640)
            {
                m_infoPanelAvailable = true;
                m_infoX = m_layoutOffsetX + control.xpos;
                m_infoY = m_layoutOffsetY + control.ypos;
                m_infoWide = control.wide;
                m_infoTall = control.tall;
            }
            else if (!_stricmp(control.controlName, "HTML") &&
                !_stricmp(control.fieldName, "MapInfo") &&
                control.wide > 0 && control.tall > 0)
            {
                m_mapInfoAvailable = true;
                m_mapInfoX = m_layoutOffsetX + control.xpos;
                m_mapInfoY = m_layoutOffsetY + control.ypos;
                m_mapInfoWide = control.wide;
                m_mapInfoTall = control.tall;
                CS16VGUI_LoadMapDescription(m_mapDescription,
                    sizeof(m_mapDescription));
            }
        }

        for (int i = 0; i < count; ++i)
        {
            const cs16_vgui_resource_control_t& control = controls[i];
            const bool isRoot = !_stricmp(control.controlName, "Frame") ||
                !_stricmp(control.controlName, "WizardSubPanel");
            if (isRoot && control.wide > 0 && control.tall > 0)
            {
                hasRoot = true;
                continue;
            }

            const bool isButton = !_stricmp(control.controlName, "Button") ||
                !_stricmp(control.controlName, "MouseOverPanelButton");
            if (isButton)
            {
                if (resourceButton >= m_entryCount)
                    continue;
                MenuEntry& entry = m_entries[resourceButton++];
                CopyText(entry.fieldName, sizeof(entry.fieldName), control.fieldName);
                if (!_stricmp(control.controlName, "MouseOverPanelButton"))
                    BuildPreviewPath(entry);
                if (control.wide > 0 && control.tall > 0)
                {
                    entry.x = m_layoutOffsetX + control.xpos;
                    entry.y = m_layoutOffsetY + control.ypos;
                    entry.wide = control.wide;
                    entry.tall = control.tall;
                }
                entry.visible = control.visible != 0;
                entry.enabled = control.enabled != 0;
                if (control.labelText[0])
                {
                    char localized[128];
                    CS16VGUI_LocalizeResourceText(control.labelText, localized,
                        sizeof(localized));
                    RemoveAccelerators(localized);
                    CopyText(entry.label, sizeof(entry.label), localized);
                }
                continue;
            }

            const bool isTitle = !_stricmp(control.controlName, "Label") &&
                (!_stricmp(control.font, "Title") ||
                 !_stricmp(control.fieldName, "Title") ||
                 !_stricmp(control.fieldName, "joinTeam") ||
                 !_stricmp(control.fieldName, "joinClass"));
            if (isTitle && !hasTitle)
            {
                m_titleX = m_layoutOffsetX + control.xpos;
                m_titleY = m_layoutOffsetY + control.ypos;
                m_titleTall = control.tall > 0 ? control.tall : 48;
                if (control.labelText[0])
                {
                    char localized[128];
                    CS16VGUI_LocalizeResourceText(control.labelText, localized,
                        sizeof(localized));
                    SetTitle(localized);
                }
                hasTitle = true;
                continue;
            }

            if (!_stricmp(control.controlName, "Label") && control.visible &&
                control.labelText[0] && m_resourceLabelCount < MAX_RESOURCE_LABELS)
            {
                ResourceLabel& label = m_resourceLabels[m_resourceLabelCount++];
                CS16VGUI_LocalizeResourceText(control.labelText, label.text,
                    sizeof(label.text));
                RemoveAccelerators(label.text);
                label.x = m_layoutOffsetX + control.xpos;
                label.y = m_layoutOffsetY + control.ypos;
                label.wide = control.wide;
                label.tall = control.tall;
                label.centered = !_stricmp(control.textAlignment, "center");
                continue;
            }

            if (!_stricmp(control.controlName, "Divider") && control.visible &&
                control.wide > 0 && control.tall > 0 &&
                m_decorationCount < MAX_DECORATIONS)
            {
                Decoration& decoration = m_decorations[m_decorationCount++];
                decoration.x = m_layoutOffsetX + control.xpos;
                decoration.y = m_layoutOffsetY + control.ypos;
                decoration.wide = control.wide;
                decoration.tall = control.tall;
            }
        }

        for (int i = resourceButton; i < m_entryCount; ++i)
            m_entries[i].visible = false;

        // ClassInfo/ItemInfo in the stock files can extend beyond their
        // WizardSubPanel. Keep images and wrapped descriptions inside the
        // shared viewport instead of painting past the black surface.
        if (m_infoPanelAvailable)
        {
            if (m_infoX + m_infoWide > m_rootWide)
                m_infoWide = m_rootWide - m_infoX;
            if (m_infoY + m_infoTall > m_rootTall)
                m_infoTall = m_rootTall - m_infoY;
            if (m_infoWide <= 0 || m_infoTall <= 0)
                m_infoPanelAvailable = false;
        }
        (void)hasRoot;
        return true;
    }

    void ApplyMenuRules(int menuId)
    {
        if (menuId == CS_MENU_CLASS_T || menuId == CS_MENU_CLASS_CT)
        {
            int nextY = m_entryCount > 0 ? m_entries[0].y : 116;
            for (int i = 0; i < m_entryCount; ++i)
            {
                MenuEntry& entry = m_entries[i];
                const bool fifthClass = i == 4 ||
                    !_stricmp(entry.fieldName, "militia") ||
                    !_stricmp(entry.fieldName, "spetsnaz");
                const bool autoSelect = i == 5 ||
                    !_strnicmp(entry.fieldName, "autoselect_", 11);
                if (fifthClass)
                    entry.visible = false;
                else if (autoSelect)
                {
                    entry.key = 5;
                    entry.command = "joinclass 5\n";
                }

                if (entry.visible)
                {
                    entry.y = nextY;
                    nextY += 32;
                }
            }
            return;
        }

        if (menuId != CS_MENU_TEAM)
            return;

        const bool vipMap = CS16VGUI_IsVIPMap() != 0;
        const bool canSpectate = CS16VGUI_CanSpectate() != 0;
        const bool canCancel = CS16VGUI_HasTeam() != 0;
        int nextY = m_entryCount > 0 ? m_entries[0].y : 116;
        for (int i = 0; i < m_entryCount; ++i)
        {
            MenuEntry& entry = m_entries[i];
            if (entry.key == 3)
                entry.visible = entry.visible && vipMap;
            else if (entry.key == 6)
                entry.visible = entry.visible && canSpectate;
            else if (entry.key == 0)
                entry.visible = entry.visible && canCancel;

            if (entry.visible)
            {
                entry.y = nextY;
                nextY += 32;
            }
        }
    }

    void ConfigureInformationPanel(int menuId)
    {
        if (!m_infoPanelAvailable)
            return;
        m_infoWide = 300;
        m_infoTall = 440 - m_infoY;
        m_infoBoxWide = 300;
        if (menuId == CS_MENU_CLASS_T || menuId == CS_MENU_CLASS_CT)
        {
            m_infoBoxTall = 196;
            m_infoTextGap = 34;
        }
        else if (menuId == CS_MENU_BUY_EQUIPMENT)
        {
            m_infoBoxTall = 144;
            m_infoTextGap = 15;
        }
        else
        {
            m_infoBoxTall = 80;
            m_infoTextGap = 15;
        }
    }

    static void AppendText(char* destination, int size, const char* text)
    {
        if (!destination || size <= 0 || !text || !text[0])
            return;
        const int length = (int)strlen(destination);
        if (length >= size - 1)
            return;
        strncat(destination, text, size - length - 1);
        destination[size - 1] = '\0';
    }

    static bool LocalizeToken(const char* token, char* output, int size)
    {
        if (!token || !output || size <= 0)
            return false;
        CS16VGUI_LocalizeResourceText(token, output, size);
        char* destination = output;
        for (const char* source = output; *source; ++source)
        {
            if (*source == '\\' && source[1] == 'n')
            {
                *destination++ = '\n';
                ++source;
            }
            else if (*source == '\\' && source[1] == 't')
            {
                *destination++ = ' ';
                ++source;
            }
            else
            {
                *destination++ = *source;
            }
        }
        *destination = '\0';
        return output[0] && _stricmp(output, token);
    }

    static const char* ClassTokenBase(const char* fieldName)
    {
        if (!_stricmp(fieldName, "terror")) return "Terror";
        if (!_stricmp(fieldName, "leet")) return "Leet";
        if (!_stricmp(fieldName, "arctic")) return "Arctic";
        if (!_stricmp(fieldName, "guerilla")) return "Guerilla";
        if (!_stricmp(fieldName, "militia")) return "Militia";
        if (!_stricmp(fieldName, "urban")) return "Urban";
        if (!_stricmp(fieldName, "gsg9")) return "GSG9";
        if (!_stricmp(fieldName, "sas")) return "SAS";
        if (!_stricmp(fieldName, "gign")) return "GIGN";
        if (!_stricmp(fieldName, "spetsnaz")) return "Spetsnaz";
        if (!_strnicmp(fieldName, "autoselect_", 11)) return "Autoselect";
        return NULL;
    }

    static const char* WeaponTokenBase(const char* fieldName)
    {
        if (!_stricmp(fieldName, "glock18")) return "Glock";
        if (!_stricmp(fieldName, "usp45")) return "USP45";
        if (!_stricmp(fieldName, "p228")) return "P228";
        if (!_stricmp(fieldName, "deserteagle")) return "DesertEagle";
        if (!_stricmp(fieldName, "fiveseven")) return "FiveSeven";
        if (!_stricmp(fieldName, "elites")) return "Elites";
        if (!_stricmp(fieldName, "m3")) return "M3";
        if (!_stricmp(fieldName, "xm1014")) return "XM1014";
        if (!_stricmp(fieldName, "tmp")) return "TMP";
        if (!_stricmp(fieldName, "mp5")) return "MP5";
        if (!_stricmp(fieldName, "mac10")) return "Mac10";
        if (!_stricmp(fieldName, "ump45")) return "UMP45";
        if (!_stricmp(fieldName, "p90")) return "P90";
        if (!_stricmp(fieldName, "famas")) return "Famas";
        if (!_stricmp(fieldName, "scout")) return "Scout";
        if (!_stricmp(fieldName, "ak47")) return "AK47";
        if (!_stricmp(fieldName, "galil")) return "Galil";
        if (!_stricmp(fieldName, "m4a1")) return "M4A1";
        if (!_stricmp(fieldName, "aug")) return "Aug";
        if (!_stricmp(fieldName, "sg550")) return "SG550";
        if (!_stricmp(fieldName, "sg552")) return "SG552";
        if (!_stricmp(fieldName, "awp")) return "AWP";
        if (!_stricmp(fieldName, "g3sg1")) return "G3SG1";
        if (!_stricmp(fieldName, "m249")) return "M249";
        return NULL;
    }

    void AppendLocalizedStat(const char* labelSuffix, const char* valueBase,
        const char* valueSuffix)
    {
        char labelToken[96], valueToken[96];
        char label[128], value[256];
        _snprintf(labelToken, sizeof(labelToken), "#CStrike_%sLabel", labelSuffix);
        _snprintf(valueToken, sizeof(valueToken), "#CStrike_%s%s", valueBase,
            valueSuffix);
        labelToken[sizeof(labelToken) - 1] = '\0';
        valueToken[sizeof(valueToken) - 1] = '\0';
        if (!LocalizeToken(valueToken, value, sizeof(value)))
            return;
        if (!LocalizeToken(labelToken, label, sizeof(label)))
            CopyText(label, sizeof(label), labelSuffix);
        AppendText(m_infoText, sizeof(m_infoText), label);
        AppendText(m_infoText, sizeof(m_infoText), "\t");
        AppendText(m_infoText, sizeof(m_infoText), value);
        AppendText(m_infoText, sizeof(m_infoText), "\n");
    }

    void BuildInformationText(const MenuEntry& entry)
    {
        m_infoText[0] = '\0';
        if (m_currentMenu == CS_MENU_CLASS_T || m_currentMenu == CS_MENU_CLASS_CT)
        {
            const char* base = ClassTokenBase(entry.fieldName);
            if (!base)
                return;
            char token[96], localized[1024];
            _snprintf(token, sizeof(token), "#Cstrike_%s_Label", base);
            if (LocalizeToken(token, localized, sizeof(localized)))
                AppendText(m_infoText, sizeof(m_infoText), localized);
            return;
        }

        const char* weaponBase = WeaponTokenBase(entry.fieldName);
        if (weaponBase)
        {
            AppendLocalizedStat("Price", weaponBase, "Price");
            AppendLocalizedStat("Origin", weaponBase, "Origin");
            AppendLocalizedStat("Calibre", weaponBase, "Calibre");
            AppendLocalizedStat("ClipCapacity", weaponBase, "ClipCapacity");
            AppendLocalizedStat("RateOfFire", weaponBase, "RateOfFire");
            AppendLocalizedStat("WeightLoaded", weaponBase, "WeightLoaded");
            AppendLocalizedStat("WeightEmpty", weaponBase, "WeightEmpty");
            AppendLocalizedStat("ProjectileWeight", weaponBase, "ProjectileWeight");
            AppendLocalizedStat("MuzzleVelocity", weaponBase, "MuzzleVelocity");
            AppendLocalizedStat("MuzzleEnergy", weaponBase, "MuzzleEnergy");
            return;
        }

        const char* priceToken = NULL;
        const char* descriptionToken = NULL;
        if (!_stricmp(entry.fieldName, "kevlar"))
        { priceToken = "#Cstrike_KevlarPrice"; descriptionToken = "#Cstrike_KevlarDescription"; }
        else if (!_stricmp(entry.fieldName, "kevlar_helmet"))
        { priceToken = "#Cstrike_KevlarHelmetPrice"; descriptionToken = "#Cstrike_KevlarHelmetDescription"; }
        else if (!_stricmp(entry.fieldName, "nightvision"))
        { priceToken = "#Cstrike_NightvisionPrice"; descriptionToken = "#Cstrike_NightvisionDescription"; }
        else if (!_stricmp(entry.fieldName, "shield"))
        { priceToken = "#Cstrike_ShieldPrice"; descriptionToken = "#Cstrike_ShieldDescription"; }
        else if (!_stricmp(entry.fieldName, "smokegrenade"))
        { priceToken = "#Cstrike_SmokeGrenadePrice"; descriptionToken = "#Cstrike_SmokeGrenadeDescription"; }
        else if (!_stricmp(entry.fieldName, "defuser"))
        { priceToken = "#Cstrike_DefuserPrice"; descriptionToken = "#Cstrike_DefuserDescription"; }
        else if (!_stricmp(entry.fieldName, "flashbang"))
        { priceToken = "#Cstrike_FlashbangPrice"; descriptionToken = "#Cstrike_FlashbangDescription"; }
        else if (!_stricmp(entry.fieldName, "hegrenade"))
        { priceToken = "#Cstrike_HEGrenadePrice"; descriptionToken = "#Cstrike_HEGrenadeDescription"; }

        char localized[1024];
        if (priceToken && LocalizeToken(priceToken, localized, sizeof(localized)))
        {
            AppendText(m_infoText, sizeof(m_infoText), localized);
            AppendText(m_infoText, sizeof(m_infoText), "\n\n");
        }
        if (descriptionToken && LocalizeToken(descriptionToken, localized,
            sizeof(localized)))
            AppendText(m_infoText, sizeof(m_infoText), localized);
    }

    int PreviewEntry() const
    {
        if (m_selectedEntry >= 0 && m_selectedEntry < m_entryCount &&
            m_entries[m_selectedEntry].visible &&
            m_entries[m_selectedEntry].previewPath[0])
            return m_selectedEntry;
        for (int i = 0; i < m_entryCount; ++i)
        {
            if (m_entries[i].visible && m_entries[i].previewPath[0])
                return i;
        }
        return -1;
    }

    void LoadPreview(int entryIndex)
    {
        if (entryIndex == m_loadedPreviewEntry || entryIndex < 0 ||
            entryIndex >= m_entryCount || !m_surface)
            return;
        if (!m_previewTexture)
            m_previewTexture = m_surface->CreateNewTextureID(false);
        m_previewWide = m_previewTall = 0;
        if (m_previewTexture)
        {
            if (CS16VGUI_LoadTGA(m_entries[entryIndex].previewPath,
                m_previewPixels, sizeof(m_previewPixels), &m_previewWide,
                &m_previewTall))
            {
                m_surface->DrawSetTextureRGBA(m_previewTexture,
                    m_previewPixels, m_previewWide, m_previewTall, 1, true);
            }
        }
        BuildInformationText(m_entries[entryIndex]);
        m_loadedPreviewEntry = entryIndex;
    }

    void DrawResourceLabel(int left, int top, const ResourceLabel& label)
    {
        int x = ScaleX(left + label.x);
        if (label.centered)
        {
            const int textWide = TextWidth(label.text, m_font);
            x += (ScaleX(label.wide) - textWide) / 2;
        }
        const int y0 = ScaleY(top + label.y);
        const int tall = ScaleY(label.tall);
        DrawText(x, y0 + (tall - m_surface->GetFontTall(m_font)) / 2,
            label.text, m_font,
            255, 165, 35, 255);
    }

    void DrawInformationPanel(int left, int top)
    {
        if (m_currentMenu == CS_MENU_TEAM && m_mapInfoAvailable &&
            m_mapDescription[0])
        {
            const int x0 = ScaleX(left + m_mapInfoX);
            const int y0 = ScaleY(top + m_mapInfoY);
            const int x1 = ScaleX(left + m_mapInfoX + m_mapInfoWide);
            const int y1 = ScaleY(top + m_mapInfoY + m_mapInfoTall);
            m_surface->DrawSetColor(188, 112, 0, 150);
            m_surface->DrawOutlinedRect(x0, y0, x1, y1);
            const int scrollWide = ScaleX(16);
            m_surface->DrawOutlinedRect(x1 - scrollWide, y0,
                x1, y1);
            DrawWrappedText(x0 + ScaleX(3), y0 + ScaleY(3),
                x1 - x0 - scrollWide - ScaleX(6), y1 - ScaleY(3), m_mapDescription,
                230, 180, 90, 255);
            return;
        }
        if (!m_infoPanelAvailable)
            return;

        const int entryIndex = PreviewEntry();
        if (entryIndex < 0)
            return;
        LoadPreview(entryIndex);

        const int boxX0 = ScaleX(left + m_infoX);
        const int boxY0 = ScaleY(top + m_infoY);
        const int boxX1 = ScaleX(left + m_infoX + m_infoBoxWide);
        const int boxY1 = ScaleY(top + m_infoY + m_infoBoxTall);
        m_surface->DrawSetColor(188, 112, 0, 150);
        m_surface->DrawOutlinedRect(boxX0, boxY0, boxX1, boxY1);

        const int drawWide = ScaleX(m_previewWide);
        const int drawTall = ScaleY(m_previewTall);
        int imageX = boxX0 + (boxX1 - boxX0 - drawWide) / 2;
        int imageY = boxY0 + (boxY1 - boxY0 - drawTall) / 2;
        if (imageX < boxX0) imageX = boxX0;
        if (imageY < boxY0) imageY = boxY0;
        if (m_previewTexture && drawWide > 0 && drawTall > 0)
        {
            m_surface->DrawSetColor(255, 255, 255, 255);
            m_surface->DrawSetTexture(m_previewTexture);
            m_surface->DrawTexturedRect(imageX, imageY,
                imageX + drawWide, imageY + drawTall);
        }

        const int textX = ScaleX(left + m_infoX);
        const int textY = ScaleY(top + m_infoY + m_infoBoxTall + m_infoTextGap);
        const int textBottom = ScaleY(top + m_infoY + m_infoTall);
        if (m_currentMenu >= CS_MENU_BUY_PISTOL &&
            m_currentMenu <= CS_MENU_BUY_MACHINEGUN)
        {
            char text[2048];
            CopyText(text, sizeof(text), m_infoText);
            const int lineTall = m_surface->GetFontTall(m_infoFont) + 3;
            int y = textY;
            for (char* line = text; line && *line && y + lineTall <= textBottom;)
            {
                char* next = strchr(line, '\n');
                if (next) *next++ = '\0';
                char* value = strchr(line, '\t');
                if (value) *value++ = '\0';
                DrawText(textX, y, line, m_infoFont, 230, 180, 90, 255);
                if (value)
                    DrawText(ScaleX(left + m_infoX + 140), y, value,
                        m_infoFont, 230, 180, 90, 255);
                y += lineTall;
                line = next;
            }
        }
        else
        {
            DrawWrappedText(textX, textY, ScaleX(m_infoWide), textBottom,
                m_infoText, 230, 180, 90, 255);
        }
    }

    void GetMenuOrigin(int& x, int& y) const
    {
        x = m_rootX;
        y = m_rootY;
    }

    int TitleTextInset() const
    {
        return m_titleTall > 18 ? (m_titleTall - 18) / 2 : 0;
    }

    static int EntryTextInset(const MenuEntry& entry)
    {
        return entry.tall > 16 ? (entry.tall - 16) / 2 : 0;
    }

    void Execute(int index)
    {
        if (index < 0 || index >= m_entryCount)
            return;
        if (!m_entries[index].visible || !m_entries[index].enabled)
            return;
        const MenuEntry entry = m_entries[index];
        if (entry.targetMenu)
        {
            ShowMenu(entry.targetMenu);
            return;
        }
        HideMenu();
        if (entry.command)
            CS16VGUI_ClientCommand(entry.command);
    }

    vgui2::IVGui* m_ivgui;
    vgui2::IPanel* m_panel;
    vgui2::ISurface* m_surface;
    vgui2::IInput* m_input;
    vgui2::VPANEL m_vpanel;
    vgui2::VPANEL m_hudVPanel;
    CCS16HudTextPanel m_hudPanelClient;
    bool m_visible;
    bool m_legacyCursorVisible;
    int m_currentMenu;
    int m_team;
    int m_entryCount;
    int m_hoveredEntry;
    int m_selectedEntry;
    HudTextDraw m_hudTextDraws[MAX_HUD_TEXT_DRAWS];
    int m_hudTextDrawCount;
    vgui2::HFont m_hudFont;
    vgui2::HFont m_font;
    vgui2::HFont m_titleFont;
    vgui2::HFont m_infoFont;
    char m_title[128];
    int m_layoutOffsetX, m_layoutOffsetY;
    int m_rootX, m_rootY, m_rootWide, m_rootTall;
    int m_titleX, m_titleY, m_titleTall;
    Decoration m_decorations[MAX_DECORATIONS];
    int m_decorationCount;
    ResourceLabel m_resourceLabels[MAX_RESOURCE_LABELS];
    int m_resourceLabelCount;
    bool m_infoPanelAvailable;
    bool m_mapInfoAvailable;
    int m_infoX, m_infoY, m_infoWide, m_infoTall;
    int m_infoBoxWide, m_infoBoxTall, m_infoTextGap;
    int m_mapInfoX, m_mapInfoY, m_mapInfoWide, m_mapInfoTall;
    int m_previewTexture;
    int m_loadedPreviewEntry;
    int m_previewWide, m_previewTall;
    int m_logoTexture;
    bool m_logoLoaded;
    char m_infoText[2048];
    char m_mapDescription[4096];
    unsigned char m_previewPixels[256 * 256 * 4];
    MenuEntry m_entries[MAX_MENU_ENTRIES];
};

void CCS16HudTextPanel::PaintTraverse(bool, bool)
{
    if (m_owner)
        m_owner->PaintHudText();
}

class CCS16ClientVGUI : public IClientVGUI
{
public:
    CCS16ClientVGUI() : m_initialized(false) {}

    void Initialize(CreateInterfaceFn* factories, int count) override
    {
        if (m_initialized)
            return;
        CS16VGUI_Trace("VGUI2: VClientVGUI Initialize enter");
        CS16VGUI2_Startup();
        if (!m_viewport.Initialize(factories, count))
        {
            CS16VGUI_Print("[CS16 VGUI2] viewport initialization failed; using VGUI1.\n");
            CS16VGUI_Trace("VGUI2: VClientVGUI Initialize failed");
            return;
        }
        m_initialized = true;
        CS16VGUI_Print("[CS16 VGUI2] VClientVGUI001 initialized.\n");
        CS16VGUI_Trace("VGUI2: VClientVGUI Initialize complete");
    }

    void Start() override {}
    void SetParent(vgui2::VPANEL parent) override { m_viewport.SetParent(parent); }
    // GoldSrc can host the VGUI2 viewport and its legacy VGUI1 root together.
    // Keep VGUI1 enabled for commandmenu.txt and as the runtime fallback.
    int UseVGUI1() override { return 1; }
    void HideScoreBoard() override {}
    void HideAllVGUIMenu() override { m_viewport.HideMenu(); }
    void ActivateClientUI() override {}
    void HideClientUI() override { m_viewport.HideMenu(); }

    void Shutdown() override
    {
        if (!m_initialized)
            return;
        m_viewport.Shutdown();
        m_initialized = false;
    }

    bool IsReady() const { return m_initialized && m_viewport.IsReady(); }
    bool ShowMenu(int menuId) { return IsReady() && m_viewport.ShowMenu(menuId); }
    void HideMenu() { if (IsReady()) m_viewport.HideMenu(); }
    bool KeyInput(int keynum) { return IsReady() && m_viewport.KeyInput(keynum); }
    void SetTeam(int team) { if (IsReady()) m_viewport.SetTeam(team); }
    void SetLegacyCursorVisible(bool visible)
    {
        if (IsReady()) m_viewport.SetLegacyCursorVisible(visible);
    }
    void BeginHudTextFrame()
    {
        if (IsReady()) m_viewport.BeginHudTextFrame();
    }
    int DrawHudString(int x, int y, const char* text,
        int r, int g, int b, int a)
    {
        return IsReady()
            ? m_viewport.DrawHudString(x, y, text, r, g, b, a)
            : -1;
    }
    bool GetHudStringSize(const char* text, int& wide, int& tall)
    {
        return IsReady() && m_viewport.GetHudStringSize(text, wide, tall);
    }

private:
    CCS16VGUI2Viewport m_viewport;
    bool m_initialized;
};

CCS16ClientVGUI g_clientVGUI;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CCS16ClientVGUI, IClientVGUI,
    CS16_CLIENTVGUI_INTERFACE_VERSION, g_clientVGUI)

extern "C" int CS16VGUI2_IsViewportReady(void)
{
    return g_clientVGUI.IsReady() ? 1 : 0;
}

extern "C" int CS16VGUI2_ShowMenu(int menuId)
{
    return g_clientVGUI.ShowMenu(menuId) ? 1 : 0;
}

extern "C" void CS16VGUI2_HideMenu(void)
{
    g_clientVGUI.HideMenu();
}

extern "C" int CS16VGUI2_KeyInput(int down, int keynum, const char*)
{
    return down && g_clientVGUI.KeyInput(keynum) ? 1 : 0;
}

extern "C" void CS16VGUI2_SetTeam(int team)
{
    g_clientVGUI.SetTeam(team);
}

extern "C" void CS16VGUI2_SetLegacyCursorVisible(int visible)
{
    g_clientVGUI.SetLegacyCursorVisible(visible != 0);
}

extern "C" void CS16VGUI2_BeginHudTextFrame(void)
{
    g_clientVGUI.BeginHudTextFrame();
}

extern "C" int CS16VGUI2_DrawHudString(int x, int y, const char* text,
    int r, int g, int b, int a)
{
    return g_clientVGUI.DrawHudString(x, y, text, r, g, b, a);
}

extern "C" int CS16VGUI2_GetHudStringSize(const char* text, int* wide, int* tall)
{
    if (!wide || !tall)
        return 0;
    return g_clientVGUI.GetHudStringSize(text, *wide, *tall) ? 1 : 0;
}

extern "C" void CS16VGUI2_ShutdownViewport(void)
{
    g_clientVGUI.Shutdown();
}
