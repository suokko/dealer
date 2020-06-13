#define __STDC_FORMAT_MACROS
#include "target_int.h"
#include <stdio.h>
#include <assert.h>
#include <type_traits>
#include <limits.h>
#include <inttypes.h>

#ifndef MVslowmul
#define MVslowmul 0
#endif

namespace std {

#ifdef __x86_64
template<>
struct make_unsigned<__int128>
{ typedef unsigned __int128 type;};

template<>
struct make_unsigned<unsigned __int128>
{ typedef unsigned __int128 type;};
#endif
}

#if __WORDSIZE == 32 && !defined(__clang__)
#define __builtin_popcountll(v) (__builtin_popcountl(v) + __builtin_popcountl(v >> 32))
#endif

template <class T>
static int popcount(T in)
{
#ifdef __POPCNT__
	/* If there is popcnt instruction that is faster.
	 * But gc provides typed builtin so we have to select correct one
	 */
	if (sizeof(in) <= sizeof(int))
		return __builtin_popcount(in);
	if (sizeof(in) <= sizeof(long))
		return __builtin_popcountl(in);
	if (sizeof(in) <= sizeof(long long))
		return __builtin_popcountll(in);
#ifdef __x86_64
	if (sizeof(in) == 2*sizeof(long long))
		return __builtin_popcountll(in) +
			__builtin_popcountll(static_cast<unsigned __int128>(in) >> (8*sizeof(long long)));
#endif
#endif
	typedef std::make_unsigned<T> uT;
	typename uT::type x = in;
	unsigned depth = 1;
	assert(sizeof(x)*8 < ULLONG_MAX);
	unsigned muldepth = 8;
	auto mask = x;
	auto mask01 = x;
	while (1ULL << muldepth < sizeof(x))
		muldepth <<= 1;
	/* count every second bit
	 * 00 - 00 -> 00
	 * 01 - 00 -> 01
	 * 10 - 01 -> 01
	 * 11 - 01 -> 10
	 * Minus is just optimisation to avoid second mask operation because
	 * of sum would overflow.
	 */
	mask = (~(typename uT::type)0)/((1ULL << depth)+1);
	x = x - ((x >> 1) & mask);
	depth *= 2;
	/* Sum the counted bits. First step may overflow so has to apply mask before overflow */
	/* mask = 0xFFFF... / (2**(depth*2)-1)*(2**depth-1)
	 * mask = 0xFFFF... / ((2**(depth*2)-1)/(2**depth-1))
	 * mask = 0xFFFF... / (2**depth+1)
	 */
	mask = (~(typename uT::type)0)/((1ULL << depth)+1);
	x = (x & mask) + ((x >> depth) & mask);
	/* Later steps an apply mask after sum because no overflow */
	/* I had here a lot simpler while loop but it was too complex
	 * to unroll for both compilers. This more complex forloop
	 * gets unrolled by clang 3.3 but not by gcc 4.8.
	 * I hate compilers!
	 */
	unsigned max = sizeof(x)*8;
	if (!SLOWMUL)
		max = muldepth;
	for (depth = 4; depth < max; depth *= 2) {
		if (depth < sizeof(long long)*8)
			mask = (~(typename uT::type)0)/((1ULL << depth)+1);
		else
			mask = (typename uT::type)~0ULL;
		x = ((x + (x >> depth)) & mask);
	}
	/* Shortcut using multiplication to sum each byte. But works
	 * only if there is fast multiplication and results is less
	 * than 256. For integers between 256 and 64k-1 bits one can
	 * run one more shit mask counting step and then do 16bit
	 * multiplication summing.
	 */
	if (!SLOWMUL && sizeof(x) > 1) {
		/* x * 0x0101... buts the sum to the highest bit
		 * then simple shift moves it back down
		 */
		mask01 = (~(typename uT::type)0)/((1ULL << muldepth) - 1);
		return (x * mask01) >> (sizeof(x)*8 - 8);
	}

	return x;
}

#include <time.h>

void DEFUN(test)(int x)
{
	unsigned int a = x;
	int i;
	clock_t start, end;
	unsigned char g = x;
	start = clock();
	for (i = 0; i < 50*1000*1000; i++) {
		g = i + popcount(g);
	}
	end = clock();
	printf("%s: char took %ld -> %u\n", __func__, end - start, (unsigned)g);
	start = clock();
	for (i = 0; i < 50*1000*1000; i++) {
		a = i + popcount(a);
	}
	end = clock();
	printf("%s: int  took %ld -> %u\n", __func__, end - start, a);
	unsigned long b = x;
	start = clock();
	for (i = 0; i < 50*1000*1000; i++) {
		b = i + popcount(b);
	}
	end = clock();
	printf("%s: long took %ld -> %lu\n", __func__, end - start, b);
	unsigned long long d = x;
	start = clock();
	for (i = 0; i < 50*1000*1000; i++) {
		d = i + popcount(d);
	}
	end = clock();
	printf("%s: ull  took %ld -> %llu\n", __func__, end - start, d);
#ifdef __x86_64
	unsigned __int128  c = a + 1;
	c <<= 64;
	c |= x*2 + 1;
	start = clock();
	for (i = 0; i < 50*1000*1000; i++) {
		c = i + popcount(c);
	}
	end = clock();
	unsigned long long high = c >> 64;
	unsigned long long low = c;
	printf("%s: 128  took %ld -> %llu %llu\n", __func__, end - start, high, low);
#endif
}
