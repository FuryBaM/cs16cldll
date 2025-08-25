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

int CHudSniperScope::Init()
{
	m_iFlags = HUD_DRAW;
	m_iScopeArc[0] = m_iScopeArc[1] =m_iScopeArc[2] = m_iScopeArc[3]  = 0;
	return 1;
}

int CHudSniperScope::VidInit()
{
	// Ѕез Xash: просто рассчитываем геометрию
	left = (TrueWidth - TrueHeight) * 0.5f;
	if (left < 0) left = 0; // на узких экранах
	right = left + TrueHeight;
	centerx = TrueWidth * 0.5f;
	centery = TrueHeight * 0.5f;
	return 1;
}

int CHudSniperScope::Draw(float)
{
	if (gHUD.m_iFOV > 40) return 1;

	// боковые шторки
	gEngfuncs.pfnFillRGBA(0, 0, (int)(left + 2), (int)TrueHeight, 0, 0, 0, 255);
	gEngfuncs.pfnFillRGBA((int)right, 0, (int)(TrueWidth - right), (int)TrueHeight, 0, 0, 0, 255);

	// горизонтальна€ лини€
	gEngfuncs.pfnFillRGBA((int)left, (int)(centery + 1), (int)(right - left), 1, 0, 0, 0, 255);
	// вертикальна€ лини€
	gEngfuncs.pfnFillRGBA((int)(centerx - 1), 0, 1, (int)TrueHeight, 0, 0, 0, 255);

	return 0;
}

void CHudSniperScope::Shutdown() { }
