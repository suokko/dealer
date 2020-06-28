#pragma once

#include <string>

/**
 * Feature bits for different CPU features. These are combined to a bit mask in
 * runtime feature detection. Query for features is done using
 * cpu::detect::supports()
 */
enum cpufeatures {
	CPUDEFAULT	= 0x000,
	CPUSSE		= 1,
	CPUSSE2		= CPUSSE << 1,
	CPUSSE3		= CPUSSE2 << 1,
	CPUSSSE3	= CPUSSE3 << 1,
	CPUPOPCNT	= CPUSSSE3 << 1,
	CPUSSE41	= CPUPOPCNT << 1,
	CPUSSE42	= CPUSSE41 << 1,
	CPULZCNT	= CPUSSE42 << 1,
	CPUBMI		= CPULZCNT << 1,
	CPUAVX		= CPUBMI << 1,
	CPUBMI2		= CPUAVX << 1,
	CPUAVX2		= CPUBMI2 << 1,
};

namespace cpu {

/**
 * A singleton providing ability to check cpu instruction set support in runtime
 * and compiler time.
 */
struct detect {
	/// Singleton management interface
	static const detect& instance();

	/// Check if cpu supports required feature
	bool supports(enum cpufeatures feature) const;
	/// Check if cpu support all required features in a bit mask
	bool supports(unsigned feature) const;

	/// Return features supported by current compiler options
	static constexpr unsigned compiler_features() {
		return CPUDEFAULT
#if __SSE__
			| CPUSSE
#endif
#if __SSE2__
			| CPUSSE2
#endif
#if __SSE3__
			| CPUSSE3
#endif
#if __SSSE3__
			| CPUSSSE3
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
#if __LZCNT__ || __ABM__
			| CPULZCNT
#endif
#if __BMI__
			| CPUBMI
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

	/// Check if compiler supports a required feature
	static constexpr bool compiler_supports(cpufeatures feature)
	{
		return compiler_features() & feature;
	}

	/// Return human readable list for compiler supported instruction sets
	static const std::string& compiler_string();
	/// Return human readable list for runtime supported instruction sets
	const std::string& cpu_string() const;
private:
	/// Private constructor for singleton
	detect();

	/// Singleton must not be copied
	detect(const detect&) = delete;

	/// Cached runtime instruction set bit mask
	unsigned features_;
};

} // namespace cpu
