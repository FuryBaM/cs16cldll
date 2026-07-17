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
#include "cl_util.h"
#include "pm_shared.h"

#include <math.h>

int CHudSniperScope::Init()
{
	gHUD.AddHudElem(this);
	// Steam draws the scope explicitly from CHudAmmo so the mask and reticle
	// always share one traversal and render order. Keep the element registered
	// for VidInit/Shutdown, but do not draw it a second time from the HUD list.
	m_iFlags = 0;
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
	if (g_iUser1 && g_iUser1 != OBS_IN_EYE)
		return 1;

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

	// Steam GoldSrc's TriAPI vertices participate in renderer state left by the
	// 3D scene, so a triangle-fan mask can be depth-tested against the map and
	// appear or disappear as the camera moves. Build the same circular mask
	// entirely from HUD-space FillRGBA bands instead. The band count is capped
	// near the original 540-line reference density to keep the edge smooth at
	// every resolution without tying it to world rendering.
	const int topPixel = max(0, (int)top);
	const int bottomPixel = min(ScreenHeight, (int)ceilf(bottom));
	const int bandHeight = max(1, (int)ceilf(diameter / 540.0f));

	if (topPixel > 0)
		gEngfuncs.pfnFillRGBABlend(0, 0, ScreenWidth, topPixel, 0, 0, 0, 255);
	if (bottomPixel < ScreenHeight)
		gEngfuncs.pfnFillRGBABlend(0, bottomPixel, ScreenWidth,
			ScreenHeight - bottomPixel, 0, 0, 0, 255);

	for (int y = topPixel; y < bottomPixel; y += bandHeight)
	{
		const int height = min(bandHeight, bottomPixel - y);
		const float dy = (y + height * 0.5f) - centery;
		const float inside = max(0.0f, radius * radius - dy * dy);
		const float halfWidth = sqrtf(inside);
		const int leftEdge = max(0, (int)floorf(centerx - halfWidth));
		const int rightEdge = min(ScreenWidth, (int)ceilf(centerx + halfWidth));

		if (leftEdge > 0)
			gEngfuncs.pfnFillRGBABlend(0, y, leftEdge, height, 0, 0, 0, 255);
		if (rightEdge < ScreenWidth)
			gEngfuncs.pfnFillRGBABlend(rightEdge, y, ScreenWidth - rightEdge,
				height, 0, 0, 0, 255);
	}

	// Match the original scope_arc renderer's one-pixel offsets. The centered
	// sniper_scope.spr remains unobscured and supplies its dotted reticle.
	gEngfuncs.pfnFillRGBABlend((int)left, (int)centery + 1,
		(int)(right - left), 1, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBABlend((int)centerx - 1, (int)top,
		1, (int)(bottom - top), 0, 0, 0, 255);

	return 1;
}

void CHudSniperScope::Shutdown() { }
