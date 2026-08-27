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
    bool  fallback;
};

LocalizationEntry* g_entries = NULL;
int g_entryCount = 0;
int g_entryCapacity = 0;
bool g_initialized = false;
int g_loadAttempts = 0;
int g_filesLoaded = 0;

// What each resource file contributed, so a missing or unreadable one can be
// told apart from one that parsed but came up short.
struct LoadRecord
{
    char fileName[128];
    int  bytes;
    int  entries;
    bool opened;
};

LoadRecord g_loadLog[8];
int g_loadLogCount = 0;

void RecordLoad(const char* fileName, int bytes, int entries, bool opened)
{
    if (g_loadLogCount >= (int)(sizeof(g_loadLog) / sizeof(g_loadLog[0])))
        return;

    LoadRecord& record = g_loadLog[g_loadLogCount++];
    strncpy(record.fileName, fileName, sizeof(record.fileName));
    record.fileName[sizeof(record.fileName) - 1] = 0;
    record.bytes = bytes;
    record.entries = entries;
    record.opened = opened;
}
bool g_commandsRegistered = false;

// Resource files only become readable once the engine has mounted the game
// directory, and the first lookup can happen before that. Give the loader a
// bounded number of retries instead of latching an empty table for the session.
const int kMaxLoadAttempts = 32;

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

void InsertEntry(const char* key, const char* value, bool replace,
    bool fallback = false)
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

        // Callers hold the returned pointers, and a reload replaces fallback
        // strings that may still be referenced, so the old value is left alone.
        // The waste is bounded by the handful of reloads a session can do.
        g_entries[existing].value = copy;
        g_entries[existing].fallback = fallback;
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
    g_entries[g_entryCount].fallback = fallback;
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

// Valve's resource files wrap long values across several lines, and the
// engine's COM_ParseFile stops at the newline inside the quotes. Every entry
// after the first such value then lands one token out of step, which is why
// tokens like Cstrike_TitlesTXT_Hint_you_have_the_bomb went missing while the
// ones earlier in the file resolved. Parse the block here instead.
const char* SkipResourceSpace(const char* cursor)
{
    while (*cursor)
    {
        if ((unsigned char)*cursor <= ' ')
        {
            ++cursor;
            continue;
        }

        if (cursor[0] == '/' && cursor[1] == '/')
        {
            while (*cursor && *cursor != '\n')
                ++cursor;
            continue;
        }

        break;
    }

    return cursor;
}

// Reads the next quoted string, stepping over braces and any [$WIN32]-style
// suffixes in between. Returns NULL at end of file.
const char* NextQuotedString(const char* cursor, char* output, size_t outputSize)
{
    for (;;)
    {
        cursor = SkipResourceSpace(cursor);
        if (!*cursor)
            return NULL;
        if (*cursor == '"')
            break;
        ++cursor;
    }

    ++cursor;

    size_t written = 0;
    while (*cursor && *cursor != '"')
    {
        char value = *cursor++;
        if (value == '\\' && *cursor)
        {
            // KeyValues resources use an escaped quote in strings such as
            // Alias_Not_Avail. Decode the common escapes without treating the
            // escaped quote as the end of the value.
            if (*cursor == '"' || *cursor == '\\')
                value = *cursor++;
            else if (*cursor == 'n')
            {
                value = '\n';
                ++cursor;
            }
            else if (*cursor == 'r')
            {
                value = '\r';
                ++cursor;
            }
            else if (*cursor == 't')
            {
                value = '\t';
                ++cursor;
            }
        }

        if (written + 1 < outputSize)
            output[written++] = value;
    }

    output[written] = '\0';

    if (*cursor == '"')
        ++cursor;

    return cursor;
}

int LoadResource(const char* baseName, const char* language)
{
    if (!baseName || !language || !gEngfuncs.COM_LoadFile ||
        !gEngfuncs.COM_FreeFile)
        return 0;

    char fileName[128];
    snprintf(fileName, sizeof(fileName), "resource/%s_%s.txt", baseName, language);

    int length = 0;
    byte* source = gEngfuncs.COM_LoadFile(fileName, 5, &length);
    if (!source)
    {
        RecordLoad(fileName, 0, 0, false);
        return 0;
    }

    char* text = ConvertResourceToUtf8(source, length);
    gEngfuncs.COM_FreeFile(source);
    if (!text)
    {
        RecordLoad(fileName, length, 0, true);
        return 0;
    }

    int inserted = 0;

    static char token[1024];
    static char value[4096];
    const char* cursor = text;
    bool foundTokens = false;

    while ((cursor = NextQuotedString(cursor, token, sizeof(token))) != NULL)
    {
        if (!stricmp(token, "Tokens"))
        {
            foundTokens = true;
            break;
        }
    }

    if (foundTokens)
    {
        while ((cursor = NextQuotedString(cursor, token, sizeof(token))) != NULL)
        {
            cursor = NextQuotedString(cursor, value, sizeof(value));
            if (!cursor)
                break;

            InsertEntry(token, value, true);
            ++inserted;
        }
    }

    free(text);
    RecordLoad(fileName, length, inserted, true);
    return inserted;
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
    // Verbatim copies of the Cstrike_TitlesTXT_* strings from Valve's
    // cstrike_english.txt. titles.txt redirects most in-game messages to
    // these tokens, and an install whose resource files predate a token ends
    // up showing the token itself. Registered with the lowest priority, so a
    // resource file -- in any language -- always wins.
    static const struct { const char* key; const char* value; } fallback[] =
    {
        { "Cstrike_TitlesTXT_AK47", "CV-47" },
        { "Cstrike_TitlesTXT_Accept_All_Messages", "Now accepting ALL text messages" },
        { "Cstrike_TitlesTXT_Accept_Radio", "Now ACCEPTING radio messages" },
        { "Cstrike_TitlesTXT_Affirmative", "Affirmative." },
        { "Cstrike_TitlesTXT_Alias_Not_Avail", "The \"%s1\"\nis not available for your team to buy." },
        { "Cstrike_TitlesTXT_All_Hostages_Rescued", "All Hostages have been rescued!" },
        { "Cstrike_TitlesTXT_All_Teams_Full", "All teams are full!" },
        { "Cstrike_TitlesTXT_All_VIP_Slots_Full", "All 5 VIP slots have been filled up.\nPlease try again later." },
        { "Cstrike_TitlesTXT_Already_Have_Kevlar", "You already have kevlar!" },
        { "Cstrike_TitlesTXT_Already_Have_Kevlar_Helmet", "You already have kevlar and a helmet!" },
        { "Cstrike_TitlesTXT_Already_Have_One", "You already have one!" },
        { "Cstrike_TitlesTXT_ArcticWarfareMagnum", "Magnum Sniper Rifle" },
        { "Cstrike_TitlesTXT_Arctic_Avengers", "Arctic Avengers" },
        { "Cstrike_TitlesTXT_Aug", "Bullpup" },
        { "Cstrike_TitlesTXT_AutoShotgun", "Auto Shotgun" },
        { "Cstrike_TitlesTXT_Auto_Select", "Auto-Select" },
        { "Cstrike_TitlesTXT_Auto_Team_Balance_Next_Round", "*** Auto-Team Balance next round ***" },
        { "Cstrike_TitlesTXT_BOMB", "BOMB" },
        { "Cstrike_TitlesTXT_Banned_For_Killing_Teammates", "You are being banned from the server for killing too many teammates" },
        { "Cstrike_TitlesTXT_Beretta96G", ".40 Dual Elites" },
        { "Cstrike_TitlesTXT_Bomb_Defusal_Kit", "Bomb Defusal Kit" },
        { "Cstrike_TitlesTXT_Bomb_Defused", "The bomb has been defused!" },
        { "Cstrike_TitlesTXT_Bomb_Planted", "The bomb has been planted!" },
        { "Cstrike_TitlesTXT_Buy_equipment", "Buy Equipment" },
        { "Cstrike_TitlesTXT_Buy_machineguns", "Buy Machine Guns" },
        { "Cstrike_TitlesTXT_Buy_pistols", "Buy Pistols" },
        { "Cstrike_TitlesTXT_Buy_prim_ammo", "Buy Primary Ammo" },
        { "Cstrike_TitlesTXT_Buy_rifles", "Buy Rifles" },
        { "Cstrike_TitlesTXT_Buy_sec_ammo", "Buy Secondary Ammo" },
        { "Cstrike_TitlesTXT_Buy_shotguns", "Buy Shotguns" },
        { "Cstrike_TitlesTXT_Buy_smgs", "Buy Sub-Machine Guns" },
        { "Cstrike_TitlesTXT_C4_Activated_At_Bomb_Spot", "C4 must be activated at a Bomb Target" },
        { "Cstrike_TitlesTXT_C4_Arming_Cancelled", "Arming Sequence Canceled. \nC4 can only be placed at a Bomb Target." },
        { "Cstrike_TitlesTXT_C4_Defuse_Must_Be_On_Ground", "You must be on the ground\nto defuse the bomb!" },
        { "Cstrike_TitlesTXT_C4_Plant_At_Bomb_Spot", "C4 must be planted at a bomb site!" },
        { "Cstrike_TitlesTXT_C4_Plant_Must_Be_On_Ground", "You must be standing on\nthe ground to plant the C4!" },
        { "Cstrike_TitlesTXT_CAM_OPTIONS", "Camera Options" },
        { "Cstrike_TitlesTXT_CLASS", "Class" },
        { "Cstrike_TitlesTXT_CT_Forces", "CT Forces" },
        { "Cstrike_TitlesTXT_CT_cant_buy", "CTs aren't allowed to buy\nanything on this map!" },
        { "Cstrike_TitlesTXT_CTs_Full", "The CT team is full!" },
        { "Cstrike_TitlesTXT_CTs_PreventEscape", "The CTs have prevented most\nof the terrorists from escaping!" },
        { "Cstrike_TitlesTXT_CTs_Win", "Counter-Terrorists Win!" },
        { "Cstrike_TitlesTXT_Cannot_Be_Spectator", "You cannot become a spectator." },
        { "Cstrike_TitlesTXT_Cannot_Buy_This", "You cannot buy this item!" },
        { "Cstrike_TitlesTXT_Cannot_Carry_Anymore", "You cannot carry anymore!" },
        { "Cstrike_TitlesTXT_Cannot_Switch_From_VIP", "You are the VIP!\nYou cannot switch roles now." },
        { "Cstrike_TitlesTXT_Cannot_Vote_Map", "You cannot vote within 3 minutes of a new map" },
        { "Cstrike_TitlesTXT_Cannot_Vote_Need_More_People", "You can't vote for a map by yourself!" },
        { "Cstrike_TitlesTXT_Cannot_Vote_With_Less_Than_Three", "You can't vote with less than three people on your team" },
        { "Cstrike_TitlesTXT_Cant_buy", "%s1 seconds have passed.\nYou can't buy anything now!" },
        { "Cstrike_TitlesTXT_Class_descr_not_avail", "Class description not available." },
        { "Cstrike_TitlesTXT_Command_Not_Available", "This command is not available to you at this point" },
        { "Cstrike_TitlesTXT_Cover_me", "Cover Me!" },
        { "Cstrike_TitlesTXT_Cstrike_Already_Own_Weapon", "You already own that weapon." },
        { "Cstrike_TitlesTXT_D3AU1", "D3AU1" },
        { "Cstrike_TitlesTXT_DEAD", "DEAD" },
        { "Cstrike_TitlesTXT_DEATHS", "DEATHS" },
        { "Cstrike_TitlesTXT_Defusal_Kit", "Defusal Kit" },
        { "Cstrike_TitlesTXT_Defusing_Bomb_With_Defuse_Kit", "Defusing bomb WITH Defuse kit." },
        { "Cstrike_TitlesTXT_Defusing_Bomb_Without_Defuse_Kit", "Defusing bomb WITHOUT Defuse kit." },
        { "Cstrike_TitlesTXT_DesertEagle", "Night Hawk .50C" },
        { "Cstrike_TitlesTXT_Dual40", ".40 Dual" },
        { "Cstrike_TitlesTXT_ESC90", "ES C90" },
        { "Cstrike_TitlesTXT_ESFiveSeven", "Five-Seven" },
        { "Cstrike_TitlesTXT_ESM249", "ES M249" },
        { "Cstrike_TitlesTXT_Enemy", "Enemy" },
        { "Cstrike_TitlesTXT_Enemy_down", "Enemy down." },
        { "Cstrike_TitlesTXT_Enemy_spotted", "Enemy spotted." },
        { "Cstrike_TitlesTXT_Equipment", "Equipment" },
        { "Cstrike_TitlesTXT_Escaping_Terrorists_Neutralized", "Escaping terrorists have\nall been neutralized!" },
        { "Cstrike_TitlesTXT_FNP90", "ES C90" },
        { "Cstrike_TitlesTXT_Famas", "Clarion 5.56" },
        { "Cstrike_TitlesTXT_Fire_in_the_hole", "Fire in the hole!" },
        { "Cstrike_TitlesTXT_FiveSeven", "ES Five-Seven" },
        { "Cstrike_TitlesTXT_Flashbang", "Flashbang" },
        { "Cstrike_TitlesTXT_Follow_me", "Follow Me." },
        { "Cstrike_TitlesTXT_Friend", "Friend" },
        { "Cstrike_TitlesTXT_G3SG1", "D3/AU-1 Semi-Auto Sniper Rifle" },
        { "Cstrike_TitlesTXT_GAMESAVED", "Saved" },
        { "Cstrike_TitlesTXT_GIGN", "GIGN" },
        { "Cstrike_TitlesTXT_GSG_9", "GSG-9" },
        { "Cstrike_TitlesTXT_Galil", "IDF Defender" },
        { "Cstrike_TitlesTXT_Game_Commencing", "Game Commencing!" },
        { "Cstrike_TitlesTXT_Game_added_position", "You have been added to position %s1 of 5" },
        { "Cstrike_TitlesTXT_Game_bomb_drop", "%s1 dropped the bomb" },
        { "Cstrike_TitlesTXT_Game_bomb_pickup", "%s1 picked up the bomb" },
        { "Cstrike_TitlesTXT_Game_connected", "%s1 connected" },
        { "Cstrike_TitlesTXT_Game_disconnected", "%s1 has left the game" },
        { "Cstrike_TitlesTXT_Game_idle_kick", "%s1 has been idle for too long and has been kicked" },
        { "Cstrike_TitlesTXT_Game_in_position", "You are already in position %s1 of 5" },
        { "Cstrike_TitlesTXT_Game_join_ct", "%s1 is joining the Counter-Terrorist force" },
        { "Cstrike_TitlesTXT_Game_join_ct_auto", "%s1 is joining the Counter-Terrorist force (auto)" },
        { "Cstrike_TitlesTXT_Game_join_terrorist", "%s1 is joining the Terrorist force" },
        { "Cstrike_TitlesTXT_Game_join_terrorist_auto", "%s1 is joining the Terrorist force (auto)" },
        { "Cstrike_TitlesTXT_Game_kicked", "Kicked %s1" },
        { "Cstrike_TitlesTXT_Game_no_timelimit", "* No Time Limit *" },
        { "Cstrike_TitlesTXT_Game_radio", "%s1 (RADIO): %s2" },
        { "Cstrike_TitlesTXT_Game_required_votes", "Required number of votes for a new map = %s1" },
        { "Cstrike_TitlesTXT_Game_scoring", "Scoring will not start until both teams have players" },
        { "Cstrike_TitlesTXT_Game_teammate_attack", "%s1 attacked a teammate" },
        { "Cstrike_TitlesTXT_Game_teammate_kills", "Teammate kills: %s1 of 3" },
        { "Cstrike_TitlesTXT_Game_timelimit", "Time Remaining:  %s1:%s2" },
        { "Cstrike_TitlesTXT_Game_unknown_command", "Unknown command: %s1" },
        { "Cstrike_TitlesTXT_Game_vote_cast", "Vote cast against player # %s1" },
        { "Cstrike_TitlesTXT_Game_vote_not_yourself", "You can't vote to kick yourself!" },
        { "Cstrike_TitlesTXT_Game_vote_player_not_found", "Player # %s1 was not found" },
        { "Cstrike_TitlesTXT_Game_vote_players_on_your_team", "You can only vote for players on your team" },
        { "Cstrike_TitlesTXT_Game_vote_usage", "Usage:  vote <id>" },
        { "Cstrike_TitlesTXT_Game_voted_for_map", "You voted for Map # %s1" },
        { "Cstrike_TitlesTXT_Game_votemap_usage", "Usage:  votemap <id>" },
        { "Cstrike_TitlesTXT_Game_will_restart_in", "The game will restart in %s1 %s2" },
        { "Cstrike_TitlesTXT_Get_in_position_and_wait", "Get in position and wait for my go." },
        { "Cstrike_TitlesTXT_Get_out_of_there", "Get out of there, it's gonna blow!" },
        { "Cstrike_TitlesTXT_Glock18", "9X19mm Sidearm" },
        { "Cstrike_TitlesTXT_Go_go_go", "Go go go!" },
        { "Cstrike_TitlesTXT_Got_bomb", "You picked up the bomb!" },
        { "Cstrike_TitlesTXT_Got_defuser", "You picked up a defuser kit!" },
        { "Cstrike_TitlesTXT_Guerilla_Warfare", "Guerilla Warfare" },
        { "Cstrike_TitlesTXT_HE_Grenade", "HE Grenade" },
        { "Cstrike_TitlesTXT_Health", "Health" },
        { "Cstrike_TitlesTXT_High_Explosive_Grenade", "High-Explosive Grenade" },
        { "Cstrike_TitlesTXT_Hint_cannot_play_because_tk", "You're not allowed to play this\nround because you TK'd last round." },
        { "Cstrike_TitlesTXT_Hint_careful_around_hostages", "Be careful around hostages.\nYou will lose money if you kill a hostage." },
        { "Cstrike_TitlesTXT_Hint_careful_around_teammates", "Careful!\nKilling teammates will not be tolerated!" },
        { "Cstrike_TitlesTXT_Hint_ct_vip_zone", "You are in a VIP escape zone.\nEscort the VIP to any one of these zones!" },
        { "Cstrike_TitlesTXT_Hint_hostage_rescue_zone", "You are in a hostage rescue zone.\nFind the hostages and bring them here!" },
        { "Cstrike_TitlesTXT_Hint_lead_hostage_to_rescue_point", "Lead the hostage to the rescue point!\nYou may USE the hostage again to\nstop him from following." },
        { "Cstrike_TitlesTXT_Hint_lost_money", "You have lost money for killing a hostage." },
        { "Cstrike_TitlesTXT_Hint_out_of_ammo", "You are out of ammunition.\nReturn to a buy zone to purchase more." },
        { "Cstrike_TitlesTXT_Hint_press_buy_to_purchase", "Press the BUY key to purchase items." },
        { "Cstrike_TitlesTXT_Hint_press_use_so_hostage_will_follow", "Press USE to get the hostage to follow you." },
        { "Cstrike_TitlesTXT_Hint_prevent_hostage_rescue", "Prevent the Counter-Terrorists from\nrescuing the hostages!" },
        { "Cstrike_TitlesTXT_Hint_removed_for_next_hostage_killed", "If you kill one more hostage, \nyou will be removed from the server." },
        { "Cstrike_TitlesTXT_Hint_rescue_the_hostages", "Rescue the hostages for money!" },
        { "Cstrike_TitlesTXT_Hint_reward_for_killing_vip", "You have been rewarded $2500 for killing the VIP!" },
        { "Cstrike_TitlesTXT_Hint_spotted_a_friend", "You have spotted a friend." },
        { "Cstrike_TitlesTXT_Hint_spotted_an_enemy", "You have spotted an enemy." },
        { "Cstrike_TitlesTXT_Hint_terrorist_escape_zone", "You are in a terrorist escape zone.\nPrevent the terrorists from getting here!" },
        { "Cstrike_TitlesTXT_Hint_terrorist_vip_zone", "You are in a VIP escape zone.\nPrevent the VIP from reaching any one of these zones." },
        { "Cstrike_TitlesTXT_Hint_try_not_to_injure_teammates", "Try not to injure your teammates." },
        { "Cstrike_TitlesTXT_Hint_use_hostage_to_stop_him", "You may USE the hostage again to\nstop him from following." },
        { "Cstrike_TitlesTXT_Hint_use_nightvision", "Press the NIGHTVISION key to turn on/off nightvision goggles.\nNightvision can be adjusted by typing:\n+nvgadjust\n-nvgadjust\nat the console." },
        { "Cstrike_TitlesTXT_Hint_win_round_by_killing_enemy", "You killed an Enemy!\nWin the round by eliminating\nthe opposing force." },
        { "Cstrike_TitlesTXT_Hint_you_are_in_targetzone", "You are in the target zone.\nSelect the bomb in your inventory\nand plant it by holding FIRE!" },
        { "Cstrike_TitlesTXT_Hint_you_are_the_vip", "You are the VIP\nMake your way to the safety zones!" },
        { "Cstrike_TitlesTXT_Hint_you_have_the_bomb", "You have the bomb!\nFind the target zone or DROP\nthe bomb for another Terrorist." },
        { "Cstrike_TitlesTXT_Hold_this_position", "Hold This Position." },
        { "Cstrike_TitlesTXT_Hostage", "Hostage" },
        { "Cstrike_TitlesTXT_Hostage_down", "Hostage down." },
        { "Cstrike_TitlesTXT_Hostages_Not_Rescued", "Hostages have not been rescued!" },
        { "Cstrike_TitlesTXT_Ignore_Broadcast_Messages", "Now ignoring BROADCAST messages" },
        { "Cstrike_TitlesTXT_Ignore_Broadcast_Team_Messages", "Now ignoring TEAM/BROADCAST messages" },
        { "Cstrike_TitlesTXT_Ignore_Radio", "Now IGNORING radio messages" },
        { "Cstrike_TitlesTXT_In_position", "I'm in position." },
        { "Cstrike_TitlesTXT_Injured_Hostage", "You injured a hostage!" },
        { "Cstrike_TitlesTXT_KM45Tactical", "K&M .45" },
        { "Cstrike_TitlesTXT_KMUMP45", "K&M UMP45" },
        { "Cstrike_TitlesTXT_Kevlar", "Kevlar" },
        { "Cstrike_TitlesTXT_Kevlar_Helmet", "Kevlar+Helmet" },
        { "Cstrike_TitlesTXT_Kevlar_Vest", "Kevlar Vest" },
        { "Cstrike_TitlesTXT_Kevlar_Vest_Ballistic_Helmet", "Kevlar Vest + Ballistic Helmet" },
        { "Cstrike_TitlesTXT_Killed_Hostage", "You killed a hostage!" },
        { "Cstrike_TitlesTXT_Killed_Teammate", "You killed a teammate!" },
        { "Cstrike_TitlesTXT_Krieg550", "Krieg 550" },
        { "Cstrike_TitlesTXT_Krieg552", "Krieg 552" },
        { "Cstrike_TitlesTXT_L337_Krew", "Elite Crew" },
        { "Cstrike_TitlesTXT_LATENCY", "LATENCY" },
        { "Cstrike_TitlesTXT_Leone12", "12 Gauge" },
        { "Cstrike_TitlesTXT_M249", "ES M249 Para" },
        { "Cstrike_TitlesTXT_M4A1", "Maverick M4A1 Carbine" },
        { "Cstrike_TitlesTXT_M4A1_Short", "M4A1" },
        { "Cstrike_TitlesTXT_Mac10", "Ingram Mac-10" },
        { "Cstrike_TitlesTXT_Mac10_Short", "Mac-10" },
        { "Cstrike_TitlesTXT_MachineGuns", "Machine Guns" },
        { "Cstrike_TitlesTXT_Magnum", "Magnum" },
        { "Cstrike_TitlesTXT_Map_Description_not_available", "Map Description not available." },
        { "Cstrike_TitlesTXT_Map_Vote_Extend", "Map has been extended for 30 minutes" },
        { "Cstrike_TitlesTXT_Map_descr_not_avail", "Map description not available." },
        { "Cstrike_TitlesTXT_Menu_Cancel", "Cancel" },
        { "Cstrike_TitlesTXT_Menu_OK", "OK" },
        { "Cstrike_TitlesTXT_Menu_Spectate", "Spectate" },
        { "Cstrike_TitlesTXT_Mic_Volume", "Mic Volume" },
        { "Cstrike_TitlesTXT_Muted", "You have muted %s1." },
        { "Cstrike_TitlesTXT_Name_change_at_respawn", "Your name will be changed after your next respawn." },
        { "Cstrike_TitlesTXT_Need_backup", "Need backup." },
        { "Cstrike_TitlesTXT_Negative", "Negative." },
        { "Cstrike_TitlesTXT_NightHawk", "Night Hawk" },
        { "Cstrike_TitlesTXT_NightVision", "NightVision" },
        { "Cstrike_TitlesTXT_Nightvision_Goggles", "Nightvision Goggles" },
        { "Cstrike_TitlesTXT_No_longer_hear_that_player", "You will no longer hear that player speak." },
        { "Cstrike_TitlesTXT_Not_Enough_Money", "You have insufficient funds!" },
        { "Cstrike_TitlesTXT_OBS_CHASE_FREE", "Free Chase Cam" },
        { "Cstrike_TitlesTXT_OBS_CHASE_LOCKED", "Locked Chase Cam" },
        { "Cstrike_TitlesTXT_OBS_IN_EYE", "First Person" },
        { "Cstrike_TitlesTXT_OBS_MAP_CHASE", "Chase Overview" },
        { "Cstrike_TitlesTXT_OBS_MAP_FREE", "Free Overview" },
        { "Cstrike_TitlesTXT_OBS_NONE", "Camera Options" },
        { "Cstrike_TitlesTXT_OBS_ROAMING", "Free Look" },
        { "Cstrike_TitlesTXT_Only_1_Team_Change", "Only 1 team change is allowed." },
        { "Cstrike_TitlesTXT_Only_CT_Can_Move_Hostages", "Only Counter-Terrorists can move the hostages!" },
        { "Cstrike_TitlesTXT_P228", "228 Compact" },
        { "Cstrike_TitlesTXT_P228Compact", "228" },
        { "Cstrike_TitlesTXT_PLAYERS", "Players" },
        { "Cstrike_TitlesTXT_Phoenix_Connexion", "Phoenix Connexion" },
        { "Cstrike_TitlesTXT_Pistols", "Pistols" },
        { "Cstrike_TitlesTXT_Player", "player" },
        { "Cstrike_TitlesTXT_Player_plural", "players" },
        { "Cstrike_TitlesTXT_Prim_Ammo", "Prim. Ammo" },
        { "Cstrike_TitlesTXT_Regroup_team", "Regroup Team." },
        { "Cstrike_TitlesTXT_Report_in_team", "Report in, team." },
        { "Cstrike_TitlesTXT_Reporting_in", "Reporting in." },
        { "Cstrike_TitlesTXT_Rifles", "Rifles" },
        { "Cstrike_TitlesTXT_Roger_that", "Roger that." },
        { "Cstrike_TitlesTXT_Round_Draw", "Round Draw!" },
        { "Cstrike_TitlesTXT_SAS", "SAS" },
        { "Cstrike_TitlesTXT_SCORE", "SCORE" },
        { "Cstrike_TitlesTXT_SCORES", "  SCORES" },
        { "Cstrike_TitlesTXT_SG550", "Krieg 550 Commando" },
        { "Cstrike_TitlesTXT_SG552", "Krieg 552 Commando" },
        { "Cstrike_TitlesTXT_SMGs", "SMG's" },
        { "Cstrike_TitlesTXT_SPECT_OPTIONS", "Options" },
        { "Cstrike_TitlesTXT_Schmidt", "Schmidt" },
        { "Cstrike_TitlesTXT_SchmidtMP", "Schmidt MP" },
        { "Cstrike_TitlesTXT_Scout", "Schmidt Scout" },
        { "Cstrike_TitlesTXT_Seal_Team_6", "Seal Team 6" },
        { "Cstrike_TitlesTXT_Sec_Ammo", "Sec. Ammo" },
        { "Cstrike_TitlesTXT_Sector_clear", "Sector clear." },
        { "Cstrike_TitlesTXT_Selection_Not_Available", "Selection Not Available" },
        { "Cstrike_TitlesTXT_Shotguns", "Shotguns" },
        { "Cstrike_TitlesTXT_Sidearm9X19mm", "9X19mm" },
        { "Cstrike_TitlesTXT_Smoke_Grenade", "Smoke Grenade" },
        { "Cstrike_TitlesTXT_Speaker_Volume", "Speaker Volume" },
        { "Cstrike_TitlesTXT_Spec_Auto", "Auto" },
        { "Cstrike_TitlesTXT_Spec_Duck", "Press DUCK for Spectator Menu" },
        { "Cstrike_TitlesTXT_Spec_Help_Text", "Use the following keys to change view styles:\n\n FIRE1 - Chase next player\n FIRE2 - Chase previous player\n JUMP - Change view modes\n USE - Change inset window mode\n \n DUCK  - Enable spectator menu\n  \nIn Overview Map Mode move around with:\n\n MOVELEFT - move left\n MOVERIGHT - move right\n FORWARD - zoom in\n BACK - zoom out\n MOUSE - rotate around map/target" },
        { "Cstrike_TitlesTXT_Spec_Help_Title", "Spectator Mode" },
        { "Cstrike_TitlesTXT_Spec_ListPlayers", "List Players" },
        { "Cstrike_TitlesTXT_Spec_Map", "Map" },
        { "Cstrike_TitlesTXT_Spec_Mode1", "Locked Chase Cam" },
        { "Cstrike_TitlesTXT_Spec_Mode2", "Free Chase Cam" },
        { "Cstrike_TitlesTXT_Spec_Mode3", "Free Look" },
        { "Cstrike_TitlesTXT_Spec_Mode4", "First Person" },
        { "Cstrike_TitlesTXT_Spec_Mode5", "Free Overview" },
        { "Cstrike_TitlesTXT_Spec_Mode6", "Chase Overview" },
        { "Cstrike_TitlesTXT_Spec_NoPlayers", "No Players to Spectate" },
        { "Cstrike_TitlesTXT_Spec_NoTarget", "No valid targets. Cannot switch to Chase-Camera Mode." },
        { "Cstrike_TitlesTXT_Spec_No_PIP", "Picture-In-Picture is not available\nin First-Person mode while playing." },
        { "Cstrike_TitlesTXT_Spec_Not_In_Spectator_Mode", "** You are not in spectator mode." },
        { "Cstrike_TitlesTXT_Spec_Not_Valid_Choice", "** You are not allowed to spectate this person." },
        { "Cstrike_TitlesTXT_Spec_Replay", "Instant Replay" },
        { "Cstrike_TitlesTXT_Spec_Slow_Motion", "Slow Motion" },
        { "Cstrike_TitlesTXT_Spec_Time", "Time" },
        { "Cstrike_TitlesTXT_Spectators", "Spectators" },
        { "Cstrike_TitlesTXT_Stick_together_team", "Stick together, team." },
        { "Cstrike_TitlesTXT_Storm_the_front", "Storm the Front!" },
        { "Cstrike_TitlesTXT_SubMachineGun", "SMG" },
        { "Cstrike_TitlesTXT_Super90", "Leone 12 Gauge Super" },
        { "Cstrike_TitlesTXT_Switch_To_BurstFire", "Switched to Burst-Fire mode" },
        { "Cstrike_TitlesTXT_Switch_To_FullAuto", "Switched to automatic" },
        { "Cstrike_TitlesTXT_Switch_To_SemiAuto", "Switched to semi-automatic" },
        { "Cstrike_TitlesTXT_TEAMS", "Teams" },
        { "Cstrike_TitlesTXT_TRAINING1", "Use Your BUY key to purchase:\n      - Sub Machine Gun\n      - Primary Ammo" },
        { "Cstrike_TitlesTXT_TRAINING2", "Use your BUY key to purchase:\n- Magnum Sniper Rifle" },
        { "Cstrike_TitlesTXT_TRAINING3", "Use your BUY key to purchase:\n     - Smoke Grenade" },
        { "Cstrike_TitlesTXT_TRAINING4", "Collect the C4 from the bench." },
        { "Cstrike_TitlesTXT_TRAINING5", "Place C4 then retreat to saftey." },
        { "Cstrike_TitlesTXT_TRAINING6", "Defuse the bomb, by holding\ndown your USE key." },
        { "Cstrike_TitlesTXT_TRAINING7", "Locate and rescue hostages." },
        { "Cstrike_TitlesTXT_TactShield", "Tactical Shield" },
        { "Cstrike_TitlesTXT_TactShield_Desc", "Tactical Shield" },
        { "Cstrike_TitlesTXT_Taking_fire", "Taking Fire...Need Assistance!" },
        { "Cstrike_TitlesTXT_Target_Bombed", "Target Successfully Bombed!" },
        { "Cstrike_TitlesTXT_Target_Saved", "Target has been saved!" },
        { "Cstrike_TitlesTXT_Team_AutoAssign", "Auto Assign" },
        { "Cstrike_TitlesTXT_Team_fall_back", "Team, fall back!" },
        { "Cstrike_TitlesTXT_Terrorist_Escaped", "A terrorist has escaped!" },
        { "Cstrike_TitlesTXT_Terrorist_Forces", "Terrorist Forces" },
        { "Cstrike_TitlesTXT_Terrorist_cant_buy", "Terrorists aren't allowed to\nbuy anything on this map!" },
        { "Cstrike_TitlesTXT_Terrorists_Escaped", "The terrorists have escaped!" },
        { "Cstrike_TitlesTXT_Terrorists_Full", "The terrorist team is full!" },
        { "Cstrike_TitlesTXT_Terrorists_Not_Escaped", "Terrorists have not escaped!" },
        { "Cstrike_TitlesTXT_Terrorists_Win", "Terrorists Win!" },
        { "Cstrike_TitlesTXT_Title_SelectYourTeam", "Select Your Team" },
        { "Cstrike_TitlesTXT_Title_ct_model_selection", "CT Model Selection" },
        { "Cstrike_TitlesTXT_Title_equipment_selection", "Equipment Selection" },
        { "Cstrike_TitlesTXT_Title_gign", "French GIGN" },
        { "Cstrike_TitlesTXT_Title_gsg9", "German GSG-9" },
        { "Cstrike_TitlesTXT_Title_machinegun_selection", "Machine Gun Selection" },
        { "Cstrike_TitlesTXT_Title_pistol_selection", "Pistol Selection" },
        { "Cstrike_TitlesTXT_Title_rifle_selection", "Rifle Selection" },
        { "Cstrike_TitlesTXT_Title_sas", "UK Special Air Service" },
        { "Cstrike_TitlesTXT_Title_seal_team", "US Seal Team 6 (DEVGRU)" },
        { "Cstrike_TitlesTXT_Title_select_category_of_purchase", "Select Category of Purchase" },
        { "Cstrike_TitlesTXT_Title_shotgun_selection", "Shotgun Selection" },
        { "Cstrike_TitlesTXT_Title_smg_selection", "SMG Selection" },
        { "Cstrike_TitlesTXT_Title_terrorist_model_selection", "Terrorist Model Selection" },
        { "Cstrike_TitlesTXT_Too_Many_CTs", "There are too many CTs!" },
        { "Cstrike_TitlesTXT_Too_Many_Terrorists", "There are too many terrorists!" },
        { "Cstrike_TitlesTXT_UMP45", "K&M UMP45" },
        { "Cstrike_TitlesTXT_USP45", "K&M .45 Tactical" },
        { "Cstrike_TitlesTXT_Unassigned", "Unassigned" },
        { "Cstrike_TitlesTXT_Unmuted", "You have unmuted %s1." },
        { "Cstrike_TitlesTXT_VIP", "VIP" },
        { "Cstrike_TitlesTXT_VIP_Assassinated", "VIP has been assassinated!" },
        { "Cstrike_TitlesTXT_VIP_Escaped", "The VIP has escaped!" },
        { "Cstrike_TitlesTXT_VIP_Not_Escaped", "VIP has not escaped!" },
        { "Cstrike_TitlesTXT_VIP_cant_buy", "You are the VIP.\nYou can't buy anything!" },
        { "Cstrike_TitlesTXT_VOICE", "VOICE" },
        { "Cstrike_TitlesTXT_Voice_Properties", "Voice Properties" },
        { "Cstrike_TitlesTXT_Vote", "%s1 :  %s2 (%s3 vote)" },
        { "Cstrike_TitlesTXT_Votes", "%s1 :  %s2 (%s3 votes)" },
        { "Cstrike_TitlesTXT_WINS", "WINS" },
        { "Cstrike_TitlesTXT_Wait_3_Seconds", "Please wait 3 seconds." },
        { "Cstrike_TitlesTXT_Weapon_Cannot_Be_Dropped", "This weapon cannot be dropped" },
        { "Cstrike_TitlesTXT_Weapon_Not_Available", "This weapon is not available to you!" },
        { "Cstrike_TitlesTXT_XM1014", "Leone YG1265 Auto Shotgun" },
        { "Cstrike_TitlesTXT_You_take_the_point", "You Take the Point." },
        { "Cstrike_TitlesTXT_mp5navy", "K&M Sub-machinegun" },
        { "Cstrike_TitlesTXT_tmp", "Schmidt Machine Pistol" }
    };

    // Keep the small compatibility set that predates the generated family.
    // These aliases cover installs with no usable titles.txt at all, plus two
    // menu labels that do not belong to Cstrike_TitlesTXT_*.
    static const struct { const char* key; const char* value; } aliases[] =
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
        InsertEntry(fallback[i].key, fallback[i].value, false, true);

    for (int i = 0; i < (int)(sizeof(aliases) / sizeof(aliases[0])); ++i)
        InsertEntry(aliases[i].key, aliases[i].value, false, true);
}

void InitializeLocalization()
{
    if (g_initialized || g_loadAttempts >= kMaxLoadAttempts)
        return;

    ++g_loadAttempts;

    int loaded = 0;
    loaded += LoadResource("valve", "english");
    loaded += LoadResource("cstrike", "english");

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
        loaded += LoadResource("valve", language);
        loaded += LoadResource("cstrike", language);
    }

    AddFallbackMessages();

    if (loaded > 0)
    {
        ++g_filesLoaded;
        g_initialized = true;
    }
}

void LocalizeStatus_f()
{
    InitializeLocalization();

    gEngfuncs.Con_Printf("localization: %d strings, %d resource set(s) loaded, "
        "%d load attempt(s)\n", g_entryCount, g_filesLoaded, g_loadAttempts);

    for (int i = 0; i < g_loadLogCount; ++i)
    {
        gEngfuncs.Con_Printf("  %-34s %s bytes=%d entries=%d\n",
            g_loadLog[i].fileName,
            g_loadLog[i].opened ? "read   " : "MISSING",
            g_loadLog[i].bytes, g_loadLog[i].entries);
    }

    if (gEngfuncs.Cmd_Argc() > 1)
    {
        const char* token = gEngfuncs.Cmd_Argv(1);
        gEngfuncs.Con_Printf("  \"%s\" -> \"%s\"\n", token, CS16_Localize(token));
    }
    else
    {
        gEngfuncs.Con_Printf("  usage: cs_localize_status [token] "
            "to resolve a single string\n");
    }
}

void LocalizeReload_f()
{
    g_initialized = false;
    g_loadAttempts = 0;
    g_loadLogCount = 0;
    InitializeLocalization();

    gEngfuncs.Con_Printf("localization reloaded: %d strings, "
        "%d resource set(s)\n", g_entryCount, g_filesLoaded);
}

const char* ResolveToken(const char* token, int depth)
{
    if (!token || !token[0] || depth > 8)
        return token ? token : "";

    InitializeLocalization();

    const char* key = token[0] == '#' ? token + 1 : token;
    int index = FindEntry(key);
    if (index >= 0 && !g_entries[index].fallback)
    {
        const char* value = g_entries[index].value;
        if (value[0] == '#' && value[1] && stricmp(value + 1, key))
            return ResolveToken(value, depth + 1);
        return value;
    }

    // Ask the engine before accepting a fallback. This preserves titles.txt as
    // the authority for placement and message text, including custom installs
    // where Spec_Duck contains text instead of redirecting to a resource token.
    client_textmessage_t* engineMessage = gEngfuncs.pfnTextMessageGet
        ? gEngfuncs.pfnTextMessageGet(key)
        : NULL;
    if (engineMessage && engineMessage->pMessage && engineMessage->pMessage[0])
    {
        if (engineMessage->pMessage[0] == '#' &&
            stricmp(engineMessage->pMessage + 1, key))
            return ResolveToken(engineMessage->pMessage, depth + 1);
        return engineMessage->pMessage;
    }

    if (index >= 0)
        return g_entries[index].value;

    // Valve's resource files keep most titles.txt strings under a
    // "Cstrike_TitlesTXT_" prefix, so "#Cant_buy" lives there as
    // "Cstrike_TitlesTXT_Cant_buy". This is deliberately after the engine
    // lookup because the prefixed English entry may only be a fallback.
    if (strnicmp(key, "Cstrike_TitlesTXT_", 18))
    {
        char prefixed[288];
        snprintf(prefixed, sizeof(prefixed), "Cstrike_TitlesTXT_%s", key);
        index = FindEntry(prefixed);
        if (index >= 0)
            return g_entries[index].value;
    }

    return token;
}
}

void CS16_LocalizeRegisterCommands(void)
{
    if (g_commandsRegistered || !gEngfuncs.pfnAddCommand)
        return;

    g_commandsRegistered = true;
    gEngfuncs.pfnAddCommand("cs_localize_status", LocalizeStatus_f);
    gEngfuncs.pfnAddCommand("cs_localize_reload", LocalizeReload_f);
}

void CS16_LocalizeRetryIfEmpty(void)
{
    if (g_filesLoaded > 0)
        return;

    g_loadAttempts = 0;
    InitializeLocalization();
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
        // Some resource files escape the percent sign of a placeholder
        // ("%%s1"). Without this the escape rule below would emit a literal
        // "%s1" on the screen instead of substituting the argument.
        const char* placeholder = p;
        if (placeholder[0] == '%' && placeholder[1] == '%' &&
            placeholder[2] == 's' &&
            placeholder[3] >= '1' && placeholder[3] <= '9')
            ++placeholder;

        if (placeholder[0] == '%' && placeholder[1] == 's')
        {
            int argument = nextArgument++;
            int consumed = 2;
            if (placeholder[2] >= '1' && placeholder[2] <= '9')
            {
                argument = placeholder[2] - '1';
                consumed = 3;
            }

            const char* replacement =
                argument >= 0 && argument < argumentCount && arguments[argument]
                ? arguments[argument]
                : "";
            while (*replacement && written + 1 < dstSize)
                dst[written++] = *replacement++;

            p = placeholder + consumed;
            continue;
        }

        if (p[0] == '%' && p[1] == '%')
        {
            dst[written++] = '%';
            p += 2;
            continue;
        }

        dst[written++] = *p++;
    }

    dst[written] = '\0';
    return written;
}

bool CS16_HasFormatPlaceholder(const char* text)
{
    for (const char* p = text ? text : ""; *p; ++p)
    {
        const char* placeholder = p;
        if (placeholder[0] == '%' && placeholder[1] == '%' &&
            placeholder[2] == 's' &&
            placeholder[3] >= '1' && placeholder[3] <= '9')
            ++placeholder;

        if (placeholder[0] == '%' && placeholder[1] == 's')
            return true;
    }

    return false;
}

void CS16_StripFormatPlaceholders(char* text)
{
    if (!text)
        return;

    char* dst = text;
    for (const char* src = text; *src;)
    {
        const char* placeholder = src;
        if (placeholder[0] == '%' && placeholder[1] == '%' &&
            placeholder[2] == 's' &&
            placeholder[3] >= '1' && placeholder[3] <= '9')
            ++placeholder;

        if (placeholder[0] == '%' && placeholder[1] == 's')
        {
            src = placeholder + 2;
            if (*src >= '1' && *src <= '9')
                ++src;
            continue;
        }

        *dst++ = *src++;
    }

    *dst = '\0';
}
