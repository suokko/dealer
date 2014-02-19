#include "target.h"
#include <stdio.h>
#include "detect.h"

int main(void)
{
	char buffer[1024];
	char *cur = buffer;
	const char *sep = "";
#ifdef __MMX__
	cur += sprintf(cur, "%smmx", sep);
	sep = ", ";
#endif
#ifdef __SSE__
	cur += sprintf(cur, "%ssse", sep);
	sep = ", ";
#endif
#ifdef __SSE2__
	cur += sprintf(cur, "%ssse2", sep);
	sep = ", ";
#endif
#ifdef __SSE3__
	cur += sprintf(cur, "%ssse3", sep);
	sep = ", ";
#endif
#ifdef __SSE4__
	cur += sprintf(cur, "%ssse4", sep);
	sep = ", ";
#endif
#ifdef __POPCNT__
	cur += sprintf(cur, "%spopcnt", sep);
	sep = ", ";
#endif
#ifdef __SSE4a__
	cur += sprintf(cur, "%ssse4a", sep);
	sep = ", ";
#endif
#ifdef __SSE4_1__
	cur += sprintf(cur, "%ssse4.1", sep);
	sep = ", ";
#endif
#ifdef __SSE4_2__
	cur += sprintf(cur, "%ssse4.2", sep);
	sep = ", ";
#endif
#ifdef __AVX__
	cur += sprintf(cur, "%savx", sep);
	sep = ", ";
#endif
#ifdef __XOP__
	cur += sprintf(cur, "%sxop", sep);
	sep = ", ";
#endif
#ifdef __AVX2__
	cur += sprintf(cur, "%savx2", sep);
	sep = ", ";
#endif
	printf("Compiler supports: %s\n", buffer);
	cur = buffer;
	buffer[0] = '\0';
	sep = "";
	cpu_init();
#if defined(__i386__) || defined(__x86_64__)
	if (cpu_supports("cmov")) {
		cur += sprintf(cur, "%scmov", sep);
		sep = ", ";
	}
	if (cpu_supports("mmx")) {
		cur += sprintf(cur, "%smmx", sep);
		sep = ", ";
	}
	if (cpu_supports("sse")) {
		cur += sprintf(cur, "%ssse", sep);
		sep = ", ";
	}
	if (cpu_supports("sse2")) {
		cur += sprintf(cur, "%ssse2", sep);
		sep = ", ";
		sse2_test(55);
	}
	if (cpu_supports("sse3")) {
		cur += sprintf(cur, "%ssse3", sep);
		sep = ", ";
		sse3_test(55);
	}
	if (cpu_supports("popcnt")) {
		cur += sprintf(cur, "%spopcnt", sep);
		sep = ", ";
		popcnt_test(55);
	}
	if (cpu_supports("sse4.1")) {
		cur += sprintf(cur, "%ssse4.1", sep);
		sep = ", ";
	}
	if (cpu_supports("sse4.2")) {
		cur += sprintf(cur, "%ssse4.2", sep);
		sep = ", ";
		sse4_test(55);
	}
	if (cpu_supports("avx")) {
		cur += sprintf(cur, "%savx", sep);
		sep = ", ";
		avx_test(55);
	}
/*	if (cpu_supports("xop")) {
		cur += sprintf(cur, "%sxop", sep);
		sep = ", ";
	}*/
	if (cpu_supports("avx2")) {
		cur += sprintf(cur, "%savx2", sep);
		sep = ", ";
		avx2_test(55);
	}
#endif

	printf("CPU supports: %s\n", buffer);
	default_test(55);
	slowmul_test(55);

	return 0;
}
