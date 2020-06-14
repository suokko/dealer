#pragma once

#include <type_traits>
#include <limits>
#include <algorithm>

#include "entry.h"

template<typename T>
entry<T (*)(T)> popcounti;

template<typename T>
static inline T popcount(T v)
{
#if __GNUC__
#if __WORDSIZE == 32
	using native_t = uint32_t;
#else
	using native_t = uint64_t;
#endif
#else
	// Native ALU width might not match pointer width on some platforms (like
	// Linux x32)
	using native_t = unsigned intptr_t;
#endif
	using uT = typename std::make_unsigned<T>::type;
	uT uv = v;
	if (sizeof(v) > sizeof(native_t)) {
		native_t rv = 0;
		unsigned count = sizeof(v)/sizeof(native_t);
		const unsigned shift = std::min(std::numeric_limits<native_t>::digits,
				std::numeric_limits<uT>::digits - 1);
		do {
			rv += popcount(static_cast<native_t>(uv));
			uv >>= shift;
		} while(--count);
		return rv;
	}
#if !__POPCNT__
	return popcounti<uT>(uv);
#endif
#if __GNUC__
	if (sizeof(v) == sizeof(long long))
		return __builtin_popcountll(uv);
	if (sizeof(v) == sizeof(long))
		return __builtin_popcountl(uv);
	if (sizeof(v) <= sizeof(int))
		return __builtin_popcount(uv);
#else
#warning "No popcnt instruction support for your compiler"
	return popcounti<uT>(uv);
#endif
}
