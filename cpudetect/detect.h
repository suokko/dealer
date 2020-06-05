#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

enum cpufeatures {
	CPUDEFAULT	= 0x000,
	CPUSSE		= 0x004,
	CPUSSE2		= 0x008,
	CPUSSE3		= 0x010,
	CPUPOPCNT	= 0x020,
	CPUSSE41	= 0x040,
	CPUSSE42	= 0x080,
	CPUAVX		= 0x100,
	CPUBMI2		= 0x200,
	CPUAVX2		= 0x400,
};

void cpu_init();
bool cpu_supports(enum cpufeatures feature);

#ifdef __cplusplus
}

namespace cpu {

struct detect {
	detect();

	/// Singleton management interface
	static const detect& instance();

	/// helper to check if cpu supports required feature
	bool supports(enum cpufeatures feature) const;
	bool supports(unsigned feature) const;

	/// Return features supported by current compiler options
	static constexpr unsigned compiler_supports() {
		return CPUDEFAULT
#if __SSE__
			| CPUSSE
#endif
#if __SSE2__
			| CPUSSE2
#endif
#if __SSSE3__
			| CPUSSE3
#endif
#if __POPCNT__
			| CPUPOPCNT
#endif
#if __SSE4_1__
			| CPUSSE41
#endif
#if __SSE4_2__
			| CPUSSE42
#endif
#if __AVX__
			| CPUAVX
#endif
#if __AVX2__
			| CPUAVX2
#endif
#if __BMI2__
			| CPUBMI2
#endif
			;
	}

	static constexpr unsigned feature_id()
	{
#ifdef MVDEFAULT
		return CPUDEFAULT;
#else
		return compiler_supports();
#endif
	}
private:
	unsigned features_;
};

} // namespace cpu

#endif
