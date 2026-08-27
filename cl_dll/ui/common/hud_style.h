// Vanilla/modern HUD look selection.
//
// The client boots with the stock Counter-Strike 1.6 layout. Every element that
// this port reworked is opt-in: either through the cl_hud_modern master switch
// or through the element's own cvar, which overrides the master when set to 0
// or 1 and follows it when left at -1.
#ifndef CS16_HUD_STYLE_H
#define CS16_HUD_STYLE_H

enum CS16HudFeature
{
	CS16_HUD_RADAR = 0,       // square overview map instead of the radar sprite
	CS16_HUD_RADAR_GUIDE,     // rings and guide lines drawn over the radar
	CS16_HUD_TOPBAR,          // top team roster with Steam avatars
	CS16_HUD_SPECTATOR,       // reworked spectator layout and player card
	CS16_HUD_SCOREBOARD,      // Steam avatars in the scoreboard
	CS16_HUD_KILLFEED,        // panelled, fading kill feed
	CS16_HUD_VOICE,           // panelled speaker list with Steam avatars
	CS16_HUD_FEATURE_COUNT
};

// Registers cl_hud_modern, the per-element cvars and the hud_modern,
// hud_vanilla and hud_style console commands. Safe to call more than once.
void CS16_HudStyleInit( void );

// True when the reworked version of the element should be drawn.
bool CS16_HudStyleModern( CS16HudFeature feature );

#endif // CS16_HUD_STYLE_H
