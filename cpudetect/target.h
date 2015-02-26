
#pragma once

#ifndef MVDEFAULT

#if defined(__x86_64__) || defined(__i386__)

#define MVDEFAULT sse2
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT sse3
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT popcnt
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT sse4
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT avx
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT avx2
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#endif /* x86 */

#define MVDEFAULT default
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#define MVDEFAULT slowmul
#include "target_int.h"
#undef DEFUN
#undef MVDEFAULT

#endif
