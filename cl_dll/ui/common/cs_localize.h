// Lightweight Counter-Strike resource localization for ordinary GoldSrc.
#ifndef CS16_LOCALIZE_H
#define CS16_LOCALIZE_H

#include <stddef.h>

const char* CS16_Localize(const char* token);
size_t CS16_LocalizeFormat(char* dst, size_t dstSize, const char* format,
    const char* const* arguments, int argumentCount);

// Registers cs_localize_status and cs_localize_reload. Safe to call twice.
void CS16_LocalizeRegisterCommands(void);

// Retries loading resource/*.txt when no file has been read yet. Call it on map
// changes: the very first lookup can happen before the game directory is
// mounted, and without a retry the client would run on fallbacks all session.
void CS16_LocalizeRetryIfEmpty(void);

// Drops any %s / %s1..%s9 placeholder that survived formatting, so a message
// with a missing argument reads like the stock client instead of showing the
// raw placeholder. Edits the string in place.
void CS16_StripFormatPlaceholders(char* text);

// True when the text still carries a %s / %s1..%s9 placeholder.
bool CS16_HasFormatPlaceholder(const char* text);

#endif
