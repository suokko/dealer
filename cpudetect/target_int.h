
#ifdef MVDEFAULT
#define DEFUN(x) default_##x
#elif defined(SLOWMUL)
#define DEFUN(x) slowmul_##x
#elif defined(__AVX2__)
#define DEFUN(x) avx2_##x
#elif defined(__AVX__)
#define DEFUN(x) avx_##x
#elif defined(__SSE4_2__)
#define DEFUN(x) sse4_##x
#elif defined(__POPCNT__)
#define DEFUN(x) popcnt_##x
#elif defined(__SSE3__)
#define DEFUN(x) sse3_##x
#elif defined(__SSE2__)
#define DEFUN(x) sse2_##x
#else
#define DEFUN(x) default_##x
#endif

void DEFUN(test)(int);
