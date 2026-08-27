// Vanilla/modern HUD look selection. See hud_style.h for the cvar contract.

#include "hud.h"
#include "cl_util.h"
#include "hud_style.h"

#include <stdlib.h>
#include <string.h>

namespace
{
struct FeatureCvar
{
	const char *name;
	const char *description;
	cvar_t     *cvar;
};

FeatureCvar g_features[CS16_HUD_FEATURE_COUNT] =
{
	{ "cl_radar_overview",       "overview map radar",          NULL },
	{ "cl_radar_style",          "radar rings and guides",      NULL },
	{ "cl_team_roster",          "top team roster panel",       NULL },
	{ "cl_spectator_hud_modern", "spectator layout",            NULL },
	{ "cl_scoreboard_avatars",   "scoreboard Steam avatars",    NULL },
	{ "cl_killfeed_modern",      "kill feed panels and fading", NULL },
	{ "cl_voice_modern",         "speaker list panels",         NULL },
};

cvar_t *g_pMaster = NULL;
bool    g_bInitialized = false;

bool MasterEnabled( void )
{
	return g_pMaster && g_pMaster->value != 0.0f;
}

// Forces every element back to "follow the master switch" and moves the master
// itself, so a single command always gives a consistent look.
void SetStyle( bool modern )
{
	gEngfuncs.Cvar_SetValue( "cl_hud_modern", modern ? 1.0f : 0.0f );

	for( int i = 0; i < CS16_HUD_FEATURE_COUNT; i++ )
		gEngfuncs.Cvar_SetValue( g_features[i].name, -1.0f );
}

void HudModern_f( void )
{
	SetStyle( true );
	gEngfuncs.Con_Printf( "HUD style: modern. Use \"hud_vanilla\" to go back.\n" );
}

void HudVanilla_f( void )
{
	SetStyle( false );
	gEngfuncs.Con_Printf( "HUD style: vanilla. Use \"hud_modern\" for the reworked look.\n" );
}

void HudStyle_f( void )
{
	if( gEngfuncs.Cmd_Argc() > 1 )
	{
		const char *arg = gEngfuncs.Cmd_Argv( 1 );

		if( !stricmp( arg, "modern" ) || !stricmp( arg, "1" ) )
		{
			HudModern_f();
			return;
		}

		if( !stricmp( arg, "vanilla" ) || !stricmp( arg, "0" ) )
		{
			HudVanilla_f();
			return;
		}

		gEngfuncs.Con_Printf( "usage: hud_style [vanilla|modern]\n" );
		return;
	}

	gEngfuncs.Con_Printf( "cl_hud_modern %d - master switch\n", MasterEnabled() ? 1 : 0 );

	for( int i = 0; i < CS16_HUD_FEATURE_COUNT; i++ )
	{
		const cvar_t *cvar = g_features[i].cvar;
		const bool modern = CS16_HudStyleModern( (CS16HudFeature)i );

		gEngfuncs.Con_Printf( "%-26s %-4s %s (%s)\n",
			g_features[i].name,
			cvar && cvar->value >= 0.0f ? ( cvar->value != 0.0f ? "1" : "0" ) : "-1",
			modern ? "modern" : "vanilla",
			g_features[i].description );
	}
}
}

void CS16_HudStyleInit( void )
{
	if( g_bInitialized )
		return;

	g_bInitialized = true;

	g_pMaster = CVAR_CREATE( "cl_hud_modern", "0", FCVAR_ARCHIVE );

	for( int i = 0; i < CS16_HUD_FEATURE_COUNT; i++ )
		g_features[i].cvar = CVAR_CREATE( g_features[i].name, "-1", FCVAR_ARCHIVE );

	gEngfuncs.pfnAddCommand( "hud_modern",  HudModern_f );
	gEngfuncs.pfnAddCommand( "hud_vanilla", HudVanilla_f );
	gEngfuncs.pfnAddCommand( "hud_style",   HudStyle_f );
}

bool CS16_HudStyleModern( CS16HudFeature feature )
{
	if( feature < 0 || feature >= CS16_HUD_FEATURE_COUNT )
		return false;

	// HUD elements register in an order we don't control, so make sure the
	// cvars exist before the first query reads them.
	CS16_HudStyleInit();

	const cvar_t *cvar = g_features[feature].cvar;

	if( cvar && cvar->value >= 0.0f )
		return cvar->value != 0.0f;

	return MasterEnabled();
}
