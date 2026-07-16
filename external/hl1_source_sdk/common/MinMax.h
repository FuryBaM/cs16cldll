#ifndef COMMON_MINMAX_H
#define COMMON_MINMAX_H

#undef min
#undef max
#undef clamp

#include <algorithm>

#if defined(min) || defined(max)
#error "Do not define min or max; use std::min and std::max instead."
#endif

using std::min;
using std::max;

template <typename T>
T clamp(const T& value, const T& minimum, const T& maximum)
{
    return value > maximum ? maximum : (value < minimum ? minimum : value);
}

#endif
