#define __STDC_FORMAT_MACROS
#include "target_int.h"
#include <iostream>
#include <cassert>
#include <type_traits>
#include <limits>
#include <cmath>

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

namespace std {

template<>
struct make_unsigned<int128_t>
{ typedef uint128_t type;};

template<>
struct make_unsigned<uint128_t>
{ typedef uint128_t type;};
}
#endif

#if __WORDSIZE == 32 && !defined(__clang__)
#define __builtin_popcountll(v) (__builtin_popcountl(v) + __builtin_popcountl(v >> 32))
#endif

#include <cxxabi.h>

template <class T>
static T popcount(T in)
{
#if __POPCNT__
	/* If there is popcnt instruction that is faster.
	 * But gcc provides typed builtin so we have to select correct one
	 */
	if (sizeof(in) <= sizeof(int))
		return __builtin_popcount(in);
	if (sizeof(in) <= sizeof(long))
		return __builtin_popcountl(in);
	if (sizeof(in) <= sizeof(long long))
		return __builtin_popcountll(in);
#if __x86_64__
	constexpr auto llbits = std::numeric_limits<long long>::digits;
	if (sizeof(in) == 2*sizeof(long long)) {
		return __builtin_popcountll(static_cast<long long>(in)) +
			__builtin_popcountll(static_cast<long long>(in >> llbits));
	}
#endif
#endif
	using uT = typename std::make_unsigned<T>::type;
	uT x = in;
	unsigned depth = 1;
	unsigned muldepth = MVslowmul ? std::numeric_limits<uT>::digits : 8;
	auto mask = x;
	auto mask01 = x;
	/* count every second bit
	 * 00 - 00 -> 00
	 * 01 - 00 -> 01
	 * 10 - 01 -> 01
	 * 11 - 01 -> 10
	 * Minus is just optimisation to avoid second mask operation because
	 * of sum would overflow.
	 */
	mask = (~(uT)0)/((1ULL << depth)+1);
	x = x - ((x >> 1) & mask);
	depth *= 2;
	/* Sum the counted bits. First step may overflow so has to apply mask before overflow */
	/* mask = 0xFFFF... / (2**(depth*2)-1)*(2**depth-1)
	 * mask = 0xFFFF... / ((2**(depth*2)-1)/(2**depth-1))
	 * mask = 0xFFFF... / (2**depth+1)
	 */
	mask = (~(uT)0)/((1ULL << depth)+1);
	x = (x & mask) + ((x >> depth) & mask);
	/* Later steps don't apply mask after sum because no overflow */
	for (depth = 4; depth < muldepth; depth *= 2) {
		if (depth < sizeof(long long)*8)
			mask = (~(uT)0)/((1ULL << depth)+1);
		else
			mask = (uT)~0ULL;
		x = ((x + (x >> depth)) & mask);
	}
	/* Shortcut using multiplication to sum each byte. But works
	 * only if there is fast multiplication and results is less
	 * than 256. For integers between 256 and 64k-1 bits one can
	 * run one more shit mask counting step and then do 16bit
	 * multiplication summing.
	 */
	if (depth < std::numeric_limits<uT>::digits) {
		/* x * 0x0101... buts the sum to the highest bit
		 * then simple shift moves it back down
		 */
		mask01 = (~(uT)0)/((1ULL << muldepth) - 1);
		return (uT)(x * mask01) >> (std::numeric_limits<uT>::digits - 8);
	}

	return x;
}

#include <time.h>

template<typename T>
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
	for (unsigned i = 0; i < 5'000'000u; i++)
		first = popcount(first) + first;
	clock_t end = clock();
	std::cout << fn
		<< ": " << type << "(" << std::numeric_limits<T>::digits << ")"
		<< " took " << (end - start)
		<< " -> " << (first + 0)
		<< '\n';
	return first;
}

void DEFUN(test)(unsigned x)
{
	run_test(__func__, "uc", static_cast<unsigned char>(x));
	run_test(__func__, "us", static_cast<unsigned short>(x));
	run_test(__func__, "ui", static_cast<unsigned int>(x));
	run_test(__func__, "ul", static_cast<unsigned long>(x));
	run_test(__func__, "ull", static_cast<unsigned long long>(x));
#if __x86_64__
	run_test(__func__, "u128", static_cast<uint128_t>(x));
#endif
}
