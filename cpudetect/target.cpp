#define __STDC_FORMAT_MACROS
#include "target_int.h"
#include <iostream>
#include <cassert>
#include <type_traits>
#include <limits>
#include <cmath>
#include <time.h>

#ifndef MVslowmul
#define MVslowmul 0
#endif

#ifdef __x86_64__
#if __GNUC__
using int128_t = __int128;
using uint128_t = unsigned __int128;
#else
#error "Missing support for 128bit integer for your compiler."
#endif

static std::ostream& operator<<(std::ostream& os, uint128_t v)
{
	if (!(os.flags() & os.dec))
		return os << static_cast<uint64_t>(v >> 64) << static_cast<uint64_t>(v);

	constexpr uint64_t max = std::pow(10ULL, std::numeric_limits<uint64_t>::digits10);
	if (v > max) {
		auto high = v / max;
		v = v % max;
		os << high;
	}
	return os << static_cast<uint64_t>(v);
}

#endif

#if __WORDSIZE == 32 && !defined(__clang__)
#define __builtin_popcountll(v) (__builtin_popcountl(v) + __builtin_popcountl(v >> 32))
#endif

#include <cxxabi.h>
#if MVslowmul
template <class T>
[[gnu::optimize("unroll-loops")]]
static inline T popcount(T in)
{
#if __WORDSIZE == 32
	using native_t = unsigned int;
#else
	using native_t = unsigned long long;
#endif
	using uT = typename std::make_unsigned<T>::type;
	uT uv = in;
	if (sizeof(uv) > sizeof(native_t)) {
		native_t rv = 0;
		unsigned count = sizeof(uv)/sizeof(native_t);
		const unsigned shift = std::min(std::numeric_limits<native_t>::digits,
				std::numeric_limits<uT>::digits - 1);
		do {
			rv += popcount(static_cast<native_t>(uv));
			uv >>= shift;
		} while(--count);
		return rv;
	}

	uT x = uv;
	unsigned depth = 1;
	unsigned muldepth = std::numeric_limits<uT>::digits;
	auto mask = x;
	/* count every second bit
	 * 00 - 00 -> 00
	 * 01 - 00 -> 01
	 * 10 - 01 -> 01
	 * 11 - 01 -> 10
	 * Minus is just optimisation to avoid second mask operation because
	 * of sum would overflow.
	 */
	mask = (~(uT)0)/((uT{1} << depth)+1u);
	x = x - ((x >> 1) & mask);
	depth *= 2;
	/* Sum the counted bits. First step may overflow so has to apply mask before overflow */
	/* mask = 0xFFFF... / (2**(depth*2)-1)*(2**depth-1)
	 * mask = 0xFFFF... / ((2**(depth*2)-1)/(2**depth-1))
	 * mask = 0xFFFF... / (2**depth+1)
	 */
	mask = (~(uT)0)/((uT{1} << depth)+1u);
	x = (x & mask) + ((x >> depth) & mask);
	/* Later steps don't apply mask after sum because no overflow */
	for (depth = 4; depth < muldepth; depth *= 2) {
		mask = (~(uT)0)/((uT{1} << depth)+1u);
		x = ((x + (x >> depth)) & mask);
	}
	return x;
}
#else
#include "../bittwiddle.h"
#endif

namespace DEFUN() {
template <class T>
[[gnu::optimize("unroll-loops"), gnu::noinline]]
static T popcountt(T in)
{
	return popcount(in);
}

template<typename T>
[[gnu::noinline]]
static T run_test(const char *fn, const char *type, T first)
{
	// duplicate bits
	T bits = std::numeric_limits<unsigned char>::digits;
	T next = first;
	do {
		first = next;
		next <<= bits;
		next |= first;
		bits *= 2;
	} while (first != next);
	clock_t start = clock();
	for (unsigned i = 0; i < 20'000'000u; i++)
		first = popcountt(first) + first;
	clock_t end = clock();
	std::cout << fn
		<< ": " << type << "(" << std::numeric_limits<T>::digits << ")"
		<< " took " << (end - start)
		<< " -> " << (typename std::make_unsigned<decltype(first+0)>::type)(first + 0)
		<< '\n';
	return first;
}

void test(unsigned x)
{
	run_test(__PRETTY_FUNCTION__, "uc", static_cast<unsigned char>(x));
	run_test(__PRETTY_FUNCTION__, "c", static_cast<signed char>(x));
	run_test(__PRETTY_FUNCTION__, "us", static_cast<unsigned short>(x));
	run_test(__PRETTY_FUNCTION__, "s", static_cast<short>(x));
	run_test(__PRETTY_FUNCTION__, "ui", static_cast<unsigned int>(x));
	run_test(__PRETTY_FUNCTION__, "i", static_cast<int>(x));
	run_test(__PRETTY_FUNCTION__, "ul", static_cast<unsigned long>(x));
	run_test(__PRETTY_FUNCTION__, "l", static_cast<long>(x));
	run_test(__PRETTY_FUNCTION__, "ull", static_cast<unsigned long long>(x));
	run_test(__PRETTY_FUNCTION__, "ll", static_cast<long long>(x));
#if __x86_64__
	run_test(__PRETTY_FUNCTION__, "u128", static_cast<uint128_t>(x));
	run_test(__PRETTY_FUNCTION__, "128", static_cast<int128_t>(x));
#endif
}
}
