#pragma once


#if defined(__GNUC__) && !defined(__clang__)
/**
 * GCC popcount fallback is horrible slow. That makes it better to use
 * hand coded fallback that produces clearly faster code.
 */

#if __WORDSIZE == 32
#define __builtin_popcountll(v) (__builtin_popcountl(v) + __builtin_popcountl(v >> 32))
#endif

#define lltype unsigned long long
#define ltype unsigned long
#define utype unsigned

static inline int popcountll(lltype v)
{
#ifdef __POPCNT__
	return __builtin_popcountll(v);
#else
	unsigned long long mask;
	unsigned depth = 1;

	/* Sum every bit.
	 * Cheat by subtract to avoid a mask operation.
	 */
	mask = (~(lltype)0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(lltype)0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(lltype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(lltype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(lltype)0)/255;
	return (v * mask) >> (sizeof(v)*8 - 8);
#endif
}

static inline int popcountl(unsigned long v)
{
#ifdef __POPCNT__
	return __builtin_popcountl(v);
#else
	unsigned long mask;
	unsigned depth = 1;

	/* Sum every bit.
	 * Cheat by subtract to avoid a mask operation.
	 */
	mask = (~(ltype)0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(ltype)0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(ltype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(ltype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(ltype)0)/255;
	return (v * mask) >> (sizeof(v)*8 - 8);
#endif
}

static inline int popcount(unsigned int v)
{
#ifdef __POPCNT__
	return __builtin_popcount(v);
#else
	unsigned int mask;
	unsigned depth = 1;

	/* Sum every bit.
	 * Cheat by subtract to avoid a mask operation.
	 */
	mask = (~(utype)0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(utype)0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(utype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(utype)0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(utype)0)/255;
	return (v * mask) >> (sizeof(v)*8 - 8);
#endif
}

#else

#define popcount __builtin_popcount
#define popcountl __builtin_popcountl
#define popcountll __builtin_popcountll

#endif
