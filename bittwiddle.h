#pragma once

#include <type_traits>
#include <limits>
#include <algorithm>

#include "entry.h"

/// A fallback implementation with runtime detected optimization
template<typename T>
entry<T (*)(T)> popcounti;

/**
 * Return count of set bits.
 *
 * @param v Integer which set bits are counted
 * @return The number of set bits
 */
template<typename T>
static inline T popcount(T v)
{
	// Native ALU width might be different than pointer width (e.g. x32)
#if __GNUC__
#if __WORDSIZE == 32
	using native_t = uint32_t;
#else
	using native_t = uint64_t;
#endif
#else
	using native_t = unsigned intptr_t;
#endif

	// Helper type to convert argument to same width unsigned type
	using uT = typename std::make_unsigned<T>::type;
	// Convert to unsigned without changes to bit representation.
	uT uv = v;
	// Optimize case when counting bits in a value which is wider than native
	// register width. This is used on 32bit system when counting cards.
	if (sizeof(v) > sizeof(native_t)) {
		native_t rv = 0;
		unsigned count = sizeof(v)/sizeof(native_t);
		// Calculate shift width while avoiding warning for undefined behavior
		// when shifting more than the width of type.
		const unsigned shift = std::min(std::numeric_limits<native_t>::digits,
				std::numeric_limits<uT>::digits - 1);
		do {
			rv += popcount(static_cast<native_t>(uv));
			uv >>= shift;
		} while(--count);
		return rv;
	}
#if !__POPCNT__
	// Without instruction support call fallback function
	return popcounti<uT>(uv);
#endif
#if __GNUC__
	// Call type specific counting builtin to generate correct instructions
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

/**
 * Count trailing zero bits.
 *
 * @param v Integer which trailing zero bits are counted
 * @return The number of trailing zero bits
 */
template<typename T>
static inline T ctz(T v)
{
	using uT = typename std::make_unsigned<T>::type;
#if __GNUC__
	assert(v != 0 && "ctz builtin requires non-zero value. "
			"Instruction could be made easily work with zero but builtin doesn't support it.");
	if (sizeof(v) == sizeof(long long))
		return __builtin_ctzll(v);
	if (sizeof(v) == sizeof(long))
		return __builtin_ctzl(v);
	if (sizeof(v) <= sizeof(int))
		return __builtin_ctz(v);
#else
#warning "No count trailing zeros instruction support for your compiler"
#endif
	uT uv = v;
	unsigned depth = std::numeric_limits<uT>::digits/2;
	unsigned position = depth;
	// Binary search for zero bits
	for (; depth > 1;) {
		depth /= 2;
		uT mask = (uT{1} << position) - 1;
		if (uv & mask)
			position -= depth;
		else
			position += depth;
	}
	uT mask = (uT{1} << position) - 1;
	if (uv & mask)
		position--;
	return position;
}

/**
 * Count leading zero bits
 *
 * @Param v Integer which leading zero bits are counted
 * @return The number of leading zero bits
 */
template<typename T>
static inline T clz(T v)
{
	using uT = typename std::make_unsigned<T>::type;
#if __GNUC__
	assert(v != 0 && "ctz builtin requires non-zero value. "
			"Instruction could be made easily work with zero but builtin doesn't support it.");
	if (sizeof(v) == sizeof(long long))
		return __builtin_clzll(v);
	if (sizeof(v) == sizeof(long))
		return __builtin_clzl(v);
	if (sizeof(v) <= sizeof(int))
		return __builtin_clz(v) -
			(std::numeric_limits<unsigned>::digits - std::numeric_limits<uT>::digits);
#else
#warning "No count leading zeros instruction support for your compiler"
#endif
	uT uv = v;
	unsigned depth = std::numeric_limits<uT>::digits/2;
	unsigned position = depth;
	// Binary search for zero bits
	for (; depth > 1;) {
		depth /= 2;
		uT mask = (uT{1} << position) - 1;
		if (uv & ~mask)
			position += depth;
		else
			position -= depth;
	}
	uT mask = (uT{1} << position) - 1;
	if (uv & ~mask)
		position++;
	return std::numeric_limits<uT>::digits - position;
}
