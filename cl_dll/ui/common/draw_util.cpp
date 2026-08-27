// draw_util.cpp — чистая реализация под HLSDK / GoldSrc
#include "hud.h"
#include "cl_dll.h"
#include "triangleapi.h"
#include "draw_util.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

// статический цвет для textmode-ветки
float DrawUtils::color[3] = { 1.0f, 1.0f, 1.0f };

int g_codepage = 0;
qboolean g_accept_utf8;

cvar_t* con_charset;
cvar_t* cl_charset;
cvar_t* cl_hud_font;

// --- Внутренние помощники ---

bool CS16_HudTextNeedsUnicode(const char* text)
{
    if (!text)
        return false;
    for (const unsigned char* cursor = (const unsigned char*)text; *cursor; ++cursor)
    {
        if (*cursor >= 0x80)
            return true;
    }
    return false;
}

const char *CS16_LegacyHudText(const char* source, char* destination, size_t destinationSize)
{
    if (!destination || destinationSize == 0)
        return source ? source : "";

    if (!source)
        source = "";

    destination[0] = '\0';

#if defined(_WIN32)
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
        // A strict decode distinguishes UTF-8 sent by modern servers/plugins
        // from the CP1251 strings still commonly sent by legacy AMX plugins.
        wchar_t wide[4096];
        const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            source, -1, wide, ARRAYSIZE(wide));
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
                (int)destinationSize, NULL, NULL) > 0)
            {
                destination[destinationSize - 1] = '\0';
                return destination;
            }
        }
    }
#endif

    strncpy(destination, source, destinationSize);
    destination[destinationSize - 1] = '\0';
    return destination;
}

static inline void SetTextColor255(int r, int g, int b)
{
    gEngfuncs.pfnDrawSetTextColor(r / 255.0f, g / 255.0f, b / 255.0f);
}

// The original CS HUD glyphs come from the engine's character font, the one
// pfnDrawCharacter draws from and SCREENINFO reports the widths of. A width of
// zero means the font has no glyph for that byte, which is how installs without
// a Cyrillic page look; those strings go to the VGUI2 font instead.
static bool HudFontHasGlyphs(const char* legacy)
{
    if (gHUD.GetCharWidth('M') <= 0)
        return false; // SCREENINFO not filled in yet

    for (const unsigned char* p = (const unsigned char*)legacy; *p; ++p)
    {
        if (*p >= 0x80 && gHUD.GetCharWidth(*p) <= 0)
            return false;
    }

    return true;
}

// cl_hud_font 0 keeps every HUD string on the font Counter-Strike defines in
// its own scheme, which is what the rest of the game's text uses. 1 switches to
// the engine's bitmap font, still falling back per string to the scheme font
// for characters that font has no glyph for.
static bool UseSchemeHudFont(const char* legacy)
{
    if (!cl_hud_font || cl_hud_font->value == 0.0f)
        return true;

    return !HudFontHasGlyphs(legacy);
}
// none, so neither ever ends up drawn as text.
static int TextEscapeLength(const char* s)
{
    if (s[0] == '\\' && s[1] && s[1] != '\n' &&
        (s[1] == 'y' || s[1] == 'r' || s[1] == 'w' || s[1] == 'd'))
        return 2;

    if (s[0] == '^' && s[1] >= '0' && s[1] <= '9')
        return 2;

    return 0;
}

static int ParseTextEscape(const char* s, int& r, int& g, int& b)
{
    const int length = TextEscapeLength(s);
    if (length && s[0] == '\\')
    {
        switch (s[1])
        {
        case 'y': DrawUtils::UnpackRGB(r, g, b, RGB_YELLOWISH); break;
        case 'r': DrawUtils::UnpackRGB(r, g, b, RGB_REDISH); break;
        case 'w': DrawUtils::UnpackRGB(r, g, b, RGB_WHITE); break;
        case 'd': DrawUtils::UnpackRGB(r, g, b, RGB_GRAY); break;
        }
    }

    return length;
}

// The VGUI2 font draws a whole string at once, so the escapes have to come out
// first. The colour of the last one wins, which is what the callers that use
// them (the text menus) expect anyway.
static const char* StripTextEscapes(const char* legacy, char* destination,
    size_t destinationSize, int& r, int& g, int& b)
{
    size_t written = 0;

    for (const char* p = legacy; *p && *p != '\n' && written + 1 < destinationSize;)
    {
        const int skip = ParseTextEscape(p, r, g, b);
        if (skip)
        {
            p += skip;
            continue;
        }

        destination[written++] = *p++;
    }

    destination[written] = 0;
    return destination;
}


static int SchemeStringWidth(const char* legacy)
{
    char clean[4096];
    int r = 0, g = 0, b = 0;
    StripTextEscapes(legacy, clean, sizeof(clean), r, g, b);

    int wide = 0, tall = 0;
    return CS16VGUI2_GetHudStringSize(clean, &wide, &tall) ? wide : -1;
}

// Draws through the scheme font, trimming the tail when the caller gave a right
// edge. Returns -1 when VGUI2 cannot draw, so the caller can fall back.
static int DrawSchemeString(int x, int y, int maxX, const char* legacy,
    int r, int g, int b)
{
    char clean[4096];
    StripTextEscapes(legacy, clean, sizeof(clean), r, g, b);

    int wide = 0, tall = 0;
    if (!CS16VGUI2_GetHudStringSize(clean, &wide, &tall))
        return -1;

    if (maxX > 0)
    {
        size_t length = strlen(clean);
        while (length && x + wide > maxX)
        {
            clean[--length] = 0;
            if (!CS16VGUI2_GetHudStringSize(clean, &wide, &tall))
                return -1;
        }
    }

    if (!clean[0])
        return x;

    const int drawn = CS16VGUI2_DrawHudString(x, y, clean, r, g, b, 255);
    return drawn >= 0 ? x + drawn : -1;
}

// Length of a "\y"-style colour escape or a "^1" colour code, 0 when there is
static int LegacyStringWidth(const char* legacy)
{
    int width = 0, r = 0, g = 0, b = 0;

    for (const char* p = legacy; *p && *p != '\n';)
    {
        const int skip = ParseTextEscape(p, r, g, b);
        if (skip)
        {
            p += skip;
            continue;
        }

        width += gHUD.GetCharWidth((unsigned char)*p++);
    }

    return width;
}

// Draws with the original glyphs. High bytes go straight to pfnDrawCharacter
// rather than through TextMessageDrawChar, which would divert them to the
// VGUI2 font even when the HUD font can render them.
static int DrawLegacyString(int x, int y, int maxX, const char* legacy,
    int r, int g, int b)
{
    for (const char* p = legacy; *p && *p != '\n';)
    {
        const int skip = ParseTextEscape(p, r, g, b);
        if (skip)
        {
            p += skip;
            continue;
        }

        const unsigned char ch = (unsigned char)*p++;
        if (maxX > 0 && x + gHUD.GetCharWidth(ch) > maxX)
            break;

        x += gEngfuncs.pfnDrawCharacter(x, y, ch, r, g, b);
    }

    return x;
}

static inline int StringWidth(const char* s)
{
    char converted[4096];
    s = CS16_LegacyHudText(s, converted, sizeof(converted));
    if (CS16_HudTextNeedsUnicode(s))
    {
        int w = 0, h = 0;
        if (CS16VGUI2_GetHudStringSize(s, &w, &h))
            return w;
    }
    int w = 0, h = 0;
    gEngfuncs.pfnDrawConsoleStringLen(const_cast<char*>(s), &w, &h);
    return w;
}

static inline void DrawTextXY(int x, int y, const char* s)
{
    char converted[4096];
    s = CS16_LegacyHudText(s, converted, sizeof(converted));
    gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(s));
}

// --- Публичные методы ---

int DrawUtils::HudStringLen(const char* szIt, float /*scale*/)
{
    char converted[4096];
    const char* s = CS16_LegacyHudText(szIt, converted, sizeof(converted));

    if (UseSchemeHudFont(s))
    {
        const int wide = SchemeStringWidth(s);
        if (wide >= 0)
            return wide;
    }

    return LegacyStringWidth(s);
}

int DrawUtils::DrawHudString(int x, int y, int iMaxX, const char* szString,
    int r, int g, int b, float /*scale*/, bool /*drawing*/)
{
    char converted[4096];
    const char* s = CS16_LegacyHudText(szString, converted, sizeof(converted));

    if (UseSchemeHudFont(s))
    {
        const int end = DrawSchemeString(x, y, iMaxX, s, r, g, b);
        if (end >= 0)
            return end;
    }

    return DrawLegacyString(x, y, iMaxX, s, r, g, b);
}

int DrawUtils::DrawHudStringReverse(int xpos, int ypos, int iMinX, const char* szString,
    int r, int g, int b, float /*scale*/, bool /*drawing*/)
{
    char converted[4096];
    const char* s = CS16_LegacyHudText(szString, converted, sizeof(converted));

    if (UseSchemeHudFont(s))
    {
        const int wide = SchemeStringWidth(s);
        if (wide >= 0)
        {
            int x = xpos - wide;
            if (iMinX > 0 && x < iMinX)
                x = iMinX;
            if (DrawSchemeString(x, ypos, xpos, s, r, g, b) >= 0)
                return x;
        }
    }

    int x = xpos - LegacyStringWidth(s);
    if (iMinX > 0 && x < iMinX)
        x = iMinX;

    DrawLegacyString(x, ypos, xpos, s, r, g, b);
    return x;
}

int DrawUtils::DrawHudNumber2(int x, int y, bool DrawZero, int iDigits, int iNumber,
    int r, int g, int b)
{
    // CS ships the HUD digits as number_0..number_9 entries in
    // cstrike/sprites/hud.txt. Draw the field from right to left so money,
    // the round timer and the rest of the HUD use those original glyphs.
    const int digitWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).Width();
    if (digitWidth <= 0 || iDigits <= 0)
        return x;

    iNumber = abs(iNumber);
    int digitX = x + (iDigits - 1) * digitWidth;
    const int resultX = digitX + digitWidth;

    do
    {
        const int digit = iNumber % 10;
        iNumber /= 10;
        SPR_Set(gHUD.GetSprite(gHUD.m_HUD_number_0 + digit), r, g, b);
        SPR_DrawAdditive(0, digitX, y,
            &gHUD.GetSpriteRect(gHUD.m_HUD_number_0 + digit));
        digitX -= digitWidth;
        --iDigits;
    }
    while (iNumber > 0 || (iDigits > 0 && DrawZero));

    return resultX;
}

int DrawUtils::DrawHudNumber2(int x, int y, int iNumber, int r, int g, int b)
{
    int value = abs(iNumber);
    int digits = 1;
    for (int remaining = value; remaining >= 10; remaining /= 10)
        ++digits;

    return DrawHudNumber2(x, y, false, digits, value, r, g, b);
}

int DrawUtils::DrawHudNumber(int x, int y, int iFlags, int iNumber,
    int r, int g, int b)
{
    // Keep the classic DHN layout (blank leading positions, not zeroes) used
    // by health, armor and ammo. CHud renders it with the same cstrike sprite
    // set loaded during VidInit.
    return gHUD.DrawHudNumber(x, y, iFlags, iNumber, r, g, b);
}

// Простейшие 2D-примитивы через TriAPI

void DrawUtils::Draw2DQuad(float x1, float y1, float x2, float y2)
{
    if (!gEngfuncs.pTriAPI)
        return;

    gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
    gEngfuncs.pTriAPI->Begin(TRI_QUADS);
    gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(x1, y1, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(x2, y1, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(x2, y2, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(x1, y2, 0.0f);
    gEngfuncs.pTriAPI->End();
}

void DrawUtils::DrawStretchPic(float x, float y, float w, float h,
    float s1, float t1, float s2, float t2)
{
    if (!gEngfuncs.pTriAPI)
        return;

    const float x1 = x;
    const float y1 = y;
    const float x2 = x + w;
    const float y2 = y + h;

    gEngfuncs.pTriAPI->RenderMode(kRenderTransTexture);
    gEngfuncs.pTriAPI->Begin(TRI_QUADS);
    gEngfuncs.pTriAPI->TexCoord2f(s1, t1); gEngfuncs.pTriAPI->Vertex3f(x1, y1, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(s2, t1); gEngfuncs.pTriAPI->Vertex3f(x2, y1, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(s2, t2); gEngfuncs.pTriAPI->Vertex3f(x2, y2, 0.0f);
    gEngfuncs.pTriAPI->TexCoord2f(s1, t2); gEngfuncs.pTriAPI->Vertex3f(x1, y2, 0.0f);
    gEngfuncs.pTriAPI->End();
}

int Con_UtfProcessChar(int in) { return in; }
int Con_UtfProcessCharForce(int in) { return in; }
