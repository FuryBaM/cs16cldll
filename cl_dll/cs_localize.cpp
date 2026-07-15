#include "hud.h"
#include "cl_util.h"
#include "cs_localize.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
struct LocalizationEntry
{
    char* key;
    char* value;
};

LocalizationEntry* g_entries = NULL;
int g_entryCount = 0;
int g_entryCapacity = 0;
bool g_initialized = false;

char* CopyString(const char* value)
{
    if (!value)
        value = "";

    const size_t length = strlen(value);
    char* copy = (char*)malloc(length + 1);
    if (!copy)
        return NULL;

    memcpy(copy, value, length + 1);
    return copy;
}

int FindEntry(const char* key)
{
    if (!key || !key[0])
        return -1;

    for (int i = 0; i < g_entryCount; ++i)
    {
        if (!stricmp(g_entries[i].key, key))
            return i;
    }

    return -1;
}

void InsertEntry(const char* key, const char* value, bool replace)
{
    if (!key || !key[0] || !value)
        return;

    const int existing = FindEntry(key);
    if (existing >= 0)
    {
        if (!replace)
            return;

        char* copy = CopyString(value);
        if (!copy)
            return;

        free(g_entries[existing].value);
        g_entries[existing].value = copy;
        return;
    }

    if (g_entryCount == g_entryCapacity)
    {
        const int nextCapacity = g_entryCapacity ? g_entryCapacity * 2 : 512;
        LocalizationEntry* next = (LocalizationEntry*)realloc(
            g_entries, sizeof(LocalizationEntry) * nextCapacity);
        if (!next)
            return;

        g_entries = next;
        g_entryCapacity = nextCapacity;
    }

    char* keyCopy = CopyString(key);
    char* valueCopy = CopyString(value);
    if (!keyCopy || !valueCopy)
    {
        free(keyCopy);
        free(valueCopy);
        return;
    }

    g_entries[g_entryCount].key = keyCopy;
    g_entries[g_entryCount].value = valueCopy;
    ++g_entryCount;
}

void AppendUtf8(char*& output, char* outputEnd, unsigned int codepoint)
{
    if (codepoint <= 0x7f)
    {
        if (output < outputEnd)
            *output++ = (char)codepoint;
    }
    else if (codepoint <= 0x7ff)
    {
        if (outputEnd - output >= 2)
        {
            *output++ = (char)(0xc0 | (codepoint >> 6));
            *output++ = (char)(0x80 | (codepoint & 0x3f));
        }
    }
    else if (codepoint <= 0xffff)
    {
        if (outputEnd - output >= 3)
        {
            *output++ = (char)(0xe0 | (codepoint >> 12));
            *output++ = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            *output++ = (char)(0x80 | (codepoint & 0x3f));
        }
    }
    else if (outputEnd - output >= 4)
    {
        *output++ = (char)(0xf0 | (codepoint >> 18));
        *output++ = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        *output++ = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        *output++ = (char)(0x80 | (codepoint & 0x3f));
    }
}

char* ConvertResourceToUtf8(const byte* data, int length)
{
    if (!data || length <= 0)
        return NULL;

    if (length >= 2 && data[0] == 0xff && data[1] == 0xfe)
    {
        char* converted = (char*)malloc((size_t)length * 2 + 1);
        if (!converted)
            return NULL;

        char* output = converted;
        char* outputEnd = converted + (size_t)length * 2;
        int offset = 2;
        while (offset + 1 < length)
        {
            unsigned int codepoint = data[offset] | (data[offset + 1] << 8);
            offset += 2;
            if (!codepoint)
                break;

            if (codepoint >= 0xd800 && codepoint <= 0xdbff && offset + 1 < length)
            {
                const unsigned int low = data[offset] | (data[offset + 1] << 8);
                if (low >= 0xdc00 && low <= 0xdfff)
                {
                    offset += 2;
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                }
                else
                {
                    codepoint = '?';
                }
            }

            AppendUtf8(output, outputEnd, codepoint);
        }

        *output = '\0';
        return converted;
    }

    int offset = 0;
    if (length >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf)
        offset = 3;

    char* copy = (char*)malloc((size_t)(length - offset) + 1);
    if (!copy)
        return NULL;

    memcpy(copy, data + offset, length - offset);
    copy[length - offset] = '\0';
    return copy;
}

void LoadResource(const char* baseName, const char* language)
{
    if (!baseName || !language || !gEngfuncs.COM_LoadFile ||
        !gEngfuncs.COM_ParseFile || !gEngfuncs.COM_FreeFile)
        return;

    char fileName[128];
    snprintf(fileName, sizeof(fileName), "resource/%s_%s.txt", baseName, language);

    int length = 0;
    byte* source = gEngfuncs.COM_LoadFile(fileName, 5, &length);
    if (!source)
        return;

    char* text = ConvertResourceToUtf8(source, length);
    gEngfuncs.COM_FreeFile(source);
    if (!text)
        return;

    char token[4096];
    char value[4096];
    char* cursor = text;
    bool foundTokens = false;

    while ((cursor = gEngfuncs.COM_ParseFile(cursor, token)) != NULL)
    {
        if (!stricmp(token, "Tokens"))
        {
            cursor = gEngfuncs.COM_ParseFile(cursor, token);
            foundTokens = cursor && !strcmp(token, "{");
            break;
        }
    }

    if (foundTokens)
    {
        while ((cursor = gEngfuncs.COM_ParseFile(cursor, token)) != NULL)
        {
            if (!strcmp(token, "}"))
                break;

            cursor = gEngfuncs.COM_ParseFile(cursor, value);
            if (!cursor || !strcmp(value, "}"))
                break;

            InsertEntry(token, value, true);
        }
    }

    free(text);
}

bool IsSafeLanguageName(const char* language)
{
    if (!language || !language[0])
        return false;

    for (const unsigned char* p = (const unsigned char*)language; *p; ++p)
    {
        if (!isalnum(*p) && *p != '_' && *p != '-')
            return false;
    }

    return true;
}

void AddFallbackMessages()
{
    static const struct { const char* key; const char* value; } fallback[] =
    {
        { "Bomb_Planted", "The bomb has been planted!" },
        { "Game_bomb_planted", "The bomb has been planted!" },
        { "Bomb_Defused", "The bomb has been defused!" },
        { "Target_Bombed", "Target Successfully Bombed!" },
        { "Target_Saved", "Target has been saved!" },
        { "Terrorists_Win", "Terrorists Win!" },
        { "CTs_Win", "Counter-Terrorists Win!" },
        { "Round_Draw", "Round Draw!" },
        { "Game_Commencing", "Game Commencing!" },
        { "Cstrike_TitlesTXT_Switch_To_BurstFire", "Switched to burst-fire mode" },
        { "Cstrike_TitlesTXT_Switch_To_FullAuto", "Switched to automatic" },
        { "Cstrike_TitlesTXT_Switch_To_SemiAuto", "Switched to semi-automatic" },
        { "All_Hostages_Rescued", "All Hostages have been rescued!" },
        { "Hostages_Not_Rescued", "Hostages have not been rescued!" },
        { "VIP_Escaped", "The VIP has escaped!" },
        { "VIP_Assassinated", "VIP has been assassinated!" },
        { "Terrorists_Escaped", "The terrorists have escaped!" },
        { "CTs_PreventEscape", "The CTs have prevented most of the terrorists from escaping!" },
        { "Escaping_Terrorists_Neutralized", "Escaping terrorists have all been neutralized!" },
        { "Cstrike_BuyMenuAutobuy", "&A AUTO-BUY" },
        { "Cstrike_BuyMenuRebuy", "&R RE-BUY PREVIOUS" }
    };

    for (int i = 0; i < (int)(sizeof(fallback) / sizeof(fallback[0])); ++i)
        InsertEntry(fallback[i].key, fallback[i].value, false);
}

void InitializeLocalization()
{
    if (g_initialized)
        return;

    g_initialized = true;
    LoadResource("valve", "english");
    LoadResource("cstrike", "english");

    const char* language = NULL;
    static const char* languageCvars[] = { "cl_language", "language", "ui_language" };
    for (int i = 0; i < (int)(sizeof(languageCvars) / sizeof(languageCvars[0])); ++i)
    {
        cvar_t* cvar = gEngfuncs.pfnGetCvarPointer
            ? gEngfuncs.pfnGetCvarPointer(languageCvars[i])
            : NULL;
        if (cvar && IsSafeLanguageName(cvar->string) && stricmp(cvar->string, "english"))
        {
            language = cvar->string;
            break;
        }
    }

    if (language)
    {
        LoadResource("valve", language);
        LoadResource("cstrike", language);
    }

    AddFallbackMessages();
}

const char* ResolveToken(const char* token, int depth)
{
    if (!token || !token[0] || depth > 8)
        return token ? token : "";

    InitializeLocalization();

    const char* key = token[0] == '#' ? token + 1 : token;
    const int index = FindEntry(key);
    if (index >= 0)
        return g_entries[index].value;

    client_textmessage_t* engineMessage = gEngfuncs.pfnTextMessageGet
        ? gEngfuncs.pfnTextMessageGet(key)
        : NULL;
    if (engineMessage && engineMessage->pMessage && engineMessage->pMessage[0])
    {
        if (engineMessage->pMessage[0] == '#')
            return ResolveToken(engineMessage->pMessage, depth + 1);
        return engineMessage->pMessage;
    }

    return token;
}
}

const char* CS16_Localize(const char* token)
{
    return ResolveToken(token, 0);
}

size_t CS16_LocalizeFormat(char* dst, size_t dstSize, const char* format,
    const char* const* arguments, int argumentCount)
{
    if (!dst || !dstSize)
        return 0;

    if (!format)
    {
        dst[0] = '\0';
        return 0;
    }

    size_t written = 0;
    int nextArgument = 0;
    for (const char* p = format; *p && written + 1 < dstSize;)
    {
        if (p[0] == '%' && p[1] == '%')
        {
            dst[written++] = '%';
            p += 2;
            continue;
        }

        if (p[0] == '%' && p[1] == 's')
        {
            int argument = nextArgument++;
            int consumed = 2;
            if (p[2] >= '1' && p[2] <= '9')
            {
                argument = p[2] - '1';
                consumed = 3;
            }

            const char* replacement =
                argument >= 0 && argument < argumentCount && arguments[argument]
                ? arguments[argument]
                : "";
            while (*replacement && written + 1 < dstSize)
                dst[written++] = *replacement++;

            p += consumed;
            continue;
        }

        dst[written++] = *p++;
    }

    dst[written] = '\0';
    return written;
}
