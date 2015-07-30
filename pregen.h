#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct prngtable {
	const unsigned char *table;
	struct entry {
		unsigned int idx;
		unsigned int mask;
	} entries[51];
};

extern const struct prngtable prnglookup;

/* Watch out for double evaluation */
/* The last element of row is read from next row */
#define ncridx(n, r) (((n-1)*n)>>1)+r

extern const unsigned ncrtable[ncridx(14,1)];
extern const uint64_t ncrtablelarge[ncridx(52,14)];

static inline unsigned ncr(unsigned n, unsigned r)
{
	return ncrtable[ncridx(n, r)];
}

static inline uint64_t ncrlarge(unsigned n, unsigned r)
{
	return ncrtablelarge[ncridx(n, r)];
}

#ifdef __cplusplus
}
#endif
