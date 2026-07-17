// Steam GoldSrc VGUI2 interface bootstrap.
//
// Keep the VGUI2 interfaces opaque here. The Steam DLL uses a Microsoft C++
// ABI, so ordinary client code should not depend on its C++ headers.

#ifndef CS16_VGUI2_H
#define CS16_VGUI2_H

#ifdef __cplusplus
extern "C" {
#endif

int CS16VGUI2_Startup(void);
void CS16VGUI2_Shutdown(void);
void CS16VGUI2_RegisterCommands(void);

int CS16VGUI2_IsAvailable(void);
void* CS16VGUI2_GetInterface(const char* versionName);

int CS16VGUI2_IsViewportReady(void);
int CS16VGUI2_ShowMenu(int menuId);
void CS16VGUI2_HideMenu(void);
int CS16VGUI2_KeyInput(int down, int keynum, const char* currentBinding);
void CS16VGUI2_SetTeam(int team);
void CS16VGUI2_SetLegacyCursorVisible(int visible);
void CS16VGUI2_BeginHudTextFrame(void);
void CS16VGUI2_SetHudAvatar(int playerIndex, unsigned long long steamId,
    const unsigned char* rgba, int wide, int tall);
void CS16VGUI2_DrawHudAvatar(int playerIndex, int x, int y, int size, int alpha);
int CS16VGUI2_DrawHudOverview(const char* filename, int rotateClockwise,
    int x, int y, int wide, int tall, int alpha);
void CS16VGUI2_DrawHudRect(int x, int y, int wide, int tall,
    int r, int g, int b, int a);
int CS16VGUI2_DrawHudString(int x, int y, const char* text,
    int r, int g, int b, int a);
int CS16VGUI2_GetHudStringSize(const char* text, int* wide, int* tall);
void CS16VGUI2_ShutdownViewport(void);

#ifdef __cplusplus
}
#endif

#endif
