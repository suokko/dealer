#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

void cpu_init();
bool cpu_supports(const char *feature);

#ifdef __cplusplus
}
#endif
