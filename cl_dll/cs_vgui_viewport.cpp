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
    MAX_COMMAND_MENU_BUTTONS = 20
};

struct BuyEntry
{
    const char* label;
    const char* command;
};

static const BuyEntry g_pistolsT[] =
{
    { "1  GLOCK 18", "glock\n" },
    { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" },
    { "4  DESERT EAGLE", "deagle\n" },
    { "5  DUAL ELITES", "elites\n" }
};

static const BuyEntry g_pistolsCT[] =
{
    { "1  GLOCK 18", "glock\n" },
    { "2  USP .45", "usp\n" },
    { "3  P228", "p228\n" },
    { "4  DESERT EAGLE", "deagle\n" },
    { "5  FIVE-SEVEN", "fiveseven\n" }
};

static const BuyEntry g_shotguns[] =
{
    { "1  M3 SUPER 90", "m3\n" },
    { "2  XM1014", "xm1014\n" }
};

static const BuyEntry g_smgsT[] =
{
    { "1  MAC-10", "mac10\n" },
    { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" },
    { "4  P90", "p90\n" }
};

static const BuyEntry g_smgsCT[] =
{
    { "1  TMP", "tmp\n" },
    { "2  MP5 NAVY", "mp5\n" },
    { "3  UMP45", "ump45\n" },
    { "4  P90", "p90\n" }
};

static const BuyEntry g_riflesT[] =
{
    { "1  GALIL", "galil\n" },
    { "2  AK-47", "ak47\n" },
    { "3  SCOUT", "scout\n" },
    { "4  SG-552", "sg552\n" },
    { "5  AWP", "awp\n" },
    { "6  G3/SG-1", "g3sg1\n" }
};

static const BuyEntry g_riflesCT[] =
{
    { "1  FAMAS", "famas\n" },
    { "2  SCOUT", "scout\n" },
    { "3  M4A1", "m4a1\n" },
    { "4  AUG", "aug\n" },
    { "5  SG-550", "sg550\n" },
    { "6  AWP", "awp\n" }
};

static const BuyEntry g_machineGuns[] =
{
    { "1  M249", "m249\n" }
};

static const BuyEntry g_equipmentT[] =
{
    { "1  KEVLAR", "vest\n" },
    { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" },
    { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" },
    { "6  NIGHTVISION", "nvgs\n" }
};

static const BuyEntry g_equipmentCT[] =
{
    { "1  KEVLAR", "vest\n" },
    { "2  KEVLAR + HELMET", "vesthelm\n" },
    { "3  FLASHBANG", "flash\n" },
    { "4  HE GRENADE", "hegren\n" },
    { "5  SMOKE GRENADE", "sgren\n" },
    { "6  DEFUSE KIT", "defuser\n" },
    { "7  NIGHTVISION", "nvgs\n" },
    { "8  TACTICAL SHIELD", "shield\n" }
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

class CCSMenuButton final : public vgui::Button
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
        int wide = 0;
        int tall = 0;
        getPaintSize(wide, tall);

        if (isArmed())
            drawSetColor(88, 67, 26, 0);
        else if (isSelected())
            drawSetColor(66, 55, 28, 0);
        else
            drawSetColor(27, 34, 28, 0);

        drawFilledRect(0, 0, wide, tall);
        drawSetColor(isArmed() ? 224 : 145, isArmed() ? 160 : 103, 30, 0);
        drawOutlinedRect(0, 0, wide, tall);
    }
};

class CCSMenuPanel : public vgui::Panel
{
public:
    CCSMenuPanel(const char* titleText, int height)
        : vgui::Panel(0, 0, 510, height)
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
        int wide = 0;
        int tall = 0;
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
        : CCSMenuPanel(
            menuId == CS_MENU_TEAM ? "CHOOSE A TEAM" :
            menuId == CS_MENU_CLASS_T ? "CHOOSE A TERRORIST" :
            "CHOOSE A COUNTER-TERRORIST",
            menuId == CS_MENU_TEAM ? 350 : 390)
    {
        if (menuId == CS_MENU_TEAM)
        {
            AddButton(viewport, "1  TERRORIST FORCES", 72, "jointeam 1\n", 0, 38);
            AddButton(viewport, "2  CT FORCES", 124, "jointeam 2\n", 0, 38);
            AddButton(viewport, "5  AUTO-SELECT", 176, "jointeam 5\n", 0, 38);
            AddButton(viewport, "6  SPECTATE", 228, "jointeam 6\n", 0, 38);
            AddButton(viewport, "0  CANCEL", 286, 0, 0, 38);
        }
        else if (menuId == CS_MENU_CLASS_T)
        {
            AddButton(viewport, "1  PHOENIX CONNEXION", 66, "joinclass 1\n", 0, 38);
            AddButton(viewport, "2  L337 KREW", 116, "joinclass 2\n", 0, 38);
            AddButton(viewport, "3  ARCTIC AVENGERS", 166, "joinclass 3\n", 0, 38);
            AddButton(viewport, "4  GUERILLA WARFARE", 216, "joinclass 4\n", 0, 38);
            AddButton(viewport, "5  AUTO-SELECT", 266, "joinclass 5\n", 0, 38);
            AddButton(viewport, "0  CANCEL", 326, 0, 0, 38);
        }
        else
        {
            AddButton(viewport, "1  SEAL TEAM 6", 66, "joinclass 1\n", 0, 38);
            AddButton(viewport, "2  GSG-9", 116, "joinclass 2\n", 0, 38);
            AddButton(viewport, "3  SAS", 166, "joinclass 3\n", 0, 38);
            AddButton(viewport, "4  GIGN", 216, "joinclass 4\n", 0, 38);
            AddButton(viewport, "5  AUTO-SELECT", 266, "joinclass 5\n", 0, 38);
            AddButton(viewport, "0  CANCEL", 326, 0, 0, 38);
        }
    }
};

class CBuyRootPanel final : public CCSMenuPanel
{
public:
    CBuyRootPanel(CCSViewport* viewport)
        : CCSMenuPanel("BUY MENU", 436)
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
    CCommandMenuPanel(CCSViewport* viewport)
        : CCSMenuPanel("COMMAND MENU", 90)
    {
        for (int i = 0; i < MAX_COMMAND_MENU_BUTTONS; ++i)
        {
            CCSMenuButton* button = new CCSMenuButton("", 20, 48 + i * 30, 320, 27);
            button->setParent(this);
            button->setContentAlignment(vgui::Label::a_west);
            button->setFgColor(230, 230, 220, 0);
            button->addActionSignal(new CCommandSlotSignal(viewport, i));
            button->setVisible(false);
            m_buttons[i] = button;
        }
    }

    bool Refresh(int node)
    {
        const int count = CS16VGUI_CommandMenuGetCount(node);
        const int parent = CS16VGUI_CommandMenuGetParent(node);
        int rows = count;
        if (rows > MAX_COMMAND_MENU_BUTTONS)
            rows = MAX_COMMAND_MENU_BUTTONS;

        for (int i = 0; i < rows; ++i)
        {
            const char* display = 0;
            if (!CS16VGUI_CommandMenuGetItem(node, i, &display, 0, 0, 0))
            {
                m_buttons[i]->setVisible(false);
                continue;
            }

            m_buttons[i]->setText(112, display ? display : "");
            m_buttons[i]->setVisible(true);
        }

        if (parent >= 0 && rows < MAX_COMMAND_MENU_BUTTONS)
        {
            m_buttons[rows]->setText(112, "0  BACK");
            m_buttons[rows]->setVisible(true);
            ++rows;
        }

        for (int i = rows; i < MAX_COMMAND_MENU_BUTTONS; ++i)
            m_buttons[i]->setVisible(false);

        setSize(360, 58 + rows * 30);
        return count > 0 || parent >= 0;
    }

private:
    CCSMenuButton* m_buttons[MAX_COMMAND_MENU_BUTTONS];
};

class CCSViewport final : public vgui::Panel
{
public:
    CCSViewport(int width, int height)
        : vgui::Panel(0, 0, width, height),
          m_width(width), m_height(height), m_currentMenu(0), m_team(CS_TEAM_T),
          m_currentEntries(0), m_currentEntryCount(0), m_commandNode(0), m_panelCount(0),
          m_commandMenu(0)
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
        m_width = width;
        m_height = height;
        setBounds(0, 0, width, height);
        setParent(root);
        setVisible(true);
        LayoutMenus();
        HideMenu();
    }

    void SetTeam(int team)
    {
        if (team == CS_TEAM_T || team == CS_TEAM_CT)
            m_team = team;
    }

    int ShowMenu(int menuId)
    {
        const BuyEntry* entries = 0;
        int entryCount = 0;
        CCSMenuPanel* panel = PanelForMenu(menuId, entries, entryCount);
        if (!panel)
            return 0;

        HideAllPanels();
        panel->setVisible(true);
        panel->requestFocus();
        m_currentMenu = menuId;
        m_currentEntries = entries;
        m_currentEntryCount = entryCount;
        UpdateCursor(true);
        repaint();
        return 1;
    }

    int ShowCommandMenu()
    {
        if (m_currentMenu == CS_MENU_COMMAND)
        {
            HideMenu();
            return 0;
        }

        if (!CS16VGUI_CommandMenuPrepare())
            return 0;

        return ShowCommandNode(0);
    }

    void ReleaseCommandMenu()
    {
        if (m_currentMenu == CS_MENU_COMMAND)
            HideMenu();
    }

    void HideMenu()
    {
        HideAllPanels();
        m_currentMenu = 0;
        m_currentEntries = 0;
        m_currentEntryCount = 0;
        m_commandNode = 0;
        UpdateCursor(false);
    }

    void PerformAction(const char* command, int targetMenu)
    {
        if (targetMenu)
        {
            ShowMenu(targetMenu);
            return;
        }

        HideMenu();
        if (command)
            CS16VGUI_ClientCommand(command);
    }

    void PerformCommandSlot(int slot)
    {
        if (m_currentMenu != CS_MENU_COMMAND || slot < 0)
            return;

        const int count = CS16VGUI_CommandMenuGetCount(m_commandNode);
        const int parent = CS16VGUI_CommandMenuGetParent(m_commandNode);
        if (slot >= count)
        {
            if (parent >= 0)
                ShowCommandNode(parent);
            return;
        }

        int itemIndex = -1;
        int childNode = -1;
        if (!CS16VGUI_CommandMenuGetItem(m_commandNode, slot, 0,
            &itemIndex, &childNode, 0))
            return;

        if (childNode >= 0)
        {
            ShowCommandNode(childNode);
            return;
        }

        HideMenu();
        CS16VGUI_CommandMenuExecute(itemIndex);
    }

    int KeyInput(int down, int keynum)
    {
        if (!m_currentMenu || !down)
            return 0;

        if (keynum == GOLDSRC_KEY_ESCAPE)
        {
            PerformAction(0, 0);
            return 1;
        }

        if (m_currentMenu == CS_MENU_COMMAND)
        {
            const int count = CS16VGUI_CommandMenuGetCount(m_commandNode);
            for (int i = 0; i < count; ++i)
            {
                int boundKey = 0;
                if (CS16VGUI_CommandMenuGetItem(m_commandNode, i, 0, 0, 0, &boundKey) &&
                    keynum == boundKey)
                {
                    PerformCommandSlot(i);
                    return 1;
                }
            }

            if (keynum == '0' && CS16VGUI_CommandMenuGetParent(m_commandNode) >= 0)
            {
                ShowCommandNode(CS16VGUI_CommandMenuGetParent(m_commandNode));
                return 1;
            }

            return keynum >= '0' && keynum <= '9';
        }

        if (m_currentMenu == CS_MENU_TEAM)
        {
            switch (keynum)
            {
            case '1': PerformAction("jointeam 1\n", 0); return 1;
            case '2': PerformAction("jointeam 2\n", 0); return 1;
            case '5': PerformAction("jointeam 5\n", 0); return 1;
            case '6': PerformAction("jointeam 6\n", 0); return 1;
            case '0': PerformAction(0, 0); return 1;
            default: return 0;
            }
        }

        if (m_currentMenu == CS_MENU_CLASS_T || m_currentMenu == CS_MENU_CLASS_CT)
        {
            if (keynum >= '1' && keynum <= '5')
            {
                static const char* commands[] =
                {
                    "joinclass 1\n", "joinclass 2\n", "joinclass 3\n",
                    "joinclass 4\n", "joinclass 5\n"
                };
                PerformAction(commands[keynum - '1'], 0);
                return 1;
            }
            if (keynum == '0')
            {
                PerformAction(0, 0);
                return 1;
            }
            return 0;
        }

        if (m_currentMenu == CS_MENU_BUY)
        {
            switch (keynum)
            {
            case '1': PerformAction(0, CS_MENU_BUY_PISTOL); return 1;
            case '2': PerformAction(0, CS_MENU_BUY_SHOTGUN); return 1;
            case '3': PerformAction(0, CS_MENU_BUY_SMG); return 1;
            case '4': PerformAction(0, CS_MENU_BUY_RIFLE); return 1;
            case '5': PerformAction(0, CS_MENU_BUY_MACHINEGUN); return 1;
            case '6': PerformAction("primammo\n", 0); return 1;
            case '7': PerformAction("secammo\n", 0); return 1;
            case '8': PerformAction(0, CS_MENU_BUY_EQUIPMENT); return 1;
            case '0': PerformAction(0, 0); return 1;
            default: return 0;
            }
        }

        if (keynum == '9')
        {
            PerformAction(0, CS_MENU_BUY);
            return 1;
        }
        if (keynum == '0')
        {
            PerformAction(0, 0);
            return 1;
        }
        if (keynum >= '1' && keynum <= '9')
        {
            const int index = keynum - '1';
            if (m_currentEntries && index < m_currentEntryCount)
            {
                PerformAction(m_currentEntries[index].command, 0);
                return 1;
            }
        }

        return 0;
    }

private:
    template <int N> static int Count(const BuyEntry (&)[N]) { return N; }

    CCSMenuPanel* AddPanel(CCSMenuPanel* panel)
    {
        panel->setParent(this);
        panel->setVisible(false);
        if (m_panelCount < (int)(sizeof(m_panels) / sizeof(m_panels[0])))
            m_panels[m_panelCount++] = panel;
        return panel;
    }

    void HideAllPanels()
    {
        for (int i = 0; i < m_panelCount; ++i)
            m_panels[i]->setVisible(false);
    }

    int ShowCommandNode(int node)
    {
        if (!m_commandMenu || !m_commandMenu->Refresh(node))
            return 0;

        HideAllPanels();
        LayoutMenus();
        m_commandMenu->setVisible(true);
        m_commandMenu->requestFocus();
        m_currentMenu = CS_MENU_COMMAND;
        m_currentEntries = 0;
        m_currentEntryCount = 0;
        m_commandNode = node;
        UpdateCursor(true);
        repaint();
        return 1;
    }

    CCSMenuPanel* PanelForMenu(int menuId, const BuyEntry*& entries, int& count)
    {
        switch (menuId)
        {
        case CS_MENU_TEAM: return m_teamMenu;
        case CS_MENU_CLASS_T: return m_tClassMenu;
        case CS_MENU_CLASS_CT: return m_ctClassMenu;
        case CS_MENU_BUY: return m_buyRoot;
        case CS_MENU_BUY_PISTOL:
            entries = m_team == CS_TEAM_CT ? g_pistolsCT : g_pistolsT;
            count = m_team == CS_TEAM_CT ? Count(g_pistolsCT) : Count(g_pistolsT);
            return m_team == CS_TEAM_CT ? m_pistolsCT : m_pistolsT;
        case CS_MENU_BUY_SHOTGUN:
            entries = g_shotguns; count = Count(g_shotguns); return m_shotguns;
        case CS_MENU_BUY_RIFLE:
            entries = m_team == CS_TEAM_CT ? g_riflesCT : g_riflesT;
            count = m_team == CS_TEAM_CT ? Count(g_riflesCT) : Count(g_riflesT);
            return m_team == CS_TEAM_CT ? m_riflesCT : m_riflesT;
        case CS_MENU_BUY_SMG:
            entries = m_team == CS_TEAM_CT ? g_smgsCT : g_smgsT;
            count = m_team == CS_TEAM_CT ? Count(g_smgsCT) : Count(g_smgsT);
            return m_team == CS_TEAM_CT ? m_smgsCT : m_smgsT;
        case CS_MENU_BUY_MACHINEGUN:
            entries = g_machineGuns; count = Count(g_machineGuns); return m_machineGuns;
        case CS_MENU_BUY_EQUIPMENT:
            entries = m_team == CS_TEAM_CT ? g_equipmentCT : g_equipmentT;
            count = m_team == CS_TEAM_CT ? Count(g_equipmentCT) : Count(g_equipmentT);
            return m_team == CS_TEAM_CT ? m_equipmentCT : m_equipmentT;
        default: return 0;
        }
    }

    void LayoutMenus()
    {
        for (int i = 0; i < m_panelCount; ++i)
        {
            int wide = 0;
            int tall = 0;
            m_panels[i]->getSize(wide, tall);
            if (m_panels[i] == m_commandMenu)
            {
                int y = (m_height - tall) / 3;
                if (y < 48)
                    y = 48;
                m_panels[i]->setPos(20, y);
            }
            else
            {
                m_panels[i]->setPos((m_width - wide) / 2, (m_height - tall) / 2);
            }
        }
    }

    void UpdateCursor(bool visible)
    {
        CS16VGUI_SetMouseVisible(visible ? 1 : 0);
        vgui::App* app = vgui::App::getInstance();
        if (!app || !app->getScheme())
            return;

        const vgui::Scheme::SchemeCursor cursor =
            visible ? vgui::Scheme::scu_arrow : vgui::Scheme::scu_none;
        app->setCursorOveride(app->getScheme()->getCursor(cursor));
    }

    int m_width;
    int m_height;
    int m_currentMenu;
    int m_team;
    const BuyEntry* m_currentEntries;
    int m_currentEntryCount;
    int m_commandNode;
    CCSMenuPanel* m_panels[16];
    int m_panelCount;
    CCSMenuPanel* m_teamMenu;
    CCSMenuPanel* m_tClassMenu;
    CCSMenuPanel* m_ctClassMenu;
    CCSMenuPanel* m_buyRoot;
    CCSMenuPanel* m_pistolsT;
    CCSMenuPanel* m_pistolsCT;
    CCSMenuPanel* m_shotguns;
    CCSMenuPanel* m_riflesT;
    CCSMenuPanel* m_riflesCT;
    CCSMenuPanel* m_smgsT;
    CCSMenuPanel* m_smgsCT;
    CCSMenuPanel* m_machineGuns;
    CCSMenuPanel* m_equipmentT;
    CCSMenuPanel* m_equipmentCT;
    CCommandMenuPanel* m_commandMenu;
};

void CMenuActionSignal::actionPerformed(vgui::Panel*)
{
    m_viewport->PerformAction(m_command, m_targetMenu);
}

void CCommandSlotSignal::actionPerformed(vgui::Panel*)
{
    m_viewport->PerformCommandSlot(m_slot);
}

CCSViewport* g_viewport = 0;
}

extern "C" int CS16VGUI_ImplStartup(void* rootPanel, int width, int height)
{
    if (!rootPanel || width <= 0 || height <= 0)
        return 0;

    if (!g_viewport)
        g_viewport = new CCSViewport(width, height);

    g_viewport->Attach(static_cast<vgui::Panel*>(rootPanel), width, height);
    return 1;
}

extern "C" void CS16VGUI_ImplShutdown(void)
{
    if (!g_viewport)
        return;

    g_viewport->HideMenu();
    g_viewport->setVisible(false);
    g_viewport->setParent(0);
}

extern "C" void CS16VGUI_ImplSetTeam(int team)
{
    if (g_viewport)
        g_viewport->SetTeam(team);
}

extern "C" int CS16VGUI_ImplShowMenu(int menuId)
{
    return g_viewport ? g_viewport->ShowMenu(menuId) : 0;
}

extern "C" int CS16VGUI_ImplShowCommandMenu(void)
{
    return g_viewport ? g_viewport->ShowCommandMenu() : 0;
}

extern "C" void CS16VGUI_ImplReleaseCommandMenu(void)
{
    if (g_viewport)
        g_viewport->ReleaseCommandMenu();
}

extern "C" void CS16VGUI_ImplHideMenu(void)
{
    if (g_viewport)
        g_viewport->HideMenu();
}

extern "C" int CS16VGUI_ImplKeyInput(int down, int keynum, const char*)
{
    return g_viewport ? g_viewport->KeyInput(down, keynum) : 0;
}

#endif
