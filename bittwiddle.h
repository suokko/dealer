#pragma once

/**
 * GCC popcount fallback is horrible slow. That makes it better to use
 * hand coded fallback that produces clearly faster code.
 */

#if __WORDSIZE == 32
#define __builtin_popcountll(v) (__builtin_popcountl(v) + __builtin_popcountl(v >> 32))
#endif

static inline int popcountll(unsigned long long v)
{
#ifdef __POPCNT__
	return __builtin_popcountll(v);
#else
	unsigned long long mask;
	unsigned depth = 1;

	/* Sum every bit.
	 * Cheat by subtract to avoid a mask operation.
	 */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(typeof(v))0)/255;
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
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(typeof(v))0)/255;
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
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = v - ((v >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v & mask) + ((v >> depth) & mask);
	depth *= 2;

	/* Sum every four bits. 
	 * No overflow allows masking after sum.
	 */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;
	depth *= 2;

	/* Sum every eith bits. */
	mask = (~(typeof(v))0)/((1ULL << depth)+1);
	v = (v  + (v >> depth)) & mask;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~(typeof(v))0)/255;
	return (v * mask) >> (sizeof(v)*8 - 8);
#endif
}
