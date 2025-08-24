#pragma once
#ifndef RADAR_H
#define RADAR_H

class CClientSprite;

class CHudRadar : public CHudBase
{
public:
    int Init() override;
    int VidInit() override;
    int Draw(float flTime) override;
    void InitHUDData() override;
    void Reset() override;
    void Shutdown() override;

    int MsgFunc_Radar(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_BombDrop(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_BombPickup(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_HostagePos(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_HostageK(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_Location(const char* pszName, int iSize, void* pbuf);

    void UserCmd_ShowRadar();
    void UserCmd_HideRadar();

    CClientSprite m_hRadar;
    CClientSprite m_hRadarOpaque;

private:
    cvar_t* cl_radartype;

    void DrawPlayerLocation(int y);
    void DrawRadarDot(int x, int y, int r, int g, int b, int a);
    void DrawCross(int x, int y, int r, int g, int b, int a);
    void DrawT(int x, int y, int r, int g, int b, int a);
    void DrawFlippedT(int x, int y, int r, int g, int b, int a);

    inline void DrawZAxis(Vector pos, int r, int g, int b, int a);

    bool HostageFlashTime(float flTime, struct hostage_info_t* pplayer);
    bool FlashTime(float flTime, struct extra_player_info_t* pplayer);
    Vector WorldToRadar(const Vector vPlayerOrigin, const Vector vObjectOrigin, const Vector vAngles);
    inline void DrawColoredTexture(int x, int y, int size, byte r, byte g, byte b, byte a, int texHandle);
    // отрисовка квадрата (без gRenderAPI)
    inline void DrawColoredQuad(int x, int y, int size, byte r, byte g, byte b, byte a);

    int iMaxRadius;
};

#endif // RADAR_H
