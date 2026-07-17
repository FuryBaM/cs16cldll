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
    // Масштаб в консольном выводе HLSDK отсутствует. Игнорируем.
    return StringWidth(szIt);
}

int DrawUtils::DrawHudString(int x, int y, int /*iMaxX*/, const char* szString,
    int r, int g, int b, float /*scale*/, bool /*drawing*/)
{
    char converted[4096];
    szString = CS16_LegacyHudText(szString, converted, sizeof(converted));
    if (CS16_HudTextNeedsUnicode(szString))
    {
        const int wide = CS16VGUI2_DrawHudString(x, y, szString, r, g, b, 255);
        if (wide >= 0)
            return x + wide;
    }
    SetTextColor255(r, g, b);
    DrawTextXY(x, y, szString);
    return x + StringWidth(szString);
}

int DrawUtils::DrawHudStringReverse(int xpos, int ypos, int /*iMinX*/, const char* szString,
    int r, int g, int b, float /*scale*/, bool /*drawing*/)
{
    char converted[4096];
    szString = CS16_LegacyHudText(szString, converted, sizeof(converted));
    const int w = StringWidth(szString);
    const int x = xpos - w;
    if (CS16_HudTextNeedsUnicode(szString) &&
        CS16VGUI2_DrawHudString(x, ypos, szString, r, g, b, 255) >= 0)
        return x;
    SetTextColor255(r, g, b);
    DrawTextXY(x, ypos, szString);
    return x;
}

int DrawUtils::DrawHudNumber2(int x, int y, bool DrawZero, int iDigits, int iNumber,
    int r, int g, int b)
{
    // Формирование числа по флагам
    // Если число 0 и DrawZero=false, ничего не рисуем.
    if (iNumber == 0 && !DrawZero && iDigits <= 0)
        return x;

    char buf[32];
    if (iDigits > 0)
        std::snprintf(buf, sizeof(buf), "%0*d", iDigits, iNumber);
    else
        std::snprintf(buf, sizeof(buf), "%d", iNumber);

    SetTextColor255(r, g, b);
    DrawTextXY(x, y, buf);
    return x + StringWidth(buf);
}

int DrawUtils::DrawHudNumber2(int x, int y, int iNumber, int r, int g, int b)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", iNumber);
    SetTextColor255(r, g, b);
    DrawTextXY(x, y, buf);
    return x + StringWidth(buf);
}

int DrawUtils::DrawHudNumber(int x, int y, int iFlags, int iNumber,
    int r, int g, int b)
{
    // Поддержка DHN_DRAWZERO / DHN_2DIGITS / DHN_3DIGITS
    const bool drawZero = (iFlags & DHN_DRAWZERO) != 0;
    int digits = 0;
    if (iFlags & DHN_3DIGITS) digits = 3;
    else if (iFlags & DHN_2DIGITS) digits = 2;

    return DrawHudNumber2(x, y, drawZero, digits, iNumber, r, g, b);
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
