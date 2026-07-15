// Minimal Counter-Strike VGUI1 viewport for ordinary Steam GoldSrc.
//
// This translation unit uses the Microsoft x86 C++ ABI because it derives
// from classes implemented by vgui.dll. Communication with the rest of
// client.dll stays behind a C ABI.

#if defined(_CS16CLIENT_ENABLE_VGUI1)

#include <VGUI_ActionSignal.h>
#include <VGUI_App.h>
#include <VGUI_Button.h>
#include <VGUI_Label.h>
#include <VGUI_Panel.h>
#include <VGUI_Scheme.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#if defined(_CS16CLIENT_VGUI_STANDALONE_NEW)
extern "C" void* malloc(unsigned int size);
extern "C" void free(void* memory);

void* __cdecl operator new(unsigned int size) { return malloc(size); }
void* __cdecl operator new[](unsigned int size) { return malloc(size); }
void __cdecl operator delete(void* memory) noexcept { free(memory); }
void __cdecl operator delete[](void* memory) noexcept { free(memory); }
void __cdecl operator delete(void* memory, unsigned int) noexcept { free(memory); }
void __cdecl operator delete[](void* memory, unsigned int) noexcept { free(memory); }
#endif

extern "C" void CS16VGUI_ClientCommand(const char* command);
extern "C" void CS16VGUI_SetMouseVisible(int visible);
extern "C" void CS16VGUI_Trace(const char* stage);
extern "C" int CS16VGUI_CommandMenuPrepare(void);
extern "C" int CS16VGUI_CommandMenuGetCount(int node);
extern "C" int CS16VGUI_CommandMenuGetParent(int node);
extern "C" int CS16VGUI_CommandMenuGetItem(int node, int visibleIndex,
    const char** displayText, int* itemIndex, int* childNode, int* boundKey);
extern "C" void CS16VGUI_CommandMenuExecute(int itemIndex);

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
    CS_MENU_COMMAND = 100,
    CS_TEAM_T = 1,
    CS_TEAM_CT = 2,
    GOLDSRC_KEY_ESCAPE = 27,
    MAX_COMMAND_MENU_BUTTONS = 10,
    COMMAND_MENU_PAGE_SIZE = 8
};

struct BuyEntry
{
    const char* label;
    const char* command;
};

static const BuyEntry g_pistolsT[] =
{
    { "1  GLOCK 18", "glock\n" }, { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" }, { "4  DESERT EAGLE", "deagle\n" },
    { "5  DUAL ELITES", "elites\n" }
};
static const BuyEntry g_pistolsCT[] =
{
    { "1  GLOCK 18", "glock\n" }, { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" }, { "4  DESERT EAGLE", "deagle\n" },
    { "5  FIVE-SEVEN", "fiveseven\n" }
};
static const BuyEntry g_shotguns[] =
{
    { "1  M3 SUPER 90", "m3\n" }, { "2  XM1014", "xm1014\n" }
};
static const BuyEntry g_smgsT[] =
{
    { "1  MAC-10", "mac10\n" }, { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" }, { "4  P90", "p90\n" }
};
static const BuyEntry g_smgsCT[] =
{
    { "1  TMP", "tmp\n" }, { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" }, { "4  P90", "p90\n" }
};
static const BuyEntry g_riflesT[] =
{
    { "1  GALIL", "galil\n" }, { "2  AK-47", "ak47\n" },
    { "3  SCOUT", "scout\n" }, { "4  SG-552", "sg552\n" },
    { "5  AWP", "awp\n" }, { "6  G3/SG-1", "g3sg1\n" }
};
static const BuyEntry g_riflesCT[] =
{
    { "1  FAMAS", "famas\n" }, { "2  SCOUT", "scout\n" },
    { "3  M4A1", "m4a1\n" }, { "4  AUG", "aug\n" },
    { "5  SG-550", "sg550\n" }, { "6  AWP", "awp\n" }
};
static const BuyEntry g_machineGuns[] = { { "1  M249", "m249\n" } };
static const BuyEntry g_equipmentT[] =
{
    { "1  KEVLAR", "vest\n" }, { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" }, { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" }, { "6  NIGHTVISION", "nvgs\n" }
};
static const BuyEntry g_equipmentCT[] =
{
    { "1  KEVLAR", "vest\n" }, { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" }, { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" }, { "6  DEFUSE KIT", "defuser\n" },
    { "7  NIGHTVISION", "nvgs\n" }, { "8  TACTICAL SHIELD", "shield\n" }
};

class CCSViewport;

class CMenuActionSignal final : public vgui::ActionSignal
{
public:
    CMenuActionSignal(CCSViewport* viewport, const char* command, int targetMenu)
        : m_viewport(viewport), m_command(command), m_targetMenu(targetMenu) {}
    void actionPerformed(vgui::Panel*) override;
private:
    CCSViewport* m_viewport;
    const char* m_command;
    int m_targetMenu;
};

class CCommandSlotSignal final : public vgui::ActionSignal
{
public:
    CCommandSlotSignal(CCSViewport* viewport, int slot)
        : m_viewport(viewport), m_slot(slot) {}
    void actionPerformed(vgui::Panel*) override;
private:
    CCSViewport* m_viewport;
    int m_slot;
};

class CCSMenuButton : public vgui::Button
{
public:
    CCSMenuButton(const char* text, int x, int y, int wide, int tall)
        : vgui::Button(text, x, y, wide, tall)
    {
        setButtonBorderEnabled(false);
        setPaintBackgroundEnabled(true);
    }
protected:
    void paintBackground() override
    {
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);
        if (isArmed()) drawSetColor(88, 67, 26, 0);
        else if (isSelected()) drawSetColor(66, 55, 28, 0);
        else drawSetColor(27, 34, 28, 0);
        drawFilledRect(0, 0, wide, tall);
        drawSetColor(isArmed() ? 224 : 145, isArmed() ? 160 : 103, 30, 0);
        drawOutlinedRect(0, 0, wide, tall);
    }
};

static int g_nextCommandTexture = 4100;

static int Utf8ToWide(const char* text, wchar_t* output, int capacity)
{
    if (!output || capacity <= 0) return 0;
    output[0] = 0;
    if (!text) return 0;
    int result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, output, capacity);
    if (result <= 0)
        result = MultiByteToWideChar(1251, 0, text, -1, output, capacity);
    output[capacity - 1] = 0;
    return result;
}

class CUnicodeCommandButton final : public CCSMenuButton
{
public:
    CUnicodeCommandButton(int x, int y, int wide, int tall)
        : CCSMenuButton("", x, y, wide, tall), m_texture(++g_nextCommandTexture),
          m_textureWidth(0), m_textureHeight(0), m_hasTexture(false)
    {
        m_text[0] = 0;
    }

    void SetUtf8Text(const char* text)
    {
        Utf8ToWide(text, m_text, (int)(sizeof(m_text) / sizeof(m_text[0])));
        BuildTexture();
        repaint();
    }

protected:
    void paint() override
    {
        if (!m_hasTexture) return;
        drawSetColor(255, 255, 255, 0);
        drawSetTexture(m_texture);
        drawTexturedRect(10, 3, 10 + m_textureWidth, 3 + m_textureHeight);
    }

private:
    void BuildTexture()
    {
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);
        if (wide <= 0 || tall <= 0) return;

        HDC dc = CreateCompatibleDC(NULL);
        if (!dc) return;

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = wide;
        info.bmiHeader.biHeight = -tall;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* pixels = NULL;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
        if (!bitmap || !pixels)
        {
            if (bitmap) DeleteObject(bitmap);
            DeleteDC(dc);
            return;
        }

        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
        HGDIOBJ oldFont = font ? SelectObject(dc, font) : NULL;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rect = { 0, 0, wide, tall };
        DrawTextW(dc, m_text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        unsigned char* rgba = new unsigned char[wide * tall * 4];
        unsigned char* bgra = static_cast<unsigned char*>(pixels);
        for (int i = 0; i < wide * tall; ++i)
        {
            const unsigned char alpha = bgra[i * 4 + 2];
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = alpha;
        }
        drawSetTextureRGBA(m_texture, reinterpret_cast<const char*>(rgba), wide, tall);
        delete[] rgba;

        if (oldFont) SelectObject(dc, oldFont);
        if (font) DeleteObject(font);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        m_textureWidth = wide;
        m_textureHeight = tall;
        m_hasTexture = true;
    }

    wchar_t m_text[256];
    int m_texture;
    int m_textureWidth;
    int m_textureHeight;
    bool m_hasTexture;
};

class CCSMenuPanel : public vgui::Panel
{
public:
    CCSMenuPanel(const char* titleText, int height) : vgui::Panel(0, 0, 510, height)
    {
        setPaintBackgroundEnabled(true);
        vgui::Label* title = new vgui::Label(titleText, 20, 14, 470, 36);
        title->setParent(this);
        title->setContentAlignment(vgui::Label::a_center);
        title->setFgColor(255, 180, 32, 0);
        title->setPaintBackgroundEnabled(false);
    }
protected:
    void AddButton(CCSViewport* viewport, const char* text, int y,
        const char* command, int targetMenu, int tall = 30)
    {
        vgui::Button* button = new CCSMenuButton(text, 40, y, 430, tall);
        button->setParent(this);
        button->setContentAlignment(vgui::Label::a_west);
        button->setFgColor(230, 230, 220, 0);
        button->addActionSignal(new CMenuActionSignal(viewport, command, targetMenu));
    }
    void paintBackground() override
    {
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);
        drawSetColor(8, 12, 8, 28);
        drawFilledRect(0, 0, wide, tall);
        drawSetColor(190, 125, 24, 0);
        drawOutlinedRect(0, 0, wide, tall);
        drawOutlinedRect(2, 2, wide - 2, tall - 2);
    }
};

class CSelectionPanel final : public CCSMenuPanel
{
public:
    CSelectionPanel(CCSViewport* viewport, int menuId)
        : CCSMenuPanel(menuId == CS_MENU_TEAM ? "CHOOSE A TEAM" :
            menuId == CS_MENU_CLASS_T ? "CHOOSE A TERRORIST" :
            "CHOOSE A COUNTER-TERRORIST", menuId == CS_MENU_TEAM ? 350 : 390)
    {
        if (menuId == CS_MENU_TEAM)
        {
            AddButton(viewport, "1  TERRORIST FORCES", 72, "jointeam 1\n", 0, 38);
            AddButton(viewport, "2  CT FORCES", 124, "jointeam 2\n", 0, 38);
            AddButton(viewport, "5  AUTO-SELECT", 176, "jointeam 5\n", 0, 38);
            AddButton(viewport, "6  SPECTATE", 228, "jointeam 6\n", 0, 38);
            AddButton(viewport, "0  CANCEL", 286, 0, 0, 38);
        }
        else
        {
            const char* namesT[] = { "1  PHOENIX CONNEXION", "2  L337 KREW", "3  ARCTIC AVENGERS", "4  GUERILLA WARFARE", "5  AUTO-SELECT" };
            const char* namesCT[] = { "1  SEAL TEAM 6", "2  GSG-9", "3  SAS", "4  GIGN", "5  AUTO-SELECT" };
            const char** names = menuId == CS_MENU_CLASS_T ? namesT : namesCT;
            static const char* commands[] = { "joinclass 1\n", "joinclass 2\n", "joinclass 3\n", "joinclass 4\n", "joinclass 5\n" };
            for (int i = 0; i < 5; ++i) AddButton(viewport, names[i], 66 + i * 50, commands[i], 0, 38);
            AddButton(viewport, "0  CANCEL", 326, 0, 0, 38);
        }
    }
};

class CBuyRootPanel final : public CCSMenuPanel
{
public:
    CBuyRootPanel(CCSViewport* viewport) : CCSMenuPanel("BUY MENU", 436)
    {
        AddButton(viewport, "1  HANDGUNS", 58, 0, CS_MENU_BUY_PISTOL);
        AddButton(viewport, "2  SHOTGUNS", 94, 0, CS_MENU_BUY_SHOTGUN);
        AddButton(viewport, "3  SUB-MACHINE GUNS", 130, 0, CS_MENU_BUY_SMG);
        AddButton(viewport, "4  RIFLES", 166, 0, CS_MENU_BUY_RIFLE);
        AddButton(viewport, "5  MACHINE GUN", 202, 0, CS_MENU_BUY_MACHINEGUN);
        AddButton(viewport, "6  PRIMARY WEAPON AMMO", 238, "primammo\n", 0);
        AddButton(viewport, "7  SECONDARY WEAPON AMMO", 274, "secammo\n", 0);
        AddButton(viewport, "8  EQUIPMENT", 310, 0, CS_MENU_BUY_EQUIPMENT);
        AddButton(viewport, "0  CANCEL", 382, 0, 0);
    }
};

class CBuyPanel final : public CCSMenuPanel
{
public:
    CBuyPanel(CCSViewport* viewport, const char* title, const BuyEntry* entries, int count)
        : CCSMenuPanel(title, 148 + count * 36)
    {
        for (int i = 0; i < count; ++i)
            AddButton(viewport, entries[i].label, 58 + i * 36, entries[i].command, 0);
        const int bottom = 58 + count * 36 + 8;
        AddButton(viewport, "9  BACK", bottom, 0, CS_MENU_BUY);
        AddButton(viewport, "0  CANCEL", bottom + 36, 0, 0);
    }
};

class CCommandMenuPanel final : public CCSMenuPanel
{
public:
    CCommandMenuPanel(CCSViewport* viewport) : CCSMenuPanel("COMMAND MENU", 90)
    {
        for (int i = 0; i < MAX_COMMAND_MENU_BUTTONS; ++i)
        {
            CUnicodeCommandButton* button = new CUnicodeCommandButton(20, 48 + i * 30, 320, 27);
            button->setParent(this);
            button->addActionSignal(new CCommandSlotSignal(viewport, i));
            button->setVisible(false);
            m_buttons[i] = button;
        }
    }

    bool Refresh(int node, int page)
    {
        CS16VGUI_Trace("VGUI1: command menu refresh enter");
        const int count = CS16VGUI_CommandMenuGetCount(node);
        const int pageCount = count > 0 ? (count + COMMAND_MENU_PAGE_SIZE - 1) / COMMAND_MENU_PAGE_SIZE : 1;
        if (page < 0) page = 0;
        if (page >= pageCount) page = pageCount - 1;
        const int first = page * COMMAND_MENU_PAGE_SIZE;
        int rows = count - first;
        if (rows > COMMAND_MENU_PAGE_SIZE) rows = COMMAND_MENU_PAGE_SIZE;
        if (rows < 0) rows = 0;

        for (int i = 0; i < rows; ++i)
        {
            const char* display = 0;
            if (!CS16VGUI_CommandMenuGetItem(node, first + i, &display, 0, 0, 0))
            {
                m_buttons[i]->setVisible(false);
                continue;
            }
            char numbered[320];
            _snprintf(numbered, sizeof(numbered), "%d  %s", i + 1, display ? display : "");
            numbered[sizeof(numbered) - 1] = 0;
            m_buttons[i]->SetUtf8Text(numbered);
            m_buttons[i]->setVisible(true);
        }
        for (int i = rows; i < COMMAND_MENU_PAGE_SIZE; ++i) m_buttons[i]->setVisible(false);

        if (page + 1 < pageCount)
        {
            m_buttons[8]->SetUtf8Text("9  NEXT");
            m_buttons[8]->setVisible(true);
        }
        else m_buttons[8]->setVisible(false);

        const int parent = CS16VGUI_CommandMenuGetParent(node);
        m_buttons[9]->SetUtf8Text(parent >= 0 || page > 0 ? "0  BACK" : "0  CLOSE");
        m_buttons[9]->setVisible(true);
        setSize(360, 58 + MAX_COMMAND_MENU_BUTTONS * 30);
        CS16VGUI_Trace("VGUI1: command menu refresh complete");
        return count > 0 || parent >= 0;
    }
private:
    CUnicodeCommandButton* m_buttons[MAX_COMMAND_MENU_BUTTONS];
};

class CCSViewport final : public vgui::Panel
{
public:
    CCSViewport(int width, int height)
        : vgui::Panel(0, 0, width, height), m_width(width), m_height(height),
          m_currentMenu(0), m_team(CS_TEAM_T), m_currentEntries(0),
          m_currentEntryCount(0), m_commandNode(0), m_commandPage(0),
          m_panelCount(0), m_commandMenu(0)
    {
        setPaintBackgroundEnabled(false);
        m_teamMenu = AddPanel(new CSelectionPanel(this, CS_MENU_TEAM));
        m_tClassMenu = AddPanel(new CSelectionPanel(this, CS_MENU_CLASS_T));
        m_ctClassMenu = AddPanel(new CSelectionPanel(this, CS_MENU_CLASS_CT));
        m_buyRoot = AddPanel(new CBuyRootPanel(this));
        m_pistolsT = AddPanel(new CBuyPanel(this, "BUY HANDGUN", g_pistolsT, Count(g_pistolsT)));
        m_pistolsCT = AddPanel(new CBuyPanel(this, "BUY HANDGUN", g_pistolsCT, Count(g_pistolsCT)));
        m_shotguns = AddPanel(new CBuyPanel(this, "BUY SHOTGUN", g_shotguns, Count(g_shotguns)));
        m_riflesT = AddPanel(new CBuyPanel(this, "BUY RIFLE", g_riflesT, Count(g_riflesT)));
        m_riflesCT = AddPanel(new CBuyPanel(this, "BUY RIFLE", g_riflesCT, Count(g_riflesCT)));
        m_smgsT = AddPanel(new CBuyPanel(this, "BUY SUB-MACHINE GUN", g_smgsT, Count(g_smgsT)));
        m_smgsCT = AddPanel(new CBuyPanel(this, "BUY SUB-MACHINE GUN", g_smgsCT, Count(g_smgsCT)));
        m_machineGuns = AddPanel(new CBuyPanel(this, "BUY MACHINE GUN", g_machineGuns, Count(g_machineGuns)));
        m_equipmentT = AddPanel(new CBuyPanel(this, "BUY EQUIPMENT", g_equipmentT, Count(g_equipmentT)));
        m_equipmentCT = AddPanel(new CBuyPanel(this, "BUY EQUIPMENT", g_equipmentCT, Count(g_equipmentCT)));
        m_commandMenu = static_cast<CCommandMenuPanel*>(AddPanel(new CCommandMenuPanel(this)));
    }

    void Attach(vgui::Panel* root, int width, int height)
    {
        m_width = width; m_height = height;
        setBounds(0, 0, width, height); setParent(root); setVisible(true);
        LayoutMenus(); HideMenu();
    }
    void SetTeam(int team) { if (team == CS_TEAM_T || team == CS_TEAM_CT) m_team = team; }

    int ShowMenu(int menuId)
    {
        const BuyEntry* entries = 0; int count = 0;
        CCSMenuPanel* panel = PanelForMenu(menuId, entries, count);
        if (!panel) return 0;
        HideAllPanels(); panel->setVisible(true); panel->requestFocus();
        m_currentMenu = menuId; m_currentEntries = entries; m_currentEntryCount = count;
        UpdateCursor(true); repaint(); return 1;
    }

    int ShowCommandMenu()
    {
        if (m_currentMenu == CS_MENU_COMMAND) { HideMenu(); return 0; }
        if (!CS16VGUI_CommandMenuPrepare()) return 0;
        return ShowCommandNode(0, 0);
    }
    void ReleaseCommandMenu() { if (m_currentMenu == CS_MENU_COMMAND) HideMenu(); }
    void HideMenu()
    {
        HideAllPanels(); m_currentMenu = 0; m_currentEntries = 0;
        m_currentEntryCount = 0; m_commandNode = 0; m_commandPage = 0;
        UpdateCursor(false);
    }
    void PerformAction(const char* command, int targetMenu)
    {
        if (targetMenu) { ShowMenu(targetMenu); return; }
        HideMenu(); if (command) CS16VGUI_ClientCommand(command);
    }

    void PerformCommandSlot(int slot)
    {
        if (m_currentMenu != CS_MENU_COMMAND || slot < 0) return;
        const int count = CS16VGUI_CommandMenuGetCount(m_commandNode);
        const int first = m_commandPage * COMMAND_MENU_PAGE_SIZE;
        const int parent = CS16VGUI_CommandMenuGetParent(m_commandNode);
        if (slot == 8)
        {
            if (first + COMMAND_MENU_PAGE_SIZE < count)
                ShowCommandNode(m_commandNode, m_commandPage + 1);
            return;
        }
        if (slot == 9)
        {
            if (m_commandPage > 0) ShowCommandNode(m_commandNode, m_commandPage - 1);
            else if (parent >= 0) ShowCommandNode(parent, 0);
            else HideMenu();
            return;
        }
        const int visibleIndex = first + slot;
        if (slot >= COMMAND_MENU_PAGE_SIZE || visibleIndex >= count) return;
        int itemIndex = -1, childNode = -1;
        if (!CS16VGUI_CommandMenuGetItem(m_commandNode, visibleIndex, 0, &itemIndex, &childNode, 0)) return;
        if (childNode >= 0) { ShowCommandNode(childNode, 0); return; }
        HideMenu(); CS16VGUI_CommandMenuExecute(itemIndex);
    }

    int KeyInput(int down, int keynum)
    {
        if (!m_currentMenu || !down) return 0;
        if (keynum == GOLDSRC_KEY_ESCAPE) { PerformAction(0, 0); return 1; }
        if (m_currentMenu == CS_MENU_COMMAND)
        {
            if (keynum >= '1' && keynum <= '8') { PerformCommandSlot(keynum - '1'); return 1; }
            if (keynum == '9') { PerformCommandSlot(8); return 1; }
            if (keynum == '0') { PerformCommandSlot(9); return 1; }
            return 0;
        }
        if (m_currentMenu == CS_MENU_TEAM)
        {
            switch (keynum) { case '1': PerformAction("jointeam 1\n", 0); return 1; case '2': PerformAction("jointeam 2\n", 0); return 1; case '5': PerformAction("jointeam 5\n", 0); return 1; case '6': PerformAction("jointeam 6\n", 0); return 1; case '0': PerformAction(0, 0); return 1; default: return 0; }
        }
        if (m_currentMenu == CS_MENU_CLASS_T || m_currentMenu == CS_MENU_CLASS_CT)
        {
            if (keynum >= '1' && keynum <= '5') { static const char* c[] = { "joinclass 1\n", "joinclass 2\n", "joinclass 3\n", "joinclass 4\n", "joinclass 5\n" }; PerformAction(c[keynum - '1'], 0); return 1; }
            if (keynum == '0') { PerformAction(0, 0); return 1; }
            return 0;
        }
        if (m_currentMenu == CS_MENU_BUY)
        {
            switch (keynum) { case '1': PerformAction(0, CS_MENU_BUY_PISTOL); return 1; case '2': PerformAction(0, CS_MENU_BUY_SHOTGUN); return 1; case '3': PerformAction(0, CS_MENU_BUY_SMG); return 1; case '4': PerformAction(0, CS_MENU_BUY_RIFLE); return 1; case '5': PerformAction(0, CS_MENU_BUY_MACHINEGUN); return 1; case '6': PerformAction("primammo\n", 0); return 1; case '7': PerformAction("secammo\n", 0); return 1; case '8': PerformAction(0, CS_MENU_BUY_EQUIPMENT); return 1; case '0': PerformAction(0, 0); return 1; default: return 0; }
        }
        if (keynum == '9') { PerformAction(0, CS_MENU_BUY); return 1; }
        if (keynum == '0') { PerformAction(0, 0); return 1; }
        if (keynum >= '1' && keynum <= '9')
        {
            const int index = keynum - '1';
            if (m_currentEntries && index < m_currentEntryCount) { PerformAction(m_currentEntries[index].command, 0); return 1; }
        }
        return 0;
    }

private:
    template <int N> static int Count(const BuyEntry (&)[N]) { return N; }
    CCSMenuPanel* AddPanel(CCSMenuPanel* panel)
    {
        panel->setParent(this); panel->setVisible(false);
        if (m_panelCount < (int)(sizeof(m_panels) / sizeof(m_panels[0]))) m_panels[m_panelCount++] = panel;
        return panel;
    }
    void HideAllPanels() { for (int i = 0; i < m_panelCount; ++i) m_panels[i]->setVisible(false); }
    int ShowCommandNode(int node, int page)
    {
        if (!m_commandMenu || !m_commandMenu->Refresh(node, page)) return 0;
        HideAllPanels(); LayoutMenus(); m_commandMenu->setVisible(true); m_commandMenu->requestFocus();
        m_currentMenu = CS_MENU_COMMAND; m_currentEntries = 0; m_currentEntryCount = 0;
        m_commandNode = node; m_commandPage = page; UpdateCursor(true); repaint(); return 1;
    }
    CCSMenuPanel* PanelForMenu(int menuId, const BuyEntry*& entries, int& count)
    {
        switch (menuId)
        {
        case CS_MENU_TEAM: return m_teamMenu; case CS_MENU_CLASS_T: return m_tClassMenu; case CS_MENU_CLASS_CT: return m_ctClassMenu; case CS_MENU_BUY: return m_buyRoot;
        case CS_MENU_BUY_PISTOL: entries = m_team == CS_TEAM_CT ? g_pistolsCT : g_pistolsT; count = m_team == CS_TEAM_CT ? Count(g_pistolsCT) : Count(g_pistolsT); return m_team == CS_TEAM_CT ? m_pistolsCT : m_pistolsT;
        case CS_MENU_BUY_SHOTGUN: entries = g_shotguns; count = Count(g_shotguns); return m_shotguns;
        case CS_MENU_BUY_RIFLE: entries = m_team == CS_TEAM_CT ? g_riflesCT : g_riflesT; count = m_team == CS_TEAM_CT ? Count(g_riflesCT) : Count(g_riflesT); return m_team == CS_TEAM_CT ? m_riflesCT : m_riflesT;
        case CS_MENU_BUY_SMG: entries = m_team == CS_TEAM_CT ? g_smgsCT : g_smgsT; count = m_team == CS_TEAM_CT ? Count(g_smgsCT) : Count(g_smgsT); return m_team == CS_TEAM_CT ? m_smgsCT : m_smgsT;
        case CS_MENU_BUY_MACHINEGUN: entries = g_machineGuns; count = Count(g_machineGuns); return m_machineGuns;
        case CS_MENU_BUY_EQUIPMENT: entries = m_team == CS_TEAM_CT ? g_equipmentCT : g_equipmentT; count = m_team == CS_TEAM_CT ? Count(g_equipmentCT) : Count(g_equipmentT); return m_team == CS_TEAM_CT ? m_equipmentCT : m_equipmentT;
        default: return 0;
        }
    }
    void LayoutMenus()
    {
        for (int i = 0; i < m_panelCount; ++i)
        {
            int wide = 0, tall = 0; m_panels[i]->getSize(wide, tall);
            if (m_panels[i] == m_commandMenu) { int y = (m_height - tall) / 3; if (y < 48) y = 48; m_panels[i]->setPos(20, y); }
            else m_panels[i]->setPos((m_width - wide) / 2, (m_height - tall) / 2);
        }
    }
    void UpdateCursor(bool visible)
    {
        CS16VGUI_SetMouseVisible(visible ? 1 : 0);
        vgui::App* app = vgui::App::getInstance(); if (!app || !app->getScheme()) return;
        app->setCursorOveride(app->getScheme()->getCursor(visible ? vgui::Scheme::scu_arrow : vgui::Scheme::scu_none));
    }

    int m_width, m_height, m_currentMenu, m_team;
    const BuyEntry* m_currentEntries;
    int m_currentEntryCount, m_commandNode, m_commandPage;
    CCSMenuPanel* m_panels[16]; int m_panelCount;
    CCSMenuPanel *m_teamMenu, *m_tClassMenu, *m_ctClassMenu, *m_buyRoot;
    CCSMenuPanel *m_pistolsT, *m_pistolsCT, *m_shotguns, *m_riflesT, *m_riflesCT;
    CCSMenuPanel *m_smgsT, *m_smgsCT, *m_machineGuns, *m_equipmentT, *m_equipmentCT;
    CCommandMenuPanel* m_commandMenu;
};

void CMenuActionSignal::actionPerformed(vgui::Panel*) { m_viewport->PerformAction(m_command, m_targetMenu); }
void CCommandSlotSignal::actionPerformed(vgui::Panel*) { m_viewport->PerformCommandSlot(m_slot); }
CCSViewport* g_viewport = 0;
}

extern "C" int CS16VGUI_ImplStartup(void* rootPanel, int width, int height)
{
    if (!rootPanel || width <= 0 || height <= 0) return 0;
    if (!g_viewport) g_viewport = new CCSViewport(width, height);
    g_viewport->Attach(static_cast<vgui::Panel*>(rootPanel), width, height); return 1;
}
extern "C" void CS16VGUI_ImplShutdown(void)
{
    if (!g_viewport) return; g_viewport->HideMenu(); g_viewport->setVisible(false); g_viewport->setParent(0);
}
extern "C" void CS16VGUI_ImplSetTeam(int team) { if (g_viewport) g_viewport->SetTeam(team); }
extern "C" int CS16VGUI_ImplShowMenu(int menuId) { return g_viewport ? g_viewport->ShowMenu(menuId) : 0; }
extern "C" int CS16VGUI_ImplShowCommandMenu(void) { return g_viewport ? g_viewport->ShowCommandMenu() : 0; }
extern "C" void CS16VGUI_ImplReleaseCommandMenu(void) { if (g_viewport) g_viewport->ReleaseCommandMenu(); }
extern "C" void CS16VGUI_ImplHideMenu(void) { if (g_viewport) g_viewport->HideMenu(); }
extern "C" int CS16VGUI_ImplKeyInput(int down, int keynum, const char*) { return g_viewport ? g_viewport->KeyInput(down, keynum) : 0; }

#endif
