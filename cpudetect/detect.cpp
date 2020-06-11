#include "detect.h"
#include <cassert>
#include <set>
#include <regex>

namespace cpu {

namespace {

#if defined(__i386__) || defined(__x86_64__)
static int get_cpuid(unsigned op, unsigned subop, unsigned &a, unsigned &b, unsigned &c, unsigned &d)
{
	static unsigned max = 0;
	static unsigned extmax = 0;
	constexpr unsigned ext = 0x80000000;
	if ((op & ext) == 0) {
		if (op > max)
			return -1;
	} else {
		if ((op & ~ext) > extmax)
			return -1;
	}

	asm ("\tcpuid\n"
			: "=a" (a), "=b" (b), "=c"(c), "=d"(d)
			: "0" (op), "2" (subop)
			);

	if ((op & ext) == 0) {
		if (op == 0)
			max = a;
	} else {
		if ((op & ~ext) == 0)
			extmax = a;
	}

	return 0;
}
#endif

static unsigned check_env(unsigned features)
{
	const char *env = getenv("CPUSUPPORTS");
	if (!env)
		return features;

	std::regex all_flags{
		"\\bno(?:"
		"(((sse)|(sse2)|sse3)|"
		"(ssse3)|(popcnt)|sse4\\.?1)|"
		"((sse4\\.?2)|(avx)|bmi)|"
		"(lzcnt)|(bmi2)|avx2"
		")\\b",
		std::regex::optimize
	};

	const char *env_end = env + strlen(env);
	std::cregex_iterator iter{env, env_end, all_flags};
	std::cregex_iterator end{};

	for (; iter != end; ++iter) {
		assert(iter->size() == 10 + 2 &&
				"Number of submatches in regular isn't expected");
		if ((*iter)[1].matched) {
			// First half
			if ((*iter)[2].matched) {
				// First quarter
				if ((*iter)[3].matched)
					features &= ~CPUSSE;
				else if ((*iter)[4].matched)
					features &= ~CPUSSE2;
				else
					features &= ~CPUSSE3;
			} else {
				// Second quarter
				if ((*iter)[5].matched)
					features &= ~CPUSSSE3;
				else if ((*iter)[6].matched)
					features &= ~CPUPOPCNT;
				else
					features &= ~CPUSSE41;
			}
		} else {
			// Second half
			if ((*iter)[7].matched) {
				// Third quarter
				if ((*iter)[8].matched)
					features &= ~CPUSSE42;
				else if ((*iter)[9].matched)
					features &= ~CPUAVX;
				else
					features &= ~CPUBMI;
			} else {
				// Fourth quarter
				if ((*iter)[10].matched)
					features &= ~CPULZCNT;
				else if ((*iter)[11].matched)
					features &= ~CPUBMI2;
				else
					features &= ~CPUAVX2;
			}
		}
	}
	return features;
}

static unsigned x86_cpu_init()
{
	unsigned features = CPUDEFAULT;
#if defined(__i386__) || defined(__x86_64__)
	/* Fetch cpu features */
	unsigned eax = 0,
			 ebx = 0,
			 ecx = 0,
			 edx = 0,
			 op = 0,
			 subop = 0;
	if (get_cpuid(op, subop, eax, ebx, ecx, edx))
		return features;
	op = 1;
	if (get_cpuid(op, subop, eax, ebx, ecx, edx))
		return features;
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

	if (info.data.sse)
		features |= CPUSSE;
	if (info.data.sse2)
		features |= CPUSSE2;
	if (info.data.sse3)
		features |= CPUSSE3;
	if (info.data.ssse3)
		features |= CPUSSSE3;
	if (info.data.popcnt)
		features |= CPUPOPCNT;
	if (info.data.sse41)
		features |= CPUSSE41;
	if (info.data.sse42)
		features |= CPUSSE42;
	if (info.data.avx)
		features |= CPUAVX;

	op = 0x80000000;
	subop = 0;
	if (get_cpuid(op, subop, eax, ebx, ecx, edx))
		return features;

	if (eax >= 1 && !get_cpuid(op+1, subop, eax, ebx, ecx, edx)) {
		union ext_info {
			uint32_t reg[4];
			struct {
				/* eax */
				uint32_t unknown1;
				/* ebx */
				uint32_t unknown2;
				/* ecx */
				uint32_t lahf_lm : 1;
				uint32_t cmp_legacy : 1;
				uint32_t svm : 1;
				uint32_t extapic : 1;
				/* 4 */
				uint32_t cr8_legacy : 1;
				uint32_t abm : 1;
				uint32_t sse4a : 1;
				/* rest not interesting yet */
			} data;
		};
		union ext_info flags;
		flags.reg[0] = eax;
		flags.reg[1] = ebx;
		flags.reg[2] = ecx;
		flags.reg[3] = edx;

		if (flags.data.abm)
			features |= CPULZCNT;
	}

	/* Read more flags from op 7 */
	op = 7;
	subop = 0;
	if (get_cpuid(op, subop, eax, ebx, ecx, edx))
		return features;

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
			uint32_t reserved2 : 1;
			uint32_t smep : 1;
			/* 8 */
			uint32_t bmi2 : 1;
			/* rest not interesting yet */
		} data;
	};

	union extraflags flags;
	flags.reg[0] = eax;
	flags.reg[1] = ebx;
	flags.reg[2] = ecx;
	flags.reg[3] = edx;

	if (flags.data.bmi1)
		features |= CPUBMI;
	if (flags.data.bmi2)
		features |= CPUBMI2;
	if (flags.data.avx2)
		features |= CPUAVX2;
#endif
	return features;
}

static std::string make_string(unsigned features)
{
	std::string rv;
	const char* sep = "";
	while (features) {
		unsigned next = (features - 1) & features;
		cpufeatures lowest = static_cast<cpufeatures>(features ^ next);
		features = next;
		switch(lowest) {
		case CPUDEFAULT:
			assert(0 && "Default should be impossible");
			return rv;
		case CPUSSE:
			rv = rv + sep + "sse";
			break;
		case CPUSSE2:
			rv = rv + sep + "sse2";
			break;
		case CPUSSE3:
			rv = rv + sep + "sse3";
			break;
		case CPUSSSE3:
			rv = rv + sep + "ssse3";
			break;
		case CPUPOPCNT:
			rv = rv + sep + "popcnt";
			break;
		case CPUSSE41:
			rv = rv + sep + "sse4.1";
			break;
		case CPUSSE42:
			rv = rv + sep + "sse4.2";
			break;
		case CPUAVX:
			rv = rv + sep + "avx";
			break;
		case CPULZCNT:
			rv = rv + sep + "lzcnt";
			break;
		case CPUBMI:
			rv = rv + sep + "bmi";
			break;
		case CPUBMI2:
			rv = rv + sep + "bmi2";
			break;
		case CPUAVX2:
			rv = rv + sep + "avx2";
			break;
		}
		sep = ",";
	}
	return rv;
}

}

detect::detect() :
	features_{x86_cpu_init()}
{
	features_ = check_env(features_);
}

const detect& detect::instance()
{
	static detect inst{};
	return inst;
}

bool detect::supports(cpufeatures feature) const
{
	return feature & features_;
}

bool detect::supports(unsigned feature) const
{
	return (feature & features_) == feature;
}

const std::string& detect::compiler_string()
{
	static std::string rv{make_string(compiler_features())};
	return rv;
}

const std::string& detect::cpu_string() const
{
	static std::string rv{make_string(features_)};
	return rv;
}

} // namespace cpu

