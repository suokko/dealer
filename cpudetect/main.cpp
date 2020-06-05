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
#ifdef __BMI2__
	cur += sprintf(cur, "%sbmi2", sep);
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
#if defined(__i386__) || defined(__x86_64__)
	if (cpu_supports(CPUSSE)) {
		cur += sprintf(cur, "%ssse", sep);
		sep = ", ";
	}
	if (cpu_supports(CPUSSE2)) {
		cur += sprintf(cur, "%ssse2", sep);
		sep = ", ";
		sse2_test(55);
	}
	if (cpu_supports(CPUSSE3)) {
		cur += sprintf(cur, "%ssse3", sep);
		sep = ", ";
	}
	if (cpu_supports(CPUPOPCNT)) {
		cur += sprintf(cur, "%spopcnt", sep);
		sep = ", ";
		popcnt_test(55);
	}
	if (cpu_supports(CPUSSE41)) {
		cur += sprintf(cur, "%ssse4.1", sep);
		sep = ", ";
	}
	if (cpu_supports(CPUSSE42)) {
		cur += sprintf(cur, "%ssse4.2", sep);
		sep = ", ";
	}
	if (cpu_supports(CPUAVX)) {
		cur += sprintf(cur, "%savx", sep);
		sep = ", ";
	}
	if (cpu_supports(CPUBMI2)) {
		cur += sprintf(cur, "%sbmi2", sep);
		sep = ", ";
	}
/*	if (cpu_supports("xop")) {
		cur += sprintf(cur, "%sxop", sep);
		sep = ", ";
	}*/
	if (cpu_supports(CPUAVX2)) {
		cur += sprintf(cur, "%savx2", sep);
		sep = ", ";
	}
#endif

	printf("CPU supports: %s\n", buffer);
	default_test(55);
	slowmul_test(55);

	return 0;
}
