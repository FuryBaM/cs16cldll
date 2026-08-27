/*
radar.cpp - Radar
Copyright (C) 2016 a1batross

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

In addition, as a special exception, the author gives permission to
link the code of this program with the Half-Life Game Engine ("HL
Engine") and Modified Game Libraries ("MODs") developed by Valve,
L.L.C ("Valve").  You must obey the GNU General Public License in all
respects for all of the code used other than the HL Engine and MODs
from Valve.  If you modify this file, you may extend this exception
to your version of the file, but you are not obligated to do so.  If
you do not wish to do so, delete this exception statement from your
version.
*/

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "draw_util.h"
#include "triangleapi.h"
#include "hud_spectator.h"
#include "platform/steam_integration.h"
#include "ui/vgui2/cs_vgui2.h"
#include "ui/common/hud_style.h"
#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif
#include <voice_status.h>

static byte	r_RadarCross[8][8] =
{
{1,1,0,0,0,0,1,1},
{1,1,1,0,0,1,1,1},
{0,1,1,1,1,1,1,0},
{0,0,1,1,1,1,0,0},
{0,0,1,1,1,1,0,0},
{0,1,1,1,1,1,1,0},
{1,1,1,0,0,1,1,1},
{1,1,0,0,0,0,1,1}
};

static byte	r_RadarT[8][8] =
{
{1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0}
};

static byte	r_RadarFlippedT[8][8] =
{
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{0,0,0,1,1,0,0,0},
{1,1,1,1,1,1,1,1},
{1,1,1,1,1,1,1,1}
};

#define BLOCK_SIZE_MAX 1024

static byte	data2D[BLOCK_SIZE_MAX*4];	// intermediate texbuffer

// World units per radar pixel. With the stock 128 px sprite this is the radius
// the radar covers: 32 reaches 2048 units, which puts most of a map inside the
// dial and leaves every marker huddled near the middle.
static cvar_t *cl_radar_scale = NULL;

DECLARE_MESSAGE(m_Radar, Radar)
DECLARE_MESSAGE(m_Radar, BombDrop)
DECLARE_MESSAGE(m_Radar, BombPickup)
DECLARE_MESSAGE(m_Radar, HostagePos)
DECLARE_MESSAGE(m_Radar, HostageK)
DECLARE_MESSAGE(m_Radar, Location)
DECLARE_COMMAND(m_Radar, ShowRadar)
DECLARE_COMMAND(m_Radar, HideRadar)

int CHudRadar::Init()
{
	HOOK_MESSAGE(Radar);
	HOOK_COMMAND("drawradar", ShowRadar);
	HOOK_COMMAND("hideradar", HideRadar);
	HOOK_MESSAGE( HostageK );
	HOOK_MESSAGE( HostagePos );
	HOOK_MESSAGE( BombDrop );
	HOOK_MESSAGE( BombPickup );
	HOOK_MESSAGE( Location );

	m_iFlags = HUD_DRAW;

	cl_radartype = CVAR_CREATE( "cl_radartype", "0", FCVAR_ARCHIVE );
	cl_radar_alpha = CVAR_CREATE( "cl_radar_alpha", "180", FCVAR_ARCHIVE );
	cl_radar_scale = CVAR_CREATE( "cl_radar_scale", "32", FCVAR_ARCHIVE );
	cl_radar_show_location = CVAR_CREATE( "cl_radar_show_location", "1", FCVAR_ARCHIVE );

	// cl_radar_overview, cl_radar_style and cl_team_roster belong to the
	// vanilla/modern switch and are registered by CS16_HudStyleInit().
	CS16_HudStyleInit();

	gHUD.AddHudElem( this );
	return 1;
}

void CHudRadar::Reset()
{
	// make radar don't draw old players after new map
	for( int i = 0; i < 34; i++ )
	{
		g_PlayerExtraInfo[i].radarflashes = 0;

		if( i <= MAX_HOSTAGES ) g_HostageInfo[i].radarflashes = 0;
	}
}

static void Radar_InitBitmap( int w, int h, byte *buf )
{
	for( int x = 0; x < w; x++ )
	{
		for( int y = 0; y < h; y++ )
		{
			data2D[(y * 8 + x) * 4 + 0] = 255;
			data2D[(y * 8 + x) * 4 + 1] = 255;
			data2D[(y * 8 + x) * 4 + 2] = 255;
			data2D[(y * 8 + x) * 4 + 3] = buf[y*h + x]  * 255;
		}
	}
}

void CHudRadar::Shutdown( void )
{
	// GL_FreeTexture( hDot ); engine inner texture
	/*if( bTexturesInitialized )
	{
		gRenderAPI.GL_FreeTexture( hT );
		gRenderAPI.GL_FreeTexture( hFlippedT );
		gRenderAPI.GL_FreeTexture( hCross );
	}*/
}

void CHudRadar::InitHUDData( void )
{
	UserCmd_ShowRadar();
	Reset();
}

int CHudRadar::VidInit(void)
{
	/*bUseRenderAPI = g_iXash && InitBuiltinTextures();*/

	m_hRadar.SetSpriteByName( "radar" );
	m_hRadarOpaque.SetSpriteByName( "radaropaque" );
	iMaxRadius = (m_hRadar.rect.Width()) / 2.0f;
	return 1;
}

void CHudRadar::UserCmd_HideRadar()
{
	m_iFlags &= ~HUD_DRAW;
}

void CHudRadar::UserCmd_ShowRadar()
{
	m_iFlags |= HUD_DRAW;
}

int CHudRadar::MsgFunc_Radar(const char *pszName,  int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );

	int index = reader.ReadByte();
	g_PlayerExtraInfo[index].origin.x = reader.ReadCoord();
	g_PlayerExtraInfo[index].origin.y = reader.ReadCoord();
	g_PlayerExtraInfo[index].origin.z = reader.ReadCoord();
	return 1;
}

bool CHudRadar::FlashTime( float flTime, extra_player_info_t *pplayer )
{
	// radar flashing
	if( pplayer->radarflashes )
	{
		if( flTime > pplayer->radarflashtime )
		{
			pplayer->nextflash = !pplayer->nextflash;
			pplayer->radarflashtime += pplayer->radarflashtimedelta;
			pplayer->radarflashes--;
		}
	}
	else
	{
		return true;
	}

	return pplayer->nextflash;
}

bool CHudRadar::HostageFlashTime( float flTime, hostage_info_t *pplayer )
{
	// radar flashing
	if( pplayer->radarflashes )
	{
		if( flTime > pplayer->radarflashtime )
		{
			pplayer->nextflash = !pplayer->nextflash;
			pplayer->radarflashtime += pplayer->radarflashtimedelta;
			pplayer->radarflashes--;
		}
	}
	else
	{
		return false; // non-flashing hostage must be never drawn on radar!
	}

	return pplayer->nextflash;
}

void CHudRadar::DrawZAxis( Vector pos, int r, int g, int b, int a )
{
	const float diff = 128;

	if( pos.z > -diff && pos.z < diff )
	{
		DrawRadarDot( pos.x, pos.y, r, g, b, a );
	}
	else if( pos.z <= -diff )
	{
		// higher than player
		DrawT( pos.x, pos.y, r, g, b, a );
	}
	else
	{
		// lower than player
		DrawFlippedT( pos.x, pos.y, r, g, b, a );
	}
}

int CHudRadar::Draw(float flTime)
{
	if ( (gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) ||
		 gEngfuncs.IsSpectateOnly() ||
		 !(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT))) ||
		 gHUD.m_fPlayerDead )
		return 1;

	int iTeamNumber = g_PlayerExtraInfo[ gHUD.m_Scoreboard.m_iPlayerNum ].teamnumber;
	int r, g, b;
	gHUD.m_Scoreboard.GetAllPlayersInfo();

	if( CS16_HudStyleModern( CS16_HUD_TOPBAR ) )
		DrawTopRoster();

	if( CS16_HudStyleModern( CS16_HUD_RADAR ) &&
		DrawOverviewRadar( flTime, iTeamNumber ) )
		return 0;

	if( cl_radartype->value )
	{
		SPR_Set(m_hRadarOpaque.spr, 200, 200, 200);
		SPR_DrawHoles(0, 0, 0, &m_hRadarOpaque.rect);
	}
	else
	{
		SPR_Set( m_hRadar.spr, 25, 75, 25 );
		SPR_DrawAdditive( 0, 0, 0, &m_hRadarOpaque.rect );
	}

	if( CS16_HudStyleModern( CS16_HUD_RADAR_GUIDE ) )
		DrawGuide();

	for(int i = 0; i < 33; i++)
	{
		// skip local player and dead players
		if( i == gHUD.m_Scoreboard.m_iPlayerNum || g_PlayerExtraInfo[i].dead )
			continue;

		// skip non-teammates
		if( g_PlayerExtraInfo[i].teamnumber != iTeamNumber )
			continue;

		// decide should player draw at this time. For flashing.
		// Always true for non-flashing players
		if( !FlashTime( flTime, &g_PlayerExtraInfo[i]) )
			continue;

		// Important roles remain immediately recognizable. Ordinary teammates
		// use team colors and active voice speakers turn green.
		if( g_PlayerExtraInfo[i].has_c4 || g_PlayerExtraInfo[i].vip )
		{
			r = 255; g = 75; b = 45;
		}
		else if( g_PlayerExtraInfo[i].talking )
		{
			r = 70; g = 255; b = 100;
		}
		else if( iTeamNumber == TEAM_CT )
		{
			r = 90; g = 175; b = 255;
		}
		else
		{
			r = 255; g = 180; b = 70;
		}

		// calc radar position
		Vector pos = WorldToRadar(gHUD.m_vecOrigin, g_PlayerExtraInfo[i].origin, gHUD.m_vecAngles);

		if( CS16_HudStyleModern( CS16_HUD_RADAR_GUIDE ) )
			DrawZAxis( Vector(pos.x + 1, pos.y + 1, pos.z), 0, 0, 0, 180 );
		DrawZAxis( pos, r, g, b,
			min( 255, max( 40, (int)cl_radar_alpha->value ) ) );
	}

	// Terrorist specific code( C4 Bomb )
	if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_TERRORIST )
	{
		// BombDrop only carries a position while the bomb lies on the ground;
		// BombPickup marks slot 33 dead the moment somebody grabs it. Point the
		// marker at the carrier instead, so the team never loses track of it.
		if( g_PlayerExtraInfo[33].dead )
		{
			for( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				if( i == gHUD.m_Scoreboard.m_iPlayerNum )
					continue; // our own C4 is on the HUD, not the radar

				if( !g_PlayerExtraInfo[i].has_c4 || g_PlayerExtraInfo[i].dead ||
					g_PlayerExtraInfo[i].teamnumber != TEAM_TERRORIST )
					continue;

				Vector pos = WorldToRadar( gHUD.m_vecOrigin,
					g_PlayerExtraInfo[i].origin, gHUD.m_vecAngles );
				DrawZAxis( pos, 255, 0, 0, 255 );
				break;
			}
		}
		else if ( !g_PlayerExtraInfo[33].dead &&
			 g_PlayerExtraInfo[33].radarflashes &&
			 FlashTime( flTime, &g_PlayerExtraInfo[33] ))
		{
			Vector pos = WorldToRadar(gHUD.m_vecOrigin, g_PlayerExtraInfo[33].origin, gHUD.m_vecAngles);
			if( g_PlayerExtraInfo[33].playerclass ) // bomb planted
			{
				DrawCross( pos.x, pos.y, 255, 0, 0, 255 );
			}
			else
			{
				DrawZAxis( pos, 255, 0, 0, 255 );
			}
		}
	}
	// Counter-Terrorist specific code( hostages )
	else if( g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber == TEAM_CT )
	{
		// draw hostages for CT
		for( int i = 0; i < MAX_HOSTAGES; i++ )
		{
			if( !HostageFlashTime( flTime, g_HostageInfo + i ) )
			{
				continue;
			}

			Vector pos = WorldToRadar(gHUD.m_vecOrigin, g_HostageInfo[i].origin, gHUD.m_vecAngles);
			if( g_HostageInfo[i].dead )
			{
				DrawZAxis( pos, 255, 0, 0, 255 );
			}
			else
			{
				DrawZAxis( pos, 4, 25, 110, 255 );
			}
		}
	}

	if( cl_radar_show_location->value > 0.0f )
		DrawPlayerLocation( ( m_hRadarOpaque.rect.Height() ) + 10 );

	return 0;
}

bool CHudRadar::OverviewToPanel(const Vector& world, int x, int y,
	int wide, int tall, int& screenX, int& screenY) const
{
	const overviewInfo_t& overview = gHUD.m_Spectator.m_OverviewData;
	if( overview.layers <= 0 || overview.zoom <= 0.0f )
		return false;

	const float aspect = 4.0f / 3.0f;
	float u, v;
	if( overview.rotated )
	{
		const float worldWide = 8192.0f / overview.zoom;
		const float worldTall = 8192.0f / (overview.zoom * aspect);
		u = (world.x - (overview.origin[0] - worldWide * 0.5f)) / worldWide;
		v = ((overview.origin[1] + worldTall * 0.5f) - world.y) / worldTall;
	}
	else
	{
		const float worldWide = 8192.0f / (overview.zoom * aspect);
		const float worldTall = 8192.0f / overview.zoom;
		u = (world.x - (overview.origin[0] - worldWide * 0.5f)) / worldWide;
		v = ((overview.origin[1] + worldTall * 0.5f) - world.y) / worldTall;
	}

	if( u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f )
		return false;
	const int mapX = overview.rotated ? x : x + wide / 8;
	const int mapY = overview.rotated ? y + tall / 8 : y;
	const int mapWide = overview.rotated ? wide : wide * 3 / 4;
	const int mapTall = overview.rotated ? tall * 3 / 4 : tall;
	screenX = mapX + (int)(u * (mapWide - 1));
	screenY = mapY + (int)(v * (mapTall - 1));
	return true;
}

bool CHudRadar::DrawOverviewRadar(float flTime, int teamNumber)
{
	const overviewInfo_t& overview = gHUD.m_Spectator.m_OverviewData;
	if( overview.layers <= 0 || !overview.layersImages[0][0] )
		return false;

	const int wide = min(240, max(180, ScreenWidth / 8));
	const int tall = wide;
	const int left = 8;
	const int top = max(22, gHUD.GetCharHeight() + 8);
	if( !CS16VGUI2_DrawHudOverview(overview.layersImages[0],
		!overview.rotated, left, top,
		wide, tall, min(255, max(80, (int)cl_radar_alpha->value + 55))) )
		return false;

	// Darken the stock overview just enough to keep markers readable.
	CS16VGUI2_DrawHudRect(left, top, wide, tall, 0, 0, 0, 82);
	CS16VGUI2_DrawHudRect(left, top, wide, 2, 235, 155, 35, 220);
	CS16VGUI2_DrawHudRect(left, top + tall - 2, wide, 2, 235, 155, 35, 220);
	CS16VGUI2_DrawHudRect(left, top, 2, tall, 235, 155, 35, 220);
	CS16VGUI2_DrawHudRect(left + wide - 2, top, 2, tall, 235, 155, 35, 220);

	for( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if( i != gHUD.m_Scoreboard.m_iPlayerNum &&
			(g_PlayerExtraInfo[i].dead ||
			 g_PlayerExtraInfo[i].teamnumber != teamNumber ||
			 !FlashTime(flTime, &g_PlayerExtraInfo[i])) )
			continue;
		if( i == gHUD.m_Scoreboard.m_iPlayerNum ||
			g_PlayerExtraInfo[i].teamnumber == teamNumber )
		{
			int markerX, markerY;
			const Vector origin = i == gHUD.m_Scoreboard.m_iPlayerNum
				? gHUD.m_vecOrigin : g_PlayerExtraInfo[i].origin;
			if( !OverviewToPanel(origin, left, top, wide, tall,
				markerX, markerY) )
				continue;
			int mr = teamNumber == TEAM_CT ? 90 : 255;
			int mg = teamNumber == TEAM_CT ? 175 : 180;
			int mb = teamNumber == TEAM_CT ? 255 : 70;
			int size = 5;
			if( i == gHUD.m_Scoreboard.m_iPlayerNum )
			{ mr = 255; mg = 225; mb = 80; size = 7; }
			else if( g_PlayerExtraInfo[i].talking )
			{ mr = 65; mg = 255; mb = 90; size = 7; }
			else if( g_PlayerExtraInfo[i].has_c4 || g_PlayerExtraInfo[i].vip )
			{ mr = 255; mg = 70; mb = 45; size = 7; }
			CS16VGUI2_DrawHudRect(markerX - size / 2 - 1,
				markerY - size / 2 - 1, size + 2, size + 2, 0, 0, 0, 210);
			CS16VGUI2_DrawHudRect(markerX - size / 2,
				markerY - size / 2, size, size, mr, mg, mb, 255);
		}
	}

	if( teamNumber == TEAM_TERRORIST && !g_PlayerExtraInfo[33].dead &&
		g_PlayerExtraInfo[33].radarflashes &&
		FlashTime(flTime, &g_PlayerExtraInfo[33]) )
	{
		int markerX, markerY;
		if( OverviewToPanel(g_PlayerExtraInfo[33].origin, left, top, wide, tall,
			markerX, markerY) )
		{
			CS16VGUI2_DrawHudRect(markerX - 5, markerY - 1, 11, 3,
				255, 55, 35, 255);
			CS16VGUI2_DrawHudRect(markerX - 1, markerY - 5, 3, 11,
				255, 55, 35, 255);
		}
	}

	if( cl_radar_show_location->value > 0.0f )
		DrawPlayerLocation(top + tall + 5);
	return true;
}

void CHudRadar::DrawTopRoster()
{
	if( g_iUser1 )
		return;

	int teams[2][MAX_PLAYERS];
	int counts[2] = { 0, 0 };
	int alive[2] = { 0, 0 };
	for( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
			continue;
		const int slot = g_PlayerExtraInfo[i].teamnumber == TEAM_TERRORIST ? 0 :
			(g_PlayerExtraInfo[i].teamnumber == TEAM_CT ? 1 : -1);
		if( slot < 0 ) continue;
		teams[slot][counts[slot]++] = i;
		if( !g_PlayerExtraInfo[i].dead ) ++alive[slot];
	}
	if( !counts[0] && !counts[1] ) return;

	const int size = ScreenHeight >= 900 ? 28 : 22;
	const int step = size + 5;
	const int centerWide = 96;
	const int y = 7;
	for( int side = 0; side < 2; ++side )
	{
		const int edge = ScreenWidth / 2 + (side ? centerWide / 2 + 8 :
			-centerWide / 2 - 8);
		for( int ordinal = 0; ordinal < counts[side]; ++ordinal )
		{
			const int player = teams[side][ordinal];
			const int x = side ? edge + ordinal * step :
				edge - size - ordinal * step;
			const bool dead = g_PlayerExtraInfo[player].dead != 0;
			const int r = side ? 85 : 245;
			const int g = side ? 165 : 165;
			const int b = side ? 245 : 55;
			CS16VGUI2_DrawHudRect(x - 2, y - 2, size + 4, size + 4,
				r, g, b, dead ? 55 : 205);
			CS16VGUI2_DrawHudRect(x, y, size, size, 8, 8, 8, 210);
			CS16Steam_QueueAvatar(player, g_PlayerInfoList[player].m_nSteamID,
				x, y, size, dead ? 65 : 255, gHUD.m_flTime);
			if( g_PlayerExtraInfo[player].talking )
				CS16VGUI2_DrawHudRect(x + size - 6, y + size - 6, 6, 6,
					55, 255, 85, 255);
		}
	}

	char score[32];
	snprintf(score, sizeof(score), "%d  :  %d", alive[0], alive[1]);
	int textWide = 0, textTall = 0;
	CS16VGUI2_GetHudStringSize(score, &textWide, &textTall);
	CS16VGUI2_DrawHudRect(ScreenWidth / 2 - centerWide / 2, y - 2,
		centerWide, size + 4, 0, 0, 0, 170);
	CS16VGUI2_DrawHudString(ScreenWidth / 2 - textWide / 2,
		y + (size - textTall) / 2, score, 245, 185, 75, 255);
}

void CHudRadar::DrawGuide()
{
	const int center = (int)iMaxRadius;
	const int alpha = min( 150, max( 25, (int)cl_radar_alpha->value / 2 ) );
	const int outerRadius = max( 4, center - 3 );
	const int innerRadius = max( 3, center / 2 );

	FillRGBABlend( 3, center, outerRadius * 2 - 1, 1, 90, 150, 90, alpha / 2 );
	FillRGBABlend( center, 3, 1, outerRadius * 2 - 1, 90, 150, 90, alpha / 2 );
	for( int degrees = 0; degrees < 360; degrees += 6 )
	{
		const float angle = DEG2RAD( (float)degrees );
		const int ox = center + (int)(cos(angle) * outerRadius);
		const int oy = center + (int)(sin(angle) * outerRadius);
		FillRGBABlend( ox, oy, 1, 1, 120, 210, 120, alpha );
		if( degrees % 12 == 0 )
		{
			const int ix = center + (int)(cos(angle) * innerRadius);
			const int iy = center + (int)(sin(angle) * innerRadius);
			FillRGBABlend( ix, iy, 1, 1, 80, 135, 80, alpha / 2 );
		}
	}

	FillRGBABlend( center - 1, center - 2, 3, 4, 255, 255, 255, 220 );
	FillRGBABlend( center - 2, center, 5, 2, 255, 160, 0, 230 );

}

void CHudRadar::DrawPlayerLocation( int y )
{
	const char *szLocation = g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].location;
	if( szLocation[0] )
	{
		int x = (m_hRadarOpaque.rect.Width()) / 2;
		int len = DrawUtils::ConsoleStringLen( szLocation );

		x = x - len / 2;

		DrawUtils::DrawConsoleString( x, y, szLocation );
	}
}

inline void CHudRadar::DrawColoredTexture(int x, int y, int size,
	byte r, byte g, byte b, byte a, int texHandle)
{
	const float x1 = (iMaxRadius + x - size * 2) * gHUD.m_flScale;
	const float y1 = (iMaxRadius + y - size * 2) * gHUD.m_flScale;
	const float x2 = (iMaxRadius + x + size * 2) * gHUD.m_flScale;
	const float y2 = (iMaxRadius + y + size * 2) * gHUD.m_flScale;

	if (gEngfuncs.pTriAPI) {
		auto tri = gEngfuncs.pTriAPI;
		tri->RenderMode(kRenderTransAlpha);
		tri->Color4ub(r, g, b, a);
		tri->Begin(TRI_QUADS);
		tri->TexCoord2f(0, 0); tri->Vertex3f(x1, y1, 0);
		tri->TexCoord2f(1, 0); tri->Vertex3f(x2, y1, 0);
		tri->TexCoord2f(1, 1); tri->Vertex3f(x2, y2, 0);
		tri->TexCoord2f(0, 1); tri->Vertex3f(x1, y2, 0);
		tri->End();
	}
	else {
		gEngfuncs.pfnFillRGBA((int)x1, (int)y1, (int)(x2 - x1), (int)(y2 - y1), r, g, b, a);
	}
}



void CHudRadar::DrawRadarDot( int x, int y, int r, int g, int b, int a )
{
	const int size = 1;
	FillRGBA(iMaxRadius + x - size*2, iMaxRadius + y - size*2, size*4, size*4, r, g, b, a);
}


void CHudRadar::DrawCross( int x, int y, int r, int g, int b, int a )
{
	const int size = 2;
	FillRGBA(iMaxRadius + x, iMaxRadius + y, size, size, r, g, b, a);
	FillRGBA(iMaxRadius + x - size, iMaxRadius + y - size, size, size, r, g, b, a);
	FillRGBA(iMaxRadius + x - size, iMaxRadius + y + size, size, size, r, g, b, a);
	FillRGBA(iMaxRadius + x + size, iMaxRadius + y - size, size, size, r, g, b, a);
	FillRGBA(iMaxRadius + x + size, iMaxRadius + y + size, size, size, r, g, b, a);
}

void CHudRadar::DrawT( int x, int y, int r, int g, int b, int a )
{
	const int size = 2;
	FillRGBA( iMaxRadius + x - size, iMaxRadius + y - size, size * 3, size, r, g, b, a);
	FillRGBA( iMaxRadius + x, iMaxRadius + y, size, size * 2, r, g, b, a);
}

void CHudRadar::DrawFlippedT( int x, int y, int r, int g, int b, int a )
{
	const int size = 2;
	FillRGBA( iMaxRadius + x, iMaxRadius + y - size, size, size*2, r, g, b, a);
	FillRGBA(iMaxRadius + x - size, iMaxRadius + y + size, size * 3, size, r, g, b, a);
}


Vector CHudRadar::WorldToRadar(const Vector vPlayerOrigin, const Vector vObjectOrigin, const Vector vAngles  )
{
	Vector2D diff = vObjectOrigin.Make2D() - vPlayerOrigin.Make2D();

	float RADAR_SCALE = cl_radar_scale ? cl_radar_scale->value : 32.0f;
	if( RADAR_SCALE < 4.0f )
		RADAR_SCALE = 4.0f;
	else if( RADAR_SCALE > 128.0f )
		RADAR_SCALE = 128.0f;

	// Supply epsilon values to avoid divide-by-zero
	if( diff.x == 0 )
		diff.x = 0.00001f;
	if( diff.y == 0 )
		diff.y = 0.00001f;

	float flOffset = DEG2RAD( vAngles.y - RAD2DEG( atan2( diff.y, diff.x ) ) );

	// this magic 32.0f just scales position on radar
	float iRadius = min( diff.Length() / RADAR_SCALE, iMaxRadius );

	// transform origin difference to radar source
	Vector ret( (float)(iRadius * sin(flOffset)),
				(float)(iRadius * -cos(flOffset)),
				(float)(vPlayerOrigin.z - vObjectOrigin.z) );

	return ret;
}

int CHudRadar::MsgFunc_BombDrop(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	g_PlayerExtraInfo[33].origin.x = reader.ReadCoord();
	g_PlayerExtraInfo[33].origin.y = reader.ReadCoord();
	g_PlayerExtraInfo[33].origin.z = reader.ReadCoord();

	g_PlayerExtraInfo[33].radarflashes = 99999;
	g_PlayerExtraInfo[33].radarflashtime = gHUD.m_flTime;
	g_PlayerExtraInfo[33].radarflashtimedelta = 0.5f;
	strncpy(g_PlayerExtraInfo[33].teamname, "TERRORIST", MAX_TEAM_NAME);
	g_PlayerExtraInfo[33].dead = false;
	g_PlayerExtraInfo[33].nextflash = true;

	int Flag = reader.ReadByte();
	g_PlayerExtraInfo[33].playerclass = Flag;

	if( Flag ) // bomb planted
	{
		// The round timer is replaced by the bomb state from here on, so the
		// flag has to be raised, not cleared: the spectator HUD reads it to
		// show "C4 PLANTED" in place of the countdown. Reset() lowers it again
		// at the start of the next round.
		gHUD.m_SpectatorGui.m_bBombPlanted = true;
		gHUD.m_Timer.m_iFlags = 0;
	}
	return 1;
}

int CHudRadar::MsgFunc_BombPickup(const char *pszName, int iSize, void *pbuf)
{
	g_PlayerExtraInfo[33].radarflashes = false;
	g_PlayerExtraInfo[33].dead = true;

	return 1;
}

int CHudRadar::MsgFunc_HostagePos(const char *pszName, int iSize, void *pbuf)
{

	BufferReader reader( pszName, pbuf, iSize );
	int Flag = reader.ReadByte();
	int idx = reader.ReadByte();
	if( idx <= MAX_HOSTAGES )
	{
		g_HostageInfo[idx].origin.x = reader.ReadCoord();
		g_HostageInfo[idx].origin.y = reader.ReadCoord();
		g_HostageInfo[idx].origin.z = reader.ReadCoord();
		g_HostageInfo[idx].dead = false;

		if( Flag == 1 ) // first message about this hostage, start flashing
		{
			g_HostageInfo[idx].radarflashes = 99999;
			g_HostageInfo[idx].radarflashtime = gHUD.m_flTime;
			g_HostageInfo[idx].radarflashtimedelta = 0.5f;
		}
	}

	return 1;
}

int CHudRadar::MsgFunc_HostageK(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );
	int idx = reader.ReadByte();
	if ( idx <= MAX_HOSTAGES )
	{
		g_HostageInfo[idx].dead = true;
		g_HostageInfo[idx].radarflashtime = gHUD.m_flTime;
		g_HostageInfo[idx].radarflashes = 15;
		g_HostageInfo[idx].radarflashtimedelta = 0.1f;
	}

	return 1;
}

int CHudRadar::MsgFunc_Location(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	int player = reader.ReadByte();
	if( player <= MAX_PLAYERS )
	{
		const char *location = reader.ReadString();

		strncpy( g_PlayerExtraInfo[player].location, location, sizeof( g_PlayerExtraInfo[player].location ) );
		g_PlayerExtraInfo[player].location[31] = 0;

		// The GoldSrc/VGUI1 voice manager used by this project has no location
		// labels. The old call belonged to CS16Client's newer VGUI2 voice HUD.
	}
	return 0;
}
