// правильный hud.h (как в HLSDK)
#include "../cl_dll/hud.h"
#include "cl_util.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "demo.h"
#include "demo_api.h"
#include "r_efx.h"
#include "entity_types.h"

#include "voice_status.h"
#include "voice_status_hud.h"
#include "triangleapi.h"
#include "draw_util.h"
#include <camera.h>

HSPRITE g_hVoiceLabelIcon = 0;
wrect_t g_rcVoiceLabelIcon{ 0,0,0,0 };

CVoiceStatusHud g_VoiceStatusHud;
IVoiceHud* GetClientVoiceHud() { return &g_VoiceStatusHud; }

void CVoiceLabel::SetLocation(const char* location)
{
    if (!location || !*location)
    {
        if (m_locationString) { delete[] m_locationString; m_locationString = nullptr; RebuildLabelText(); }
        return;
    }

    const char* newLocation = Localize(location);
    if (newLocation)
    {
        const size_t n = strlen(newLocation) + 1;
        if (!m_locationString || strcmp(newLocation, m_locationString) != 0)
        {
            delete[] m_locationString;
            m_locationString = new char[n];
            memcpy(m_locationString, newLocation, n);
            RebuildLabelText();
        }
    }
}

void CVoiceLabel::SetPlayerName(const char* name)
{
    delete[] m_playerName; m_playerName = nullptr;
    if (name)
    {
        const size_t n = strlen(name) + 1;
        m_playerName = new char[n];
        memcpy(m_playerName, name, n);
    }
    RebuildLabelText();
}

void CVoiceLabel::RebuildLabelText()
{
    const int BufLen = sizeof(m_buf);
    m_buf[0] = '\0';

    if (m_playerName)
    {
        const char* formatStr = "%s   ";
        if (m_locationString)
        {
            const char* locFmt = Localize("#Voice_Location");
            if (locFmt) formatStr = locFmt;
            else       formatStr = "%s/%s   ";
            snprintf(m_buf, BufLen, formatStr, m_playerName, m_locationString);
            return;
        }
        snprintf(m_buf, BufLen, formatStr, m_playerName);
    }
}

void CVoiceLabel::Draw()
{
    if (!GetVisible()) return;

    int offset = 1;
    int iconsize = tall - offset * 2;

    gEngfuncs.pfnFillRGBABlend(x, y, wide, tall, m_bgColor.r, m_bgColor.g, m_bgColor.b, m_bgColor.a);

    if (g_hVoiceLabelIcon)
    {
        gEngfuncs.pfnSPR_Set(g_hVoiceLabelIcon, 255, 255, 255);
        gEngfuncs.pfnSPR_DrawAdditive(0, x + 1, y + offset, &g_rcVoiceLabelIcon);
    }

    const int textx = x + iconsize + offset;
    const int texty = y + (tall - gHUD.GetCharHeight()) / 2;
    DrawUtils::DrawHudString(textx, texty, textx + wide, m_buf, m_fgColor.r, m_fgColor.g, m_fgColor.b);
}

// ---------------------------------------------------------------------- //
// CVoiceStatusHud
// ---------------------------------------------------------------------- //
CVoiceStatusHud::CVoiceStatusHud() {}
CVoiceStatusHud::~CVoiceStatusHud() { Shutdown(); }

int CVoiceStatusHud::Init(IVoiceStatusHelper* pHelper, IVoiceStatus* pStatus)
{
    m_pHelper = pHelper;
    m_pStatus = pStatus;

    gHUD.AddHudElem(this);
    m_iFlags = HUD_ACTIVE;

    m_Labels.clear();
    return 1;
}

int CVoiceStatusHud::VidInit()
{
    m_VoiceHeadModelHeight = 45;
    if (char* pFile = (char*)gEngfuncs.COM_LoadFile("scripts/voicemodel.txt", 5, NULL))
    {
        char token[4096]{ 0 };
        gEngfuncs.COM_ParseFile(pFile, token);
        if (token[0] >= '0' && token[0] <= '9') m_VoiceHeadModelHeight = (float)atof(token);
        gEngfuncs.COM_FreeFile(pFile);
    }

    m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/voiceicon.spr");

    m_hLocalTalkIcon = gEngfuncs.pfnSPR_Load("sprites/icntlk_pl.spr");
    m_rcLocalTalkIcon.left = 0; m_rcLocalTalkIcon.top = 0;
    m_rcLocalTalkIcon.right = gEngfuncs.pfnSPR_Width(m_hLocalTalkIcon, 0);
    m_rcLocalTalkIcon.bottom = gEngfuncs.pfnSPR_Height(m_hLocalTalkIcon, 0);
    m_LocalPlayerTalkIconXSize = m_rcLocalTalkIcon.right - m_rcLocalTalkIcon.left;
    m_LocalPlayerTalkIconYSize = m_rcLocalTalkIcon.bottom - m_rcLocalTalkIcon.top;
    m_LocalPlayerTalkIconVisible = false;

    g_hVoiceLabelIcon = gEngfuncs.pfnSPR_Load("sprites/speaker4.spr");
    g_rcVoiceLabelIcon.left = 0; g_rcVoiceLabelIcon.top = 0;
    g_rcVoiceLabelIcon.right = gEngfuncs.pfnSPR_Width(g_hVoiceLabelIcon, 0);
    g_rcVoiceLabelIcon.bottom = gEngfuncs.pfnSPR_Height(g_hVoiceLabelIcon, 0);

    return TRUE;
}

void CVoiceStatusHud::CreateEntities()
{
    if (!m_VoiceHeadModel) return;

    cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();
    int iOutModel = 0;

    for (int i = 0; i < VOICE_MAX_PLAYERS; ++i)
    {
        if (m_pStatus->GetSpeakerStatus(i) != CVoiceStatus::VOICE_TALKING)
            continue;

        cl_entity_s* pClient = gEngfuncs.GetEntityByIndex(i + 1);
        if (!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum)
            continue;
        if (pClient->curstate.effects & EF_NODRAW)
            continue;
        if (pClient == localPlayer && !cam_thirdperson)
            continue;

        cl_entity_s* pEnt = &m_VoiceHeadModels[iOutModel++];
        memset(pEnt, 0, sizeof(*pEnt));

        pEnt->curstate.rendermode = kRenderTransAdd;
        pEnt->curstate.renderamt = 255;
        pEnt->baseline.renderamt = 255;
        pEnt->curstate.renderfx = kRenderFxNoDissipation;
        pEnt->curstate.framerate = 1;
        pEnt->curstate.frame = 0;
        pEnt->model = (struct model_s*)gEngfuncs.GetSpritePointer(m_VoiceHeadModel);
        pEnt->angles[0] = pEnt->angles[1] = pEnt->angles[2] = 0;
        pEnt->curstate.scale = 0.5f;

        pEnt->origin[0] = pEnt->origin[1] = 0;
        pEnt->origin[2] = 45;
        VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

        gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
    }
}

void CVoiceStatusHud::UpdateLocation(int entindex, const char* location)
{
    int iClient = entindex - 1;
    if (iClient < 0) return;
    if (CVoiceLabel* pLabel = FindVoiceLabel(iClient))
    {
        pLabel->SetLocation(location);
        RepositionLabels();
    }
}

CVoiceLabel* CVoiceStatusHud::FindVoiceLabel(int clientindex)
{
    for (auto* L : m_Labels)
        if (L && L->GetClientIndex() == clientindex) return L;
    return nullptr;
}

CVoiceLabel* CVoiceStatusHud::GetFreeVoiceLabel()
{
    if (CVoiceLabel* L = FindVoiceLabel(-1)) return L;
    auto* lab = new CVoiceLabel();
    m_Labels.push_back(lab);
    return lab;
}

void CVoiceStatusHud::RepositionLabels()
{
    int y = ScreenHeight / 2;
    for (auto* pLabel : m_Labels)
    {
        if (!pLabel || !pLabel->GetVisible()) continue;
        int w, h; pLabel->GetContentSize(w, h);
        pLabel->SetBounds(ScreenWidth - w - 8, y);
        y += h + 2;
    }
}

void CVoiceStatusHud::UpdateSpeakerStatus(int entindex, bool bTalking)
{
    if (entindex == -2)
    {
        if (bTalking && m_hLocalTalkIcon)
        {
            const int local_xPos = ScreenWidth - m_LocalPlayerTalkIconXSize - 10;
            const int local_yPos = ScreenHeight - (m_pHelper ? m_pHelper->GetAckIconHeight() : 0) - m_LocalPlayerTalkIconYSize;
            m_LocalPlayerTalkIconXPos = local_xPos;
            m_LocalPlayerTalkIconYPos = local_yPos;
            m_LocalPlayerTalkIconVisible = true;
        }
        else
        {
            m_LocalPlayerTalkIconVisible = false;
        }
    }
    else
    {
        if (entindex >= 0 && entindex <= MAX_PLAYERS)
        {
            int iClient = entindex - 1;
            if (iClient < 0) return;

            CVoiceLabel* pLabel = FindVoiceLabel(iClient);
            if (bTalking)
            {
                if (!pLabel)
                {
                    pLabel = GetFreeVoiceLabel();
                    if (pLabel)
                    {
                        hud_player_info_t info{}; gEngfuncs.pfnGetPlayerInfo(entindex, &info);
                        int color[3]{ 255,255,255 };
                        if (m_pHelper) m_pHelper->GetPlayerTextColor(entindex, color);

                        pLabel->SetFgColor(RGBA({ 255,255,255,255 }));
                        pLabel->SetBgColor(RGBA({ (unsigned char)color[0], (unsigned char)color[1], (unsigned char)color[2], 180 }));
                        pLabel->SetPlayerName(info.name);
                        pLabel->SetLocation(m_pHelper ? m_pHelper->GetPlayerLocation(entindex) : "");
                        pLabel->SetClientIndex(iClient);
                        pLabel->SetVisible(!m_pHelper || m_pHelper->CanShowSpeakerLabels());
                    }
                }
            }
            else if (pLabel)
            {
                pLabel->SetVisible(false);
                pLabel->SetClientIndex(-1);
            }
        }
    }
    RepositionLabels();
}

void CVoiceStatusHud::Shutdown(void)
{
    for (auto*& L : m_Labels) { delete L; L = nullptr; }
    m_Labels.clear();
}

int CVoiceStatusHud::Draw(float flTime)
{
    if (m_LocalPlayerTalkIconVisible && m_hLocalTalkIcon)
    {
        gEngfuncs.pfnSPR_Set(m_hLocalTalkIcon, 255, 255, 255);
        gEngfuncs.pfnSPR_DrawAdditive(0, m_LocalPlayerTalkIconXPos, m_LocalPlayerTalkIconYPos, &m_rcLocalTalkIcon);
    }

    for (auto* L : m_Labels)
        if (L && L->GetVisible()) L->Draw();

    return 1;
}
