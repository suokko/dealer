
#pragma once

#if defined(__x86_64__) || defined(__i386__)

#ifndef __AVX2__
#define __AVX2__
#include "target_int.h"
#undef __AVX2__
#else
#define NAVX2
#include "target_int.h"
#undef __AVX2__
#endif
#undef DEFUN

#ifndef __AVX__
#define __AVX__
#include "target_int.h"
#undef __AVX__
#else
#define NAVX
#include "target_int.h"
#undef __AVX__
#endif
#undef DEFUN

#ifndef __SSE4_2__
#define __SSE4_2__
#include "target_int.h"
#undef __SSE4_2__
#else
#define NSSE4_2
#include "target_int.h"
#undef __SSE4_2__
#endif
#undef DEFUN

#ifndef __POPCNT__
#define __POPCNT__
#include "target_int.h"
#undef __POPCNT__
#else
#define NPOPCNT
#include "target_int.h"
#undef __POPCNT__
#endif
#undef DEFUN

#ifndef __SSE3__
#define __SSE3__
#include "target_int.h"
#undef __SSE3__
#else
#define NSSE3
#include "target_int.h"
#undef __SSE3__
#endif
#undef DEFUN

#ifndef __SSE2__
#define __SSE2__
#include "target_int.h"
#undef __SSE2__
#else
#define NSSE2
#include "target_int.h"
#undef __SSE2__
#endif
#undef DEFUN

#endif /* x86 */

#ifndef SLOWMUL
#define SLOWMUL
#include "target_int.h"
#undef SLOWMUL
#else
#define NSLOWMUL
#include "target_int.h"
#undef SLOWMUL
#endif
#undef DEFUN

#include "target_int.h"
#undef DEFUN

/* Restore preprocessor state */
#ifdef NAVX2
#define __AVX2__
#endif

#ifdef NAVX
#define __AVX__
#endif

#ifdef NSSE4_2
#define __SSE4_2__
#endif

#ifdef NPOPCNT
#define __POPCNT__
#endif

#ifdef NSSE3
#define __SSE3__
#endif

#ifdef NSSE2
#define __SSE2__
#endif

#ifdef NSLOWMUL
#define SLOWMUL
#endif
