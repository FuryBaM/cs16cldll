// voice_status_hud.h — HLSDK/GoldSrc
#ifndef VOICE_STATUS_HUD_H
#define VOICE_STATUS_HUD_H
#pragma once

#include "voice_common.h"
#include "cl_entity.h"
#include "voice_banmgr.h"
#include "draw_util.h"
#include "triangleapi.h"
#include <vector>

struct wrect_s;

extern HSPRITE g_hVoiceLabelIcon;
extern wrect_t g_rcVoiceLabelIcon;

class CVoiceLabel
{
public:
    CVoiceLabel()
    {
        m_clientindex = -1;
        m_locationString = nullptr;
        m_playerName = nullptr;
        m_visible = false;
        x = y = wide = tall = 0;
        m_buf[0] = '\0';
        iconwidth = 0;
        m_fgColor = RGBA({ 255,255,255,255 });
        m_bgColor = RGBA({ 0,0,0,180 });
    }
    ~CVoiceLabel()
    {
        delete[] m_locationString;
        delete[] m_playerName;
    }

    void SetFgColor(RGBA c) { m_fgColor = c; }
    void SetBgColor(RGBA c) { m_bgColor = c; }
    void SetVisible(bool s) { m_visible = s; }
    bool GetVisible() { return m_visible; }

    void GetContentSize(int& outW, int& outH)
    {
        outW = DrawUtils::HudStringLen(m_playerName ? m_playerName : "") + 8;
        outH = gHUD.GetCharHeight();
        if (outH < 32) outH = 32;
        outW += outH - 2; // место под иконку
    }

    void SetBounds(int _x, int _y)
    {
        x = _x; y = _y;
        GetContentSize(wide, tall);
    }

    void Draw();

    void SetClientIndex(int in) { m_clientindex = in; }
    int  GetClientIndex() { return m_clientindex; }

    void SetLocation(const char* location);
    void SetPlayerName(const char* name);

private:
    void RebuildLabelText();

    int   m_clientindex;
    char* m_locationString;
    char* m_playerName;

    RGBA  m_fgColor;
    RGBA  m_bgColor;
    bool  m_visible;

    int   x, y, wide, tall;
    char  m_buf[512];
    int   iconwidth;
};

class CVoiceStatusHud : public IVoiceHud, public CHudBase
{
public:
    CVoiceStatusHud();
    virtual ~CVoiceStatusHud();

    int  Init(IVoiceStatusHelper* pHelper, IVoiceStatus* pStatus) override;
    int  VidInit() override;
    void CreateEntities();
    void UpdateLocation(int entindex, const char* location);
    void UpdateSpeakerStatus(int entindex, bool bTalking);
    CVoiceLabel* FindVoiceLabel(int clientindex);
    CVoiceLabel* GetFreeVoiceLabel();
    void RepositionLabels();
    void Shutdown(void);
    int  Draw(float flTime) override;

    // доступ из CVoiceLabel
    HSPRITE m_hLocalTalkIcon = 0;
    wrect_t m_rcLocalTalkIcon{};
    bool    m_LocalPlayerTalkIconVisible = false;
    int     m_LocalPlayerTalkIconXPos = 0, m_LocalPlayerTalkIconYPos = 0;
    int     m_LocalPlayerTalkIconXSize = 0, m_LocalPlayerTalkIconYSize = 0;

private:
    cl_entity_s m_VoiceHeadModels[VOICE_MAX_PLAYERS]{};
    HSPRITE     m_VoiceHeadModel = 0;
    float       m_VoiceHeadModelHeight = 45.0f;

    IVoiceStatusHelper* m_pHelper = nullptr;
    IVoiceStatus* m_pStatus = nullptr;

    std::vector<CVoiceLabel*> m_Labels;
};

#endif // VOICE_STATUS_HUD_H
