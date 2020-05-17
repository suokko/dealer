#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

enum cpufeatures {
	CPUCMOV		= 0x001,
	CPUMMX		= 0x002,
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
#endif
