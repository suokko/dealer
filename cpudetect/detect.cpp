#include "detect.h"
#include <set>
#include <string>
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

typedef std::set<std::string> feature_set;

unsigned features;

static void x86_cpu_init()
{
#if defined(__i386__) || defined(__x86_64__)
	/* Fetch cpu features */
	unsigned eax = 0, ebx = 0, ecx = 0, edx = 0, op = 1;
	__get_cpuid(op, &eax, &ebx, &ecx, &edx);
	/**
	 * basic info
	 */
	union basicinfo {
		uint32_t reg[4];
		struct {
			/* eax */
			uint32_t stepping : 4;
			uint32_t model : 4;
			uint32_t family : 4;
			uint32_t type : 2;
			uint32_t reserved1 : 2;
			uint32_t extModel : 4;
			uint32_t extFamily : 8;
			uint32_t reserved2 : 4;
			/* edx */
			uint32_t fpu : 1;
			uint32_t vme : 1;
			uint32_t de : 1;
			uint32_t pse : 1;
			/* 4 */
			uint32_t tsc : 1;
			uint32_t msr : 1;
			uint32_t pae : 1;
			uint32_t mce : 1;
		        /* 8 */
			uint32_t cx8 : 1;
			uint32_t apic : 1;
			uint32_t reserved3 : 1;
			uint32_t sep : 1;
			/* 12 */
			uint32_t mtrr : 1;
			uint32_t pge : 1;
			uint32_t mca : 1;
			uint32_t cmov : 1;
			/* 16 */
			uint32_t pat : 1;
			uint32_t pse36 : 1;
			uint32_t psn : 1;
			uint32_t clfsh : 1;
			/* 20 */
			uint32_t reserved4 : 1;
			uint32_t ds : 1;
			uint32_t acpi : 1;
			uint32_t mmx : 1;
			/* 24 */
			uint32_t fxsr : 1;
			uint32_t sse : 1;
			uint32_t sse2 : 1;
			uint32_t ss : 1;
			/* 28 */
			uint32_t htt : 1;
			uint32_t tm : 1;
			uint32_t ia64 : 1;
			uint32_t pbe : 1;
			/* ecx */
			uint32_t sse3 : 1;
			uint32_t pclmulqdq : 1;
			uint32_t dtest64 : 1;
			uint32_t monitor : 1;
		        /* 4 */
			uint32_t dscpl : 1;
			uint32_t vmx : 1;
			uint32_t smx : 1;
			uint32_t est : 1;
		        /* 8 */
			uint32_t tm2 : 1;
			uint32_t ssse3 : 1;
			uint32_t cnxtid : 1;
			uint32_t reserved5 : 1;
		        /* 12 */
			uint32_t fma3 : 1;
			uint32_t cx16 : 1;
			uint32_t xtpr : 1;
			uint32_t pdcm : 1;
		        /* 16 */
			uint32_t reserved6 : 1;
			uint32_t pcid : 1;
			uint32_t dca : 1;
			uint32_t sse41 : 1;
		        /* 20 */
			uint32_t sse42 : 1;
			uint32_t x2apic : 1;
			uint32_t movbe : 1;
			uint32_t popcnt : 1;
		        /* 24 */
			uint32_t tscdeadline : 1;
			uint32_t aes : 1;
			uint32_t xsave : 1;
			uint32_t osxsave : 1;
		        /* 28 */
			uint32_t avx : 1;
			uint32_t f16c : 1;
			uint32_t rdmd : 1;
			uint32_t hypervisor : 1;
		} data;
	};

	union basicinfo info;
	info.reg[0] = eax;
	info.reg[1] = edx;
	info.reg[2] = ecx;
	info.reg[3] = ebx;

	if (info.data.cmov)
		features |= CPUCMOV;
	if (info.data.sse)
		features |= CPUSSE;
	if (info.data.sse2)
		features |= CPUSSE2;
	if (info.data.sse3)
		features |= CPUSSE3;
	if (info.data.popcnt)
		features |= CPUPOPCNT;
	if (info.data.sse41)
		features |= CPUSSE41;
	if (info.data.sse42)
		features |= CPUSSE42;
	if (info.data.avx)
		features |= CPUAVX;

	/* Read more flags from op 7 */
	op = 7;
	__get_cpuid(op, &eax, &ebx, &ecx, &edx);

	union extraflags {
		uint32_t reg[4];
		struct {
			/* eax */
			uint32_t maxsublevel;
			/* ebx */
			uint32_t fsgsbase : 1;
			uint32_t reserved1 : 2;
			uint32_t bmi1 : 1;
			/* 4 */
			uint32_t hle : 1;
			uint32_t avx2 : 1;
			/* rest not interesting yet */
		} data;
	};

	union extraflags flags;
	flags.reg[0] = eax;
	flags.reg[1] = ebx;
	flags.reg[2] = ecx;
	flags.reg[3] = edx;

	if (flags.data.avx2)
		features |= CPUAVX2;
#endif
}

void cpu_init()
{
	x86_cpu_init();
}

bool cpu_supports(enum cpufeatures feature)
{
	return feature & features;
}

