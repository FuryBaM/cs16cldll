/*
hud_overlays.cpp - HUD Overlays
Copyright (C) 2015-2016 a1batross

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
#include "triangleapi.h"
#include "r_efx.h"
#include "cl_util.h"

#include "draw_util.h"

#include <math.h>

namespace
{
void DrawScopeCorner(float outerX, float outerY, float centerX, float centerY,
	float radius, float startAngle, float endAngle)
{
	triangleapi_t* tri = gEngfuncs.pTriAPI;
	if (!tri)
		return;

	const int segments = 24;
	tri->Begin(TRI_TRIANGLE_FAN);
	tri->Vertex3f(outerX, outerY, 0.0f);
	for (int i = 0; i <= segments; ++i)
	{
		const float fraction = (float)i / (float)segments;
		const float angle = startAngle + (endAngle - startAngle) * fraction;
		tri->Vertex3f(centerX + cosf(angle) * radius,
			centerY + sinf(angle) * radius, 0.0f);
	}
	tri->End();
}
}

int CHudSniperScope::Init()
{
	gHUD.AddHudElem(this);
	m_iFlags = HUD_DRAW;
	m_iScopeArc[0] = m_iScopeArc[1] =m_iScopeArc[2] = m_iScopeArc[3]  = 0;
	return 1;
}

int CHudSniperScope::VidInit()
{
	centerx = ScreenWidth * 0.5f;
	centery = ScreenHeight * 0.5f;
	const float diameter = (float)min(ScreenWidth, ScreenHeight);
	left = centerx - diameter * 0.5f;
	right = centerx + diameter * 0.5f;
	return 1;
}

int CHudSniperScope::Draw(float)
{
	if (gHUD.m_iFOV <= 0 || gHUD.m_iFOV > 40)
		return 1;

	// Recalculate every frame so a late video-mode change cannot leave stale
	// scope geometry behind.
	centerx = ScreenWidth * 0.5f;
	centery = ScreenHeight * 0.5f;
	const float diameter = (float)min(ScreenWidth, ScreenHeight);
	const float radius = diameter * 0.5f;
	left = centerx - radius;
	right = centerx + radius;
	const float top = centery - radius;
	const float bottom = centery + radius;

	// Black bars outside the largest centered square.
	gEngfuncs.pfnFillRGBA(0, 0, (int)left + 1, ScreenHeight, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBA((int)right, 0, ScreenWidth - (int)right, ScreenHeight, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBA((int)left, 0, (int)(right - left), (int)top + 1, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBA((int)left, (int)bottom, (int)(right - left), ScreenHeight - (int)bottom, 0, 0, 0, 255);

	if (gEngfuncs.pTriAPI)
	{
		triangleapi_t* tri = gEngfuncs.pTriAPI;
		tri->RenderMode(kRenderNormal);
		tri->Brightness(1.0f);
		tri->Color4ub(0, 0, 0, 255);
		tri->CullFace(TRI_NONE);

		const float pi = 3.14159265358979323846f;
		DrawScopeCorner(left, top, centerx, centery, radius, -pi * 0.5f, -pi);
		DrawScopeCorner(right, top, centerx, centery, radius, 0.0f, -pi * 0.5f);
		DrawScopeCorner(right, bottom, centerx, centery, radius, pi * 0.5f, 0.0f);
		DrawScopeCorner(left, bottom, centerx, centery, radius, pi, pi * 0.5f);
	}

	// Pixel-perfect crosshair lines across the circular opening.
	gEngfuncs.pfnFillRGBA((int)left, (int)centery, (int)(right - left), 1, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBA((int)centerx, (int)top, 1, (int)(bottom - top), 0, 0, 0, 255);

	return 1;
}

void CHudSniperScope::Shutdown() { }
