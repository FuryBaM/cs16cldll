// Lightweight Counter-Strike resource localization for ordinary GoldSrc.
#ifndef CS16_LOCALIZE_H
#define CS16_LOCALIZE_H

#include <stddef.h>

const char* CS16_Localize(const char* token);
size_t CS16_LocalizeFormat(char* dst, size_t dstSize, const char* format,
    const char* const* arguments, int argumentCount);

#endif
