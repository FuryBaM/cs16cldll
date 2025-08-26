// draw_util.cpp — чистая реализация под HLSDK / GoldSrc
#include "hud.h"
#include "cl_dll.h"
#include "triangleapi.h"
#include "draw_util.h"

#include <cstdio>
#include <cstring>

// статический цвет для textmode-ветки
float DrawUtils::color[3] = { 1.0f, 1.0f, 1.0f };

int g_codepage = 0;
qboolean g_accept_utf8;

cvar_t* con_charset;
cvar_t* cl_charset;

// --- Внутренние помощники ---

static inline void SetTextColor255(int r, int g, int b)
{
    gEngfuncs.pfnDrawSetTextColor(r / 255.0f, g / 255.0f, b / 255.0f);
}

static inline int StringWidth(const char* s)
{
    int w = 0, h = 0;
    gEngfuncs.pfnDrawConsoleStringLen(const_cast<char*>(s), &w, &h);
    return w;
}

static inline void DrawTextXY(int x, int y, const char* s)
{
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
    SetTextColor255(r, g, b);
    DrawTextXY(x, y, szString);
    return x + StringWidth(szString);
}

int DrawUtils::DrawHudStringReverse(int xpos, int ypos, int /*iMinX*/, const char* szString,
    int r, int g, int b, float /*scale*/, bool /*drawing*/)
{
    const int w = StringWidth(szString);
    const int x = xpos - w;
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