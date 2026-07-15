// GoldSrc VGUI1 integration boundary.
//
// Keep this header free of VGUI C++ types. Steam's vgui.dll uses the
// Microsoft C++ ABI, while diagnostic builds of the rest of client.dll may
// use another compiler. The viewport therefore talks to the client only
// through these C functions.

#ifndef CS16_VGUI_H
#define CS16_VGUI_H

#ifdef __cplusplus
extern "C" {
#endif

int CS16VGUI_Startup(int width, int height);
void CS16VGUI_Shutdown(void);
void CS16VGUI_ResetSession(void);

int CS16VGUI_IsAvailable(void);
int CS16VGUI_ShowMenu(int menuId);
void CS16VGUI_HideMenu(void);
void CS16VGUI_DisableForSession(void);
int CS16VGUI_KeyInput(int down, int keynum, const char* currentBinding);

#ifdef __cplusplus
}
#endif

#endif
