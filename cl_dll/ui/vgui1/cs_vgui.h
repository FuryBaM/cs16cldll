// GoldSrc VGUI1 integration boundary.
//
// Keep this header free of VGUI C++ types. Steam's vgui.dll uses the
// Microsoft C++ ABI, while diagnostic builds of the rest of client.dll may
// use another compiler. The viewport therefore talks to the client only
// through these C functions.

#ifndef CS16_VGUI_H
#define CS16_VGUI_H

#define CS16_VGUI_MAX_RESOURCE_CONTROLS 64

typedef struct cs16_vgui_resource_control_s
{
    char fieldName[64];
    char controlName[32];
    char labelText[128];
    char command[128];
    char textAlignment[16];
    char font[32];
    int xpos;
    int ypos;
    int wide;
    int tall;
    int visible;
    int enabled;
} cs16_vgui_resource_control_t;

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

int CS16VGUI_LoadResourceLayout(const char* filename,
    cs16_vgui_resource_control_t* controls, int maxControls);
int CS16VGUI_LocalizeResourceText(const char* text, char* output, int outputSize);
int CS16VGUI_IsVIPMap(void);
int CS16VGUI_CanSpectate(void);
int CS16VGUI_HasTeam(void);
int CS16VGUI_GetMapName(char* output, int outputSize);
int CS16VGUI_LoadMapDescription(char* output, int outputSize);
int CS16VGUI_LoadTGA(const char* filename, unsigned char* rgba, int rgbaSize,
    int* width, int* height);
int CS16VGUI_GetRoundTime(char* output, int outputSize);

#ifdef __cplusplus
}
#endif

#endif
