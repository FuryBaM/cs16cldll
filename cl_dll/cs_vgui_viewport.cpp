// Minimal Counter-Strike VGUI1 viewport for ordinary Steam GoldSrc.
//
// This translation unit uses the Microsoft x86 C++ ABI because it derives
// from classes implemented by vgui.dll. Communication with the rest of
// client.dll stays behind a C ABI.

#if defined(_CS16CLIENT_ENABLE_VGUI1)

#include <VGUI_ActionSignal.h>
#include <VGUI_App.h>
#include <VGUI_Button.h>
#include <VGUI_Font.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_InputSignal.h>
#include <VGUI_Label.h>
#include <VGUI_Panel.h>
#include <VGUI_Scheme.h>
#include "cs_vgui.h"
#include "vgui_loadtga.h"
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
extern "C" void CS16VGUI_PaintBackground(int extents[4]);
extern "C" float CS16VGUI_GetLayoutScale(void);
extern "C" int CS16VGUI_CommandMenuPrepare(void);
extern "C" int CS16VGUI_CommandMenuGetCount(int node);
extern "C" int CS16VGUI_CommandMenuGetParent(int node);
extern "C" int CS16VGUI_CommandMenuGetItem(int node, int visibleIndex,
    const char** displayText, int* itemIndex, int* childNode, int* boundKey);
extern "C" void CS16VGUI_CommandMenuExecute(int itemIndex);

namespace
{
static int LayoutValue(int value)
{
    const float scale = CS16VGUI_GetLayoutScale();
    const int result = (int)(value * scale + 0.5f);
    return value > 0 && result < 1 ? 1 : result;
}

static vgui::Font* MenuButtonFont()
{
    // The stock App scheme's sf_primary3 is the compact command-menu font.
    // At a widescreen HUD scale it becomes much smaller than the 20-pixel
    // button rows described by the Counter-Strike VGUI2 .res files.
    static vgui::Font* font = new vgui::Font("Tahoma", LayoutValue(18), 0,
        0.0f, 500, false, false, false, false);
    return font;
}

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
    MAX_COMMAND_MENU_BUTTONS = 32,
    MAX_COMMAND_MENU_DEPTH = 8
};

// Match the visual language used by Velaron/cs16-client's active HUD UI:
// translucent black surfaces, the classic Counter-Strike orange accent and
// a light accent wash for hover/selection. VGUI1 stores transparency (0 is
// opaque, 255 is invisible), so these alpha values are the inverse of the
// regular HUD FillRGBABlend values.
enum
{
    VELARON_ACCENT_R = 255,
    VELARON_ACCENT_G = 140,
    VELARON_ACCENT_B = 0,
    VELARON_PANEL_TRANSPARENCY = 102,
    VELARON_BUTTON_TRANSPARENCY = 128,
    VELARON_HOVER_TRANSPARENCY = 207,
    VELARON_SELECTED_TRANSPARENCY = 176
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

class CPreviewImagePanel final : public vgui::ImagePanel
{
public:
    CPreviewImagePanel() : m_currentImage(NULL)
    {
        // The parent menu already supplies the translucent surface. Painting
        // an ImagePanel background creates a second opaque-looking rectangle.
        setPaintBackgroundEnabled(true);
        setVisible(false);
    }

    void ShowImage(vgui::BitmapTGA* image)
    {
        if (!image)
            return;

        if (m_currentImage == image && isVisible())
            return;

        int panelWide = 0, panelTall = 0;
        int imageWide = 0, imageTall = 0;
        getPaintSize(panelWide, panelTall);
        image->getSize(imageWide, imageTall);

        // ImagePanel::setImage associates the image with the panel and may
        // reset its origin. Position it afterwards, centered at its native
        // TGA size inside the ClassInfo/ItemInfo bounds from the .res file.
        setImage(image);
        int imageX = (panelWide - imageWide) / 2;
        int imageY = (panelTall - imageTall) / 2;
        if (imageX < 0) imageX = 0;
        if (imageY < 0) imageY = 0;
        image->setPos(imageX, imageY);
        m_currentImage = image;
        setVisible(true);
        repaint();
    }

protected:
    void paintBackground() override
    {
        // Steam's VGUI1 ImagePanel implementation does not reliably paint an
        // image assigned after the panel has been attached to this custom
        // viewport.  Paint the BitmapTGA explicitly; this is the same path
        // used by the stock VGUI1 custom controls and keeps the image origin
        // calculated above relative to the ClassInfo/ItemInfo .res bounds.
        if (m_currentImage)
            m_currentImage->doPaint(this);
    }

private:
    vgui::BitmapTGA* m_currentImage;
};

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
    CCommandSlotSignal(CCSViewport* viewport, int depth, int slot)
        : m_viewport(viewport), m_depth(depth), m_slot(slot) {}
    void actionPerformed(vgui::Panel*) override;
private:
    CCSViewport* m_viewport;
    int m_depth;
    int m_slot;
};

class CCSMenuButton : public vgui::Button, public vgui::InputSignal
{
public:
    CCSMenuButton(const char* text, int x, int y, int wide, int tall)
        : vgui::Button(text, x, y, wide, tall), m_previewPanel(NULL),
          m_previewImage(NULL), m_hovered(false)
    {
        m_previewPath[0] = '\0';
        setButtonBorderEnabled(false);
        setPaintBackgroundEnabled(true);
        addInputSignal(this);
    }

    void SetPreview(CPreviewImagePanel* panel, const char* imageName)
    {
        m_previewPanel = panel;
        m_previewImage = NULL;
        m_previewPath[0] = '\0';
        if (!panel || !imageName || !imageName[0])
            return;

        _snprintf(m_previewPath, sizeof(m_previewPath), "gfx/vgui/%s.tga", imageName);
        m_previewPath[sizeof(m_previewPath) - 1] = '\0';
    }

    bool HasPreview() const
    {
        return m_previewPanel && m_previewPath[0];
    }

    void ShowPreview()
    {
        if (!HasPreview())
            return;

        if (!m_previewImage)
            m_previewImage = vgui_LoadTGA(m_previewPath);
        if (m_previewImage)
            m_previewPanel->ShowImage(m_previewImage);
    }

    void cursorMoved(int, int, vgui::Panel*) override
    {
        // Some Steam VGUI1 builds do not arm custom Button subclasses from
        // their internal controller. Mouse movement is a reliable fallback.
        EnterHover();
    }

    void cursorEntered(vgui::Panel*) override
    {
        EnterHover();
    }

    void cursorExited(vgui::Panel*) override
    {
        if (!m_hovered)
            return;
        m_hovered = false;
        setArmed(false);
        repaint();
    }

    void mousePressed(vgui::MouseCode, vgui::Panel*) override {}
    void mouseDoublePressed(vgui::MouseCode, vgui::Panel*) override {}
    void mouseReleased(vgui::MouseCode, vgui::Panel*) override {}
    void mouseWheeled(int, vgui::Panel*) override {}
    void keyPressed(vgui::KeyCode, vgui::Panel*) override {}
    void keyTyped(vgui::KeyCode, vgui::Panel*) override {}
    void keyReleased(vgui::KeyCode, vgui::Panel*) override {}
    void keyFocusTicked(vgui::Panel*) override {}

protected:
    void paintBackground() override
    {
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);

        if (m_hovered || isArmed())
            drawSetColor(VELARON_ACCENT_R, VELARON_ACCENT_G,
                VELARON_ACCENT_B, VELARON_HOVER_TRANSPARENCY);
        else if (isSelected())
            drawSetColor(VELARON_ACCENT_R, VELARON_ACCENT_G,
                VELARON_ACCENT_B, VELARON_SELECTED_TRANSPARENCY);
        else
            drawSetColor(0, 0, 0, 255);

        drawFilledRect(0, 0, wide, tall);
    }

private:
    void EnterHover()
    {
        if (m_hovered)
            return;
        m_hovered = true;
        setArmed(true);
        ShowPreview();
        repaint();
    }

    CPreviewImagePanel* m_previewPanel;
    vgui::BitmapTGA* m_previewImage;
    bool m_hovered;
    char m_previewPath[128];
};

static void RemoveButtonAccelerators(char* text)
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

static const char* PreviewImageName(const char* fieldName)
{
    if (!fieldName || !fieldName[0] || !stricmp(fieldName, "CancelButton"))
        return NULL;
    if (!stricmp(fieldName, "autoselect_t") || !stricmp(fieldName, "militia"))
        return "t_random";
    if (!stricmp(fieldName, "autoselect_ct") || !stricmp(fieldName, "spetsnaz"))
        return "ct_random";
    return fieldName;
}

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
    CUnicodeCommandButton(CCSViewport* viewport, int depth, int slot,
        int x, int y, int wide, int tall)
        : CCSMenuButton("", x, y, wide, tall), m_viewport(viewport),
          m_depth(depth), m_slot(slot), m_texture(++g_nextCommandTexture),
          m_textureWidth(0), m_textureHeight(0), m_hasTexture(false),
          m_opensSubmenu(false)
    {
        m_text[0] = 0;
    }

    void SetUtf8Text(const char* text)
    {
        Utf8ToWide(text, m_text, (int)(sizeof(m_text) / sizeof(m_text[0])));
        BuildTexture();
        repaint();
    }

    void SetOpensSubmenu(bool opens) { m_opensSubmenu = opens; }

    void cursorEntered(vgui::Panel* panel) override;

protected:
    void paint() override
    {
        if (!m_hasTexture) return;
        drawSetColor(255, 255, 255, 0);
        drawSetTexture(m_texture);
        drawTexturedRect(4, 3, 4 + m_textureWidth, 3 + m_textureHeight);
    }

    void paintBackground() override
    {
        CCSMenuButton::paintBackground();
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);
        drawSetColor(VELARON_ACCENT_R, VELARON_ACCENT_G,
            VELARON_ACCENT_B, 128);
        drawOutlinedRect(0, 0, wide, tall);
    }

private:
    CCSViewport* m_viewport;
    int m_depth;
    int m_slot;
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
        HFONT font = CreateFontW(-LayoutValue(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
        HGDIOBJ oldFont = font ? SelectObject(dc, font) : NULL;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(VELARON_ACCENT_R, VELARON_ACCENT_G, VELARON_ACCENT_B));
        RECT rect = { 0, 0, wide, tall };
        DrawTextW(dc, m_text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        unsigned char* rgba = new unsigned char[wide * tall * 4];
        unsigned char* bgra = static_cast<unsigned char*>(pixels);
        for (int i = 0; i < wide * tall; ++i)
        {
            const unsigned char alpha = bgra[i * 4 + 2];
            rgba[i * 4 + 0] = VELARON_ACCENT_R;
            rgba[i * 4 + 1] = VELARON_ACCENT_G;
            rgba[i * 4 + 2] = VELARON_ACCENT_B;
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
    bool m_opensSubmenu;
};

class CCSMenuPanel : public vgui::Panel
{
public:
    CCSMenuPanel(const char* titleText, int height, int width = 510)
        : vgui::Panel(0, 0, LayoutValue(width), LayoutValue(height)), m_buttonCount(0),
          m_decorationCount(0), m_titleBottom(50), m_resourceLayout(false),
          m_resourceXpos(0), m_resourceYpos(0), m_hasPreviewPanel(false)
    {
        setPaintBackgroundEnabled(true);
        m_preview = new CPreviewImagePanel();
        m_preview->setParent(this);
        m_title = new vgui::Label(titleText, LayoutValue(20), LayoutValue(14),
            LayoutValue(width - 40), LayoutValue(36));
        m_title->setParent(this);
        m_title->setContentAlignment(vgui::Label::a_center);
        m_title->setFgColor(VELARON_ACCENT_R, VELARON_ACCENT_G, VELARON_ACCENT_B, 0);
        m_title->setPaintBackgroundEnabled(false);
    }

    bool GetResourcePosition(int& x, int& y) const
    {
        if (!m_resourceLayout)
            return false;
        x = m_resourceXpos;
        y = m_resourceYpos;
        return true;
    }

protected:
    void HideTitle()
    {
        m_title->setVisible(false);
    }

    void AddButton(CCSViewport* viewport, const char* text, int y,
        const char* command, int targetMenu, int tall = 30)
    {
        vgui::Button* button = new CCSMenuButton(text, LayoutValue(40),
            LayoutValue(y), LayoutValue(430), LayoutValue(tall));
        button->setParent(this);
        button->setContentAlignment(vgui::Label::a_west);
        button->setFont(MenuButtonFont());
        button->setFgColor(VELARON_ACCENT_R, VELARON_ACCENT_G, VELARON_ACCENT_B, 0);
        button->addActionSignal(new CMenuActionSignal(viewport, command, targetMenu));
        if (m_buttonCount < (int)(sizeof(m_buttons) / sizeof(m_buttons[0])))
            m_buttons[m_buttonCount++] = button;
    }

    bool LoadResource(const char* filename)
    {
        cs16_vgui_resource_control_t controls[CS16_VGUI_MAX_RESOURCE_CONTROLS];
        const int controlCount = CS16VGUI_LoadResourceLayout(filename, controls,
            CS16_VGUI_MAX_RESOURCE_CONTROLS);
        if (controlCount <= 0)
            return false;

        int resourceButton = 0;
        bool titleApplied = false;
        m_decorationCount = 0;
        m_hasPreviewPanel = false;

        // The stock MouseOverPanelButton does not contain the picture itself:
        // it targets ClassInfo/ItemInfo and loads gfx/vgui/<fieldName>.tga.
        // Locate that target before assigning buttons, regardless of ordering
        // in the .res file (ClassInfo is deliberately marked invisible).
        int panelWide = 0, panelTall = 0;
        getSize(panelWide, panelTall);
        for (int i = 0; i < controlCount; ++i)
        {
            const cs16_vgui_resource_control_t& control = controls[i];
            const bool isPreviewPanel = !stricmp(control.controlName, "Panel") &&
                (!stricmp(control.fieldName, "ClassInfo") ||
                 !stricmp(control.fieldName, "ItemInfo"));
            if (isPreviewPanel && control.wide > 0 && control.tall > 0 &&
                control.xpos >= 0 && control.ypos >= 0 &&
                control.xpos < panelWide && control.ypos < panelTall)
            {
                m_preview->setBounds(LayoutValue(control.xpos), LayoutValue(control.ypos),
                    LayoutValue(control.wide), LayoutValue(control.tall));
                m_preview->setVisible(false);
                m_hasPreviewPanel = true;
                break;
            }
        }

        CCSMenuButton* firstPreview = NULL;

        for (int i = 0; i < controlCount; ++i)
        {
            cs16_vgui_resource_control_t& control = controls[i];
            const bool isRoot = !stricmp(control.controlName, "Frame") ||
                !stricmp(control.controlName, "WizardSubPanel");
            if (isRoot && control.wide > 0 && control.tall > 0)
            {
                setSize(LayoutValue(control.wide), LayoutValue(control.tall));
                m_resourceXpos = LayoutValue(control.xpos);
                m_resourceYpos = LayoutValue(control.ypos);
                continue;
            }

            const bool isButton = !stricmp(control.controlName, "Button") ||
                !stricmp(control.controlName, "MouseOverPanelButton");
            if (isButton)
            {
                if (resourceButton >= m_buttonCount)
                    continue;

                vgui::Button* button = m_buttons[resourceButton++];
                if (control.wide > 0 && control.tall > 0)
                    button->setBounds(LayoutValue(control.xpos), LayoutValue(control.ypos),
                        LayoutValue(control.wide), LayoutValue(control.tall));

                if (control.labelText[0])
                {
                    char text[256];
                    CS16VGUI_LocalizeResourceText(control.labelText, text, sizeof(text));
                    RemoveButtonAccelerators(text);
                    button->setText(sizeof(text), text);
                }

                button->setContentAlignment(!stricmp(control.textAlignment, "center")
                    ? vgui::Label::a_center : vgui::Label::a_west);
                button->setVisible(control.visible != 0);
                button->setEnabled(control.enabled != 0);
                if (!stricmp(control.controlName, "MouseOverPanelButton") &&
                    m_hasPreviewPanel)
                {
                    CCSMenuButton* menuButton = static_cast<CCSMenuButton*>(button);
                    menuButton->SetPreview(m_preview, PreviewImageName(control.fieldName));
                    if (!firstPreview && menuButton->HasPreview())
                        firstPreview = menuButton;
                }
                continue;
            }

            if (!stricmp(control.controlName, "Label") && !titleApplied &&
                (!stricmp(control.font, "Title") || !stricmp(control.fieldName, "Title") ||
                 !stricmp(control.fieldName, "joinTeam") || !stricmp(control.fieldName, "joinClass")))
            {
                if (control.wide > 0 && control.tall > 0)
                    m_title->setBounds(LayoutValue(control.xpos), LayoutValue(control.ypos),
                        LayoutValue(control.wide), LayoutValue(control.tall));
                if (control.labelText[0])
                {
                    char text[256];
                    CS16VGUI_LocalizeResourceText(control.labelText, text, sizeof(text));
                    m_title->setText(sizeof(text), text);
                }
                m_title->setContentAlignment(!stricmp(control.textAlignment, "center")
                    ? vgui::Label::a_center : vgui::Label::a_west);
                m_titleBottom = LayoutValue(control.ypos + control.tall);
                titleApplied = true;
                continue;
            }

            const bool isDivider = !stricmp(control.controlName, "Divider");
            if (isDivider && control.visible && control.wide > 0 &&
                control.tall > 0 && m_decorationCount < (int)(sizeof(m_decorations) / sizeof(m_decorations[0])))
            {
                Decoration& decoration = m_decorations[m_decorationCount++];
                decoration.x = LayoutValue(control.xpos);
                decoration.y = LayoutValue(control.ypos);
                decoration.wide = LayoutValue(control.wide);
                decoration.tall = LayoutValue(control.tall);
                decoration.divider = isDivider;
            }
        }

        // Buttons present only in the fallback layout (for example the old
        // explicit Back button) must not leak into a resource-driven panel.
        for (int i = resourceButton; i < m_buttonCount; ++i)
            m_buttons[i]->setVisible(false);

        if (firstPreview)
            firstPreview->ShowPreview();

        m_resourceLayout = true;
        return true;
    }
    void paintBackground() override
    {
        int wide = 0, tall = 0;
        getPaintSize(wide, tall);
        drawSetColor(0, 0, 0, VELARON_PANEL_TRANSPARENCY);
        drawFilledRect(0, 0, wide, tall);

        drawSetColor(VELARON_ACCENT_R, VELARON_ACCENT_G, VELARON_ACCENT_B, 0);
        for (int i = 0; i < m_decorationCount; ++i)
        {
            const Decoration& decoration = m_decorations[i];
            drawFilledRect(decoration.x, decoration.y,
                decoration.x + decoration.wide, decoration.y + decoration.tall);
        }
    }

private:
    struct Decoration
    {
        int x, y, wide, tall;
        bool divider;
    };

    vgui::Label* m_title;
    vgui::Button* m_buttons[16];
    int m_buttonCount;
    Decoration m_decorations[8];
    int m_decorationCount;
    int m_titleBottom;
    bool m_resourceLayout;
    int m_resourceXpos;
    int m_resourceYpos;
    CPreviewImagePanel* m_preview;
    bool m_hasPreviewPanel;
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
            AddButton(viewport, "3  VIP", 176, "jointeam 3\n", 0, 38);
            AddButton(viewport, "5  AUTO-SELECT", 228, "jointeam 5\n", 0, 38);
            AddButton(viewport, "6  SPECTATE", 280, "jointeam 6\n", 0, 38);
            AddButton(viewport, "0  CANCEL", 332, 0, 0, 38);
            LoadResource("resource/UI/Teammenu.res");
        }
        else
        {
            const char* namesT[] = { "1  PHOENIX CONNEXION", "2  L337 KREW", "3  ARCTIC AVENGERS", "4  GUERILLA WARFARE", "5  MILITIA", "6  AUTO-SELECT" };
            const char* namesCT[] = { "1  SEAL TEAM 6", "2  GSG-9", "3  SAS", "4  GIGN", "5  SPETSNAZ", "6  AUTO-SELECT" };
            const char** names = menuId == CS_MENU_CLASS_T ? namesT : namesCT;
            static const char* commands[] = { "joinclass 1\n", "joinclass 2\n", "joinclass 3\n", "joinclass 4\n", "joinclass 5\n", "joinclass 6\n" };
            for (int i = 0; i < 6; ++i) AddButton(viewport, names[i], 66 + i * 44, commands[i], 0, 34);
            AddButton(viewport, "0  CANCEL", 338, 0, 0, 34);
            LoadResource(menuId == CS_MENU_CLASS_T
                ? "resource/UI/Classmenu_TER.res"
                : "resource/UI/Classmenu_CT.res");
        }
    }
};

class CBuyRootPanel final : public CCSMenuPanel
{
public:
    CBuyRootPanel(CCSViewport* viewport) : CCSMenuPanel("BUY MENU", 448, 640)
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
        AddButton(viewport, "AUTO-BUY", 58, "autobuy\n", 0);
        AddButton(viewport, "RE-BUY PREVIOUS", 94, "rebuy\n", 0);
        LoadResource("resource/UI/MainBuyMenu.res");
    }
};

class CBuyPanel final : public CCSMenuPanel
{
public:
    CBuyPanel(CCSViewport* viewport, const char* title, const BuyEntry* entries,
        int count, const char* resourcePath)
        : CCSMenuPanel(title, 148 + count * 36)
    {
        for (int i = 0; i < count; ++i)
            AddButton(viewport, entries[i].label, 58 + i * 36, entries[i].command, 0);
        const int bottom = 58 + count * 36 + 8;
        AddButton(viewport, "0  CANCEL", bottom, 0, 0);
        AddButton(viewport, "9  BACK", bottom + 36, 0, CS_MENU_BUY);
        LoadResource(resourcePath);
    }
};

class CCommandMenuPanel final : public CCSMenuPanel
{
public:
    CCommandMenuPanel(CCSViewport* viewport, int depth)
        : CCSMenuPanel("", 90, 140), m_depth(depth)
    {
        HideTitle();
        for (int i = 0; i < MAX_COMMAND_MENU_BUTTONS; ++i)
        {
            CUnicodeCommandButton* button = new CUnicodeCommandButton(
                viewport, depth, i, 0, LayoutValue(i * 27),
                LayoutValue(140), LayoutValue(28));
            button->setParent(this);
            button->addActionSignal(new CCommandSlotSignal(viewport, depth, i));
            button->setVisible(false);
            m_buttons[i] = button;
        }
    }

    bool Refresh(int node, int page)
    {
        (void)page;
        CS16VGUI_Trace("VGUI1: command menu refresh enter");
        const int count = CS16VGUI_CommandMenuGetCount(node);
        SetSelectedSlot(-1);
        int rows = count;
        if (rows > MAX_COMMAND_MENU_BUTTONS)
            rows = MAX_COMMAND_MENU_BUTTONS;

        for (int i = 0; i < rows; ++i)
        {
            const char* display = 0;
            int childNode = -1;
            if (!CS16VGUI_CommandMenuGetItem(node, i, &display, 0,
                &childNode, 0))
            {
                m_buttons[i]->setVisible(false);
                continue;
            }
            const char* label = display ? strstr(display, "  ") : NULL;
            label = label ? label + 2 : (display ? display : "");
            m_buttons[i]->SetUtf8Text(label);
            m_buttons[i]->SetOpensSubmenu(childNode >= 0);
            m_buttons[i]->setVisible(true);
        }
        int visibleRows = rows;
        for (int i = visibleRows; i < MAX_COMMAND_MENU_BUTTONS; ++i)
        {
            m_buttons[i]->SetOpensSubmenu(false);
            m_buttons[i]->setVisible(false);
        }
        setSize(LayoutValue(140), LayoutValue(visibleRows > 0
            ? visibleRows * 27 + 1 : 28));
        CS16VGUI_Trace("VGUI1: command menu refresh complete");
        return count > 0 || CS16VGUI_CommandMenuGetParent(node) >= 0;
    }
    void SetSelectedSlot(int selected)
    {
        for (int i = 0; i < MAX_COMMAND_MENU_BUTTONS; ++i)
            m_buttons[i]->setSelected(i == selected);
        repaint();
    }
private:
    int m_depth;
    CUnicodeCommandButton* m_buttons[MAX_COMMAND_MENU_BUTTONS];
};

class CCSViewport final : public vgui::Panel
{
public:
    CCSViewport(int width, int height)
        : vgui::Panel(0, 0, width, height), m_width(width), m_height(height),
          m_currentMenu(0), m_team(CS_TEAM_T), m_currentEntries(0),
          m_currentEntryCount(0), m_commandNode(0), m_commandPage(0),
          m_commandDepth(0), m_panelCount(0)
    {
        // GoldSrc's VGUI1 surface starts as an opaque black canvas. While a
        // fullscreen viewport menu is visible, ask the engine to paint the
        // current 3D view into that canvas before painting the menu panels.
        setPaintBackgroundEnabled(true);
        m_teamMenu = AddPanel(new CSelectionPanel(this, CS_MENU_TEAM));
        m_tClassMenu = AddPanel(new CSelectionPanel(this, CS_MENU_CLASS_T));
        m_ctClassMenu = AddPanel(new CSelectionPanel(this, CS_MENU_CLASS_CT));
        m_buyRoot = AddPanel(new CBuyRootPanel(this));
        m_pistolsT = AddPanel(new CBuyPanel(this, "BUY HANDGUN", g_pistolsT, Count(g_pistolsT), "resource/UI/BuyPistols_TER.res"));
        m_pistolsCT = AddPanel(new CBuyPanel(this, "BUY HANDGUN", g_pistolsCT, Count(g_pistolsCT), "resource/UI/BuyPistols_CT.res"));
        m_shotgunsT = AddPanel(new CBuyPanel(this, "BUY SHOTGUN", g_shotguns, Count(g_shotguns), "resource/UI/BuyShotguns_TER.res"));
        m_shotgunsCT = AddPanel(new CBuyPanel(this, "BUY SHOTGUN", g_shotguns, Count(g_shotguns), "resource/UI/BuyShotguns_CT.res"));
        m_riflesT = AddPanel(new CBuyPanel(this, "BUY RIFLE", g_riflesT, Count(g_riflesT), "resource/UI/BuyRifles_TER.res"));
        m_riflesCT = AddPanel(new CBuyPanel(this, "BUY RIFLE", g_riflesCT, Count(g_riflesCT), "resource/UI/BuyRifles_CT.res"));
        m_smgsT = AddPanel(new CBuyPanel(this, "BUY SUB-MACHINE GUN", g_smgsT, Count(g_smgsT), "resource/UI/BuySubMachineguns_TER.res"));
        m_smgsCT = AddPanel(new CBuyPanel(this, "BUY SUB-MACHINE GUN", g_smgsCT, Count(g_smgsCT), "resource/UI/BuySubMachineguns_CT.res"));
        m_machineGunsT = AddPanel(new CBuyPanel(this, "BUY MACHINE GUN", g_machineGuns, Count(g_machineGuns), "resource/UI/BuyMachineguns_TER.res"));
        m_machineGunsCT = AddPanel(new CBuyPanel(this, "BUY MACHINE GUN", g_machineGuns, Count(g_machineGuns), "resource/UI/BuyMachineguns_CT.res"));
        m_equipmentT = AddPanel(new CBuyPanel(this, "BUY EQUIPMENT", g_equipmentT, Count(g_equipmentT), "resource/UI/BuyEquipment_TER.res"));
        m_equipmentCT = AddPanel(new CBuyPanel(this, "BUY EQUIPMENT", g_equipmentCT, Count(g_equipmentCT), "resource/UI/BuyEquipment_CT.res"));
        for (int depth = 0; depth < MAX_COMMAND_MENU_DEPTH; ++depth)
        {
            m_commandMenus[depth] = static_cast<CCommandMenuPanel*>(
                AddPanel(new CCommandMenuPanel(this, depth)));
            m_commandNodes[depth] = 0;
            m_commandAnchorRows[depth] = 0;
        }
    }

    void Attach(vgui::Panel* root, int width, int height)
    {
        m_width = width; m_height = height;
        setBounds(0, 0, width, height); setParent(root);
        LayoutMenus(); HideMenu(); setVisible(true);
    }
    void SetTeam(int team) { if (team == CS_TEAM_T || team == CS_TEAM_CT) m_team = team; }

    int ShowMenu(int menuId)
    {
        const BuyEntry* entries = 0; int count = 0;
        CCSMenuPanel* panel = PanelForMenu(menuId, entries, count);
        if (!panel) return 0;
        HideAllPanels(); setVisible(true); panel->setVisible(true); panel->requestFocus();
        m_currentMenu = menuId; m_currentEntries = entries; m_currentEntryCount = count;
        UpdateCursor(true); repaint(); return 1;
    }

    int ShowCommandMenu()
    {
        if (m_currentMenu == CS_MENU_COMMAND) { HideMenu(); return 0; }
        if (!CS16VGUI_CommandMenuPrepare()) return 0;
        return ShowCommandNode(0, 0, 0, true);
    }
    void ReleaseCommandMenu() { if (m_currentMenu == CS_MENU_COMMAND) HideMenu(); }
    void HideMenu()
    {
        CS16VGUI_Trace("VGUI1: viewport hide panels enter");
        // Keep the root viewport alive: GoldSrc renders the normal 3D frame
        // through this panel's paintBackground(), even when no menu is open.
        HideAllPanels(); m_currentMenu = 0; m_currentEntries = 0;
        m_currentEntryCount = 0; m_commandNode = 0; m_commandPage = 0;
        m_commandDepth = 0;
        UpdateCursor(false);
        CS16VGUI_Trace("VGUI1: viewport hide panels complete");
    }
    void Shutdown()
    {
        HideMenu();
        setVisible(false);
    }
    void PerformAction(const char* command, int targetMenu)
    {
        if (targetMenu) { ShowMenu(targetMenu); return; }
        if (command && !strncmp(command, "joinclass ", 10))
            CS16VGUI_Trace("VGUI1: class action enter");
        HideMenu();
        if (command) CS16VGUI_ClientCommand(command);
        if (command && !strncmp(command, "joinclass ", 10))
            CS16VGUI_Trace("VGUI1: class action complete");
    }

    void PerformCommandSlot(int depth, int slot)
    {
        if (m_currentMenu != CS_MENU_COMMAND || depth < 0 ||
            depth > m_commandDepth || slot < 0) return;
        const int node = m_commandNodes[depth];
        const int count = CS16VGUI_CommandMenuGetCount(node);
        if (slot >= count || slot >= MAX_COMMAND_MENU_BUTTONS) return;
        int itemIndex = -1, childNode = -1;
        if (!CS16VGUI_CommandMenuGetItem(node, slot, 0, &itemIndex, &childNode, 0)) return;
        if (childNode >= 0)
        {
            m_commandMenus[depth]->SetSelectedSlot(slot);
            if (depth + 1 < MAX_COMMAND_MENU_DEPTH)
                ShowCommandNode(childNode, depth + 1, slot, false);
            return;
        }
        HideMenu(); CS16VGUI_CommandMenuExecute(itemIndex);
    }

    void HoverCommandSlot(int depth, int slot, bool opensSubmenu)
    {
        if (m_currentMenu != CS_MENU_COMMAND || depth < 0 ||
            depth > m_commandDepth || slot < 0)
            return;
        m_commandMenus[depth]->SetSelectedSlot(slot);
        for (int i = depth + 1; i < MAX_COMMAND_MENU_DEPTH; ++i)
            m_commandMenus[i]->setVisible(false);
        m_commandDepth = depth;
        m_commandNode = m_commandNodes[depth];
        if (opensSubmenu)
            PerformCommandSlot(depth, slot);
        repaint();
    }

    int KeyInput(int down, int keynum)
    {
        if (!m_currentMenu || !down) return 0;
        if (keynum == GOLDSRC_KEY_ESCAPE) { PerformAction(0, 0); return 1; }
        if (m_currentMenu == CS_MENU_COMMAND)
        {
            const int node = m_commandNodes[m_commandDepth];
            const int count = CS16VGUI_CommandMenuGetCount(node);
            for (int i = 0; i < count && i < MAX_COMMAND_MENU_BUTTONS; ++i)
            {
                int boundKey = 0;
                if (CS16VGUI_CommandMenuGetItem(node, i, 0, 0, 0,
                    &boundKey) && boundKey == keynum)
                {
                    PerformCommandSlot(m_commandDepth, i);
                    return 1;
                }
            }
            if (keynum == '0' && m_commandDepth > 0)
            {
                m_commandMenus[m_commandDepth]->setVisible(false);
                --m_commandDepth;
                m_commandNode = m_commandNodes[m_commandDepth];
                return 1;
            }
            return 0;
        }
        if (m_currentMenu == CS_MENU_TEAM)
        {
            switch (keynum) { case '1': PerformAction("jointeam 1\n", 0); return 1; case '2': PerformAction("jointeam 2\n", 0); return 1; case '3': PerformAction("jointeam 3\n", 0); return 1; case '5': PerformAction("jointeam 5\n", 0); return 1; case '6': PerformAction("jointeam 6\n", 0); return 1; case '0': PerformAction(0, 0); return 1; default: return 0; }
        }
        if (m_currentMenu == CS_MENU_CLASS_T || m_currentMenu == CS_MENU_CLASS_CT)
        {
            if (keynum >= '1' && keynum <= '6') { static const char* c[] = { "joinclass 1\n", "joinclass 2\n", "joinclass 3\n", "joinclass 4\n", "joinclass 5\n", "joinclass 6\n" }; PerformAction(c[keynum - '1'], 0); return 1; }
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

protected:
    void paintBackground() override
    {
        static bool tracedFirstPaint = false;
        if (!tracedFirstPaint)
            CS16VGUI_Trace("VGUI1: viewport background paint enter");

        int extents[4] = { 0, 0, 0, 0 };
        getAbsExtents(extents[0], extents[1], extents[2], extents[3]);
        CS16VGUI_PaintBackground(extents);

        if (!tracedFirstPaint)
        {
            CS16VGUI_Trace("VGUI1: viewport background paint complete");
            tracedFirstPaint = true;
        }
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
    int ShowCommandNode(int node, int depth, int anchorRow, bool reset)
    {
        if (depth < 0 || depth >= MAX_COMMAND_MENU_DEPTH ||
            !m_commandMenus[depth] || !m_commandMenus[depth]->Refresh(node, 0))
            return 0;
        if (reset)
            HideAllPanels();
        else
        {
            for (int i = depth; i < MAX_COMMAND_MENU_DEPTH; ++i)
                m_commandMenus[i]->setVisible(false);
        }
        m_commandNodes[depth] = node;
        m_commandAnchorRows[depth] = anchorRow;
        m_commandDepth = depth;
        m_commandNode = node;
        LayoutMenus(); setVisible(true);
        m_commandMenus[depth]->setVisible(true);
        m_commandMenus[depth]->requestFocus();
        m_currentMenu = CS_MENU_COMMAND; m_currentEntries = 0; m_currentEntryCount = 0;
        m_commandPage = 0; UpdateCursor(true); repaint(); return 1;
    }

    int CommandPanelDepth(CCSMenuPanel* panel) const
    {
        for (int depth = 0; depth < MAX_COMMAND_MENU_DEPTH; ++depth)
        {
            if (panel == m_commandMenus[depth])
                return depth;
        }
        return -1;
    }
    CCSMenuPanel* PanelForMenu(int menuId, const BuyEntry*& entries, int& count)
    {
        switch (menuId)
        {
        case CS_MENU_TEAM: return m_teamMenu; case CS_MENU_CLASS_T: return m_tClassMenu; case CS_MENU_CLASS_CT: return m_ctClassMenu; case CS_MENU_BUY: return m_buyRoot;
        case CS_MENU_BUY_PISTOL: entries = m_team == CS_TEAM_CT ? g_pistolsCT : g_pistolsT; count = m_team == CS_TEAM_CT ? Count(g_pistolsCT) : Count(g_pistolsT); return m_team == CS_TEAM_CT ? m_pistolsCT : m_pistolsT;
        case CS_MENU_BUY_SHOTGUN: entries = g_shotguns; count = Count(g_shotguns); return m_team == CS_TEAM_CT ? m_shotgunsCT : m_shotgunsT;
        case CS_MENU_BUY_RIFLE: entries = m_team == CS_TEAM_CT ? g_riflesCT : g_riflesT; count = m_team == CS_TEAM_CT ? Count(g_riflesCT) : Count(g_riflesT); return m_team == CS_TEAM_CT ? m_riflesCT : m_riflesT;
        case CS_MENU_BUY_SMG: entries = m_team == CS_TEAM_CT ? g_smgsCT : g_smgsT; count = m_team == CS_TEAM_CT ? Count(g_smgsCT) : Count(g_smgsT); return m_team == CS_TEAM_CT ? m_smgsCT : m_smgsT;
        case CS_MENU_BUY_MACHINEGUN: entries = g_machineGuns; count = Count(g_machineGuns); return m_team == CS_TEAM_CT ? m_machineGunsCT : m_machineGunsT;
        case CS_MENU_BUY_EQUIPMENT: entries = m_team == CS_TEAM_CT ? g_equipmentCT : g_equipmentT; count = m_team == CS_TEAM_CT ? Count(g_equipmentCT) : Count(g_equipmentT); return m_team == CS_TEAM_CT ? m_equipmentCT : m_equipmentT;
        default: return 0;
        }
    }
    void LayoutMenus()
    {
        for (int i = 0; i < m_panelCount; ++i)
        {
            int wide = 0, tall = 0; m_panels[i]->getSize(wide, tall);
            const int commandDepth = CommandPanelDepth(m_panels[i]);
            if (commandDepth >= 0)
            {
                int y = 152;
                for (int depth = 1; depth <= commandDepth; ++depth)
                    y += m_commandAnchorRows[depth] * 27;
                m_panels[i]->setPos(LayoutValue(commandDepth * 139), LayoutValue(y));
            }
            else
            {
                int resourceX = 0, resourceY = 0;
                if (m_panels[i]->GetResourcePosition(resourceX, resourceY))
                {
                    int canvasX = (m_width - LayoutValue(640)) / 2;
                    int canvasY = (m_height - LayoutValue(480)) / 2;
                    if (canvasX < 0) canvasX = 0;
                    if (canvasY < 0) canvasY = 0;
                    m_panels[i]->setPos(canvasX + resourceX, canvasY + resourceY);
                }
                else
                    m_panels[i]->setPos((m_width - wide) / 2, (m_height - tall) / 2);
            }
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
    int m_currentEntryCount, m_commandNode, m_commandPage, m_commandDepth;
    CCSMenuPanel* m_panels[24]; int m_panelCount;
    CCSMenuPanel *m_teamMenu, *m_tClassMenu, *m_ctClassMenu, *m_buyRoot;
    CCSMenuPanel *m_pistolsT, *m_pistolsCT, *m_shotgunsT, *m_shotgunsCT, *m_riflesT, *m_riflesCT;
    CCSMenuPanel *m_smgsT, *m_smgsCT, *m_machineGunsT, *m_machineGunsCT, *m_equipmentT, *m_equipmentCT;
    CCommandMenuPanel* m_commandMenus[MAX_COMMAND_MENU_DEPTH];
    int m_commandNodes[MAX_COMMAND_MENU_DEPTH];
    int m_commandAnchorRows[MAX_COMMAND_MENU_DEPTH];
};

void CMenuActionSignal::actionPerformed(vgui::Panel*) { m_viewport->PerformAction(m_command, m_targetMenu); }
void CCommandSlotSignal::actionPerformed(vgui::Panel*) { m_viewport->PerformCommandSlot(m_depth, m_slot); }
void CUnicodeCommandButton::cursorEntered(vgui::Panel* panel)
{
    CCSMenuButton::cursorEntered(panel);
    if (m_viewport)
        m_viewport->HoverCommandSlot(m_depth, m_slot, m_opensSubmenu);
}
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
    // GoldSrc can tear down the VGUI root before HUD_Shutdown. Detaching from
    // that already-destroying parent calls into an invalid vgui::Panel.
    CS16VGUI_Trace("VGUI1: viewport shutdown enter");
    if (g_viewport) g_viewport->Shutdown();
    CS16VGUI_Trace("VGUI1: viewport shutdown complete");
}
extern "C" void CS16VGUI_ImplSetTeam(int team) { if (g_viewport) g_viewport->SetTeam(team); }
extern "C" int CS16VGUI_ImplShowMenu(int menuId) { return g_viewport ? g_viewport->ShowMenu(menuId) : 0; }
extern "C" int CS16VGUI_ImplShowCommandMenu(void) { return g_viewport ? g_viewport->ShowCommandMenu() : 0; }
extern "C" void CS16VGUI_ImplReleaseCommandMenu(void) { if (g_viewport) g_viewport->ReleaseCommandMenu(); }
extern "C" void CS16VGUI_ImplHideMenu(void) { if (g_viewport) g_viewport->HideMenu(); }
extern "C" int CS16VGUI_ImplKeyInput(int down, int keynum, const char*) { return g_viewport ? g_viewport->KeyInput(down, keynum) : 0; }

#endif
