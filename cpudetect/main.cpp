#include "target.h"
#include <stdio.h>
#include "detect.h"

int main(void)
{
	auto compiler = cpu::detect::compiler_string();
	auto runtime = cpu::detect::instance().cpu_string();

	printf("Compiler supports: %s\n"
			"CPU supports: %s\n",
			compiler.c_str(),
			runtime.c_str());

	if (cpu::detect::instance().supports(CPUPOPCNT))
		popcnt_::test(55);
	default_::test(55);
	slowmul_::test(55);

	return 0;
}
