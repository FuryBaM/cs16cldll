// Lightweight GoldSrc voice state manager.
//
// This target intentionally does not use the obsolete VGUI1 scoreboard/menu
// implementation. Voice state, the in-world speaker sprite, and the compact
// speaker-name HUD are drawn directly by the client.

#ifndef VOICE_STATUS_H
#define VOICE_STATUS_H
#pragma once

#include "voice_common.h"
#include "cl_entity.h"
#include "voice_banmgr.h"

class IVoiceStatusHelper
{
public:
	virtual ~IVoiceStatusHelper() {}
	virtual void GetPlayerTextColor(int entindex, int color[3]) = 0;
	virtual void UpdateCursorState() = 0;
	virtual int GetAckIconHeight() = 0;
	virtual bool CanShowSpeakerLabels() = 0;
};

class CVoiceStatus : public CHudBase
{
public:
	CVoiceStatus();
	~CVoiceStatus() override;

	int Init(IVoiceStatusHelper *pHelper);
	int VidInit() override;
	int Draw(float flTime) override;
	void Shutdown() override;

	void Frame(double frameTime);
	void CreateEntities();
	void UpdateSpeakerStatus(int entindex, qboolean talking);
	void HandleVoiceMaskMsg(int size, void *buffer);
	void HandleReqStateMsg(int size, void *buffer);

	void StartSquelchMode();
	void StopSquelchMode();
	bool IsInSquelchMode() const;

	bool IsPlayerBlocked(int playerIndex);
	bool IsPlayerAudible(int playerIndex);
	void SetPlayerBlockedState(int playerIndex, bool blocked);
	void UpdateServerState(bool force);

	CVoiceBanMgr m_BanMgr;

private:
	IVoiceStatusHelper *m_pHelper;
	CPlayerBitVec m_VoicePlayers;
	CPlayerBitVec m_VoiceEnabledPlayers;
	CPlayerBitVec m_AudiblePlayers;
	CPlayerBitVec m_ServerBannedPlayers;
	cl_entity_s m_VoiceHeadModels[VOICE_MAX_PLAYERS];
	HSPRITE m_VoiceHeadModel;
	float m_VoiceHeadModelHeight;
	float m_LastUpdateServerState;
	int m_ServerModEnable;
	bool m_InSquelchMode;
	bool m_Talking;
	bool m_ServerAcked;
	bool m_Initialized;
	bool m_BanMgrInitialized;
	char *m_GameDir;
};

CVoiceStatus *GetClientVoiceMgr();

#endif
