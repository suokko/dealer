#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <deque>
#include <algorithm>

#include "pregen.h"
#include "bittwiddle.h"

static uint64_t ncrtablegen[ncridx(52,14)];

static uint64_t ncrgen(int n, int r)
{
	return ncrtablegen[ncridx(n, r)];
}

static __attribute__((constructor)) void initncr(void)
{
	int n,r;
	for (n = 0; n < 52; n++) {
		for (r = 0; r < n + 1; r++) {
			if (r == 0 || r == n)
				ncrtablegen[ncridx(n, r)] = 1;
			else
				ncrtablegen[ncridx(n, r)] = ncrgen(n - 1, r - 1) +
					ncrgen(n - 1, r);
		}
	}
	for (r = 0; r < 14; r++) {
		if (r == 0 || r == n)
			ncrtablegen[ncridx(n, r)] = 1;
		else
			ncrtablegen[ncridx(n, r)] = ncrgen(n - 1, r - 1) +
				ncrgen(n - 1, r);
	}
}

static int doncrtables(void)
{
	unsigned i;
	printf("const unsigned ncrtable[] = {\n\t");
	for (i = 0; i < ncridx(14,1); i++) {
		printf("0x%x,",(unsigned)ncrtablegen[i]);
		if (i % 8 == 0 && i != 0)
			printf(" // %d\n\t", i);
	}
	puts("};");
	printf("const uint64_t ncrtablelarge[] = {\n\t");
	for (i = 0; i < ncridx(52,14); i++) {
		printf("0x%" PRIx64",",ncrtablegen[i]);
		if (i % 8 == 0 && i != 0)
			printf(" // %d\n\t", i);
	}
	puts("};");
	return 0;
}

static int doincludes(void)
{
	puts("/* This file is generated source. There is no point editing it. */\n"
		"\n#include \"../pregen.h\"\n");
	return 0;
}

static int doprngtables(void)
{
	int64_t cards, pot, idx = 32; // Starts with pot table
	const int64_t ratelimit = 64;

	struct entry {
		unsigned mask;
		unsigned idx;

		entry() : mask(0), idx(0)
		{}
	};

	struct entry results[52];

	for (cards = 2; cards < 52; cards++) {
		int64_t bestreminder = INT64_MAX;
		int64_t bestrate = ratelimit - 1;
		int64_t bestpot = 0;
		if (((cards - 1) & cards) == 0)
			continue;
		for (pot = ratelimit; pot < (1LL << 56); pot <<= 1) {
			if (cards > pot)
				continue;
			int64_t reminder = pot % cards;
			int64_t missrate = pot / (reminder);
			if (missrate > bestrate)
				bestrate = missrate;
			else
				continue;

			if (reminder >= bestreminder)
				break;

			bestreminder = reminder;
			bestpot = pot;
		}

		results[cards - 2].mask = bestpot - 1;
		results[cards - 2].idx = idx;
		idx += bestpot;
	}
	// 52 cards remaining is common case so make it use larger table
	results[cards - 2].mask = (1 << 13) - 1;
	results[cards - 2].idx = idx;

	// Print the lookup table
	puts("static const unsigned char _prngtable[] = {");

	char buffer[1024];
	char *iter = buffer;
	char termination[] = "\t255,255,255,255,255,255,255,255," // 8
		"255,255,255,255,255,255,255,255," // 16
		"255,255,255,255,255,255,255,255," // 24
		"255,255,255,255,255,255,255,255," // 32
		"255,255,255,255,255,255,255,255," // 40
		"255,255,255,255,255,255,255,255," // 48
		"255,255,255,255"; // 52

	iter += sprintf(iter, "\t0,");
	putchar('\t');
	for (cards = 0; cards < 32; cards++) {
		printf("%d,", (int)cards);
	}
	putchar('\n');

	for (cards = 2; cards < 53; cards++) {

		iter += sprintf(iter, "%" PRId64 ",", cards - 1);

		if (results[cards - 2].mask == 0) {
			results[cards - 2].mask = cards - 1;
			continue;
		}

		unsigned count;

		for (count = 0; count + cards <= results[cards - 2].mask; count += cards) {
			puts(buffer);
		}

		count = results[cards - 2].mask - count + 1;

		fwrite(termination, 1, count*4+1, stdout);
		putchar('\n');
	}

	puts("};\n");
	// print the headers

	puts("const struct prngtable prnglookup = {\n"
		"\t.table = _prngtable,\n"
		"\t.entries = {");
	for (cards = 2; cards < 53; cards++) {
		unsigned mask = results[cards - 2].mask;
		unsigned lidx = results[cards - 2].idx;

		printf("\t\t{.idx = %u, // %" PRIu64"\n"
			"\t\t.mask = %u,},\n",
			lidx, cards, mask);
	}
	puts("\t}\n};");
	return 0;
}

#ifdef __amd64__
typedef __int128 int128;
#else
struct fint128 {
	int64_t high;
	uint64_t low;

	fint128() : high(0), low(0)
	{}

	fint128(int val) :
		high(val < 0 ? INT64_MIN : 0),
		low(val)
	{}

	fint128 operator *=(const uint64_t &mul)
	{
		uint64_t m0 = mul & UINT32_MAX;
		uint64_t m1 = mul >> 32;
		uint64_t v0 = low & UINT32_MAX;
		uint64_t v1 = low >> 32;

		uint64_t r0 = v0 * m0;
		uint64_t r1l = v0 * m1;
		uint64_t r1h = v1 * m0;
		r1l += r1h;
		uint64_t r2 = v1 * m1 + (r1l >> 32); // no overflow to r2
		r2 += (r1l < r1h);
		if (high) {
			uint64_t v2 = high & UINT32_MAX;
			r2 += v2 * m0;
		}
		low = r0 + (r1l << 32);
		high = r2 + (low < r0);
		return *this;
	}

	fint128 operator +=(const fint128 &add);

	operator uint64_t() const
	{
		return low;
	}

	fint128 operator >>(const int shift) const
	{
		fint128 r(0);

		if (__builtin_constant_p(shift) && shift == 64) {
			r.low = high;
			return r;
		}
		r.low = low >> shift | high << (64 - shift);
		r.high = high >> shift;
		return r;
	}
};

fint128 fint128::operator +=(const fint128 &add)
{
	low += add.low;
	high += add.high;
	high += low < add.low;
	return *this;
}

typedef struct fint128 int128;
#endif

struct patternentry {
	uint64_t count;
	uint16_t len[3];

	patternentry() :
		count(1),
		len{0,0,0}
	{}
};

struct patternparam {
	int128 total;
	std::deque<patternentry> tosort;
	void (*complete)(struct patternparam *p);
	uint64_t count[4];
	unsigned max[4];
	unsigned len[16];
	unsigned handidx;
	unsigned nrhands;
	patternparam() :
		total(0),
		tosort(),
		count{0,0,0,0},
		len{0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0},
		handidx(0),
		nrhands(0)
	{
	}
};

void patterncalcminmax(unsigned *min, unsigned *max, struct patternparam *p, unsigned level)
{
	unsigned i, dealt = 0, freeafter = 0;
	for (i = 0; i < level; i++) {
		dealt += p->len[i];
	}
	for (i = level + 1; i < 4; i++) {
		freeafter += p->max[i];
	}

	*min = 0;
	*max = 13 - dealt;

	if (freeafter < *max)
		*min = *max - freeafter;

	if (*max > p->max[level])
		*max = p->max[level];

	*max += 1;
}

static void patternhand(struct patternparam *p);

static void patternrunloop(struct patternparam *p)
{
	unsigned ma0, ma1, ma2;
	unsigned mi0, mi1, mi2;
	patterncalcminmax(&mi0, &ma0, p, 0);
	for (p->len[0] = mi0; p->len[0] < ma0; p->len[0]++) {
		patterncalcminmax(&mi1, &ma1, p, 1);
		for (p->len[1] = mi1; p->len[1] < ma1; p->len[1]++) {
			patterncalcminmax(&mi2, &ma2, p, 2);
			for (p->len[2] = mi2; p->len[2] < ma2; p->len[2]++) {
				p->len[3] = 13 - p->len[0] - p->len[1] - p->len[2];
				p->count[0] = ncrgen(p->max[0], p->len[0]) *
					ncrgen(p->max[1], p->len[1]) *
					ncrgen(p->max[2], p->len[2]) *
					ncrgen(p->max[3], p->len[3]);
				patternhand(p);
			}
		}
	}
}

static void patternprintstats(struct patternparam *p)
{
	unsigned i;
	int128 cnt = 1;
	printf("%03u: ", p->handidx);

	for (i = 3; i < 4; i--) {
		if (p->count[i] == 0)
			continue;
		cnt *= p->count[i];
		printf("%x%x%x%x ",
				p->len[0 + i*4],
				p->len[1 + i*4],
				p->len[2 + i*4],
				p->len[3 + i*4]);
	}
	p->total += cnt;
	printf("%10" PRId64": %16" PRIx64" %16" PRIx64"\n",
			(uint64_t)cnt, (uint64_t)(p->total >> 64), (uint64_t)p->total);
	p->handidx++;
	p->count[0] = 1;
}

static void patternstore(struct patternparam *p)
{
	unsigned i;
	patternentry e;

	for (i = 3; i < 4; i--) {
		if (p->count[i] == 0)
			continue;
		e.count *= p->count[i];
		e.len[i] = p->len[0 + i*4] << 0;
		e.len[i] |= p->len[1 + i*4] << 4;
		e.len[i] |= p->len[2 + i*4] << 8;
	}
	p->tosort.push_back(e);
	p->count[0] = 1;
}

static void patternprintstatsuniq(struct patternparam *p)
{
	unsigned i,j;
	unsigned idxchange = 1;
	unsigned equals[4] = {0,0,0,0};
	int128 cnt = 1;
	unsigned filter = 0;
	unsigned same = 0;

	for (i = 0; i < 3; i++) {
		if (p->len[i] < p->len[i + 1]) {
			filter = i << 16;
			goto out;
		}
	}

	for (i = 1; i < 4; i++) {
		if (p->count[i] == 0)
			continue;

		unsigned temp[4] = {
			p->len[0 + i*4],
			p->len[1 + i*4],
			p->len[2 + i*4],
			p->len[3 + i*4],
		};

		std::sort(temp, temp + 4, [](unsigned a, unsigned b) {return a > b;});

		/* Limit generated second distributions only to smaller than
		 * or equal to first. This avoids generating same pairs again
		 */
		if (temp[0] > p->len[0]) {
			filter = 1 << 2;
			goto out;
		}
		if (temp[0] == p->len[0]) {
			if (temp[1] > p->len[1]) {
				filter = 2 << 4;
				goto out;
			}
			if (temp[1] == p->len[1]) {
				if (temp[2] > p->len[2]) {
					filter = 3 << 4;
					goto out;
				}
			}
		}
		/* If first has two or more equals filter out duplicates
		 * from reordering the second
		 */
		for (j = 0; j < 3; j++) {
			if (p->len[j + (i-1)*4] == p->len[j + 1 + (i-1)*4] &&
					p->len[j + i*4] < p->len[j + 1 + i*4]) {
				filter = j << 8;
				goto out;
			}
		}

		if (temp[0] == p->len[0] &&
			temp[1] == p->len[1] &&
			temp[2] == p->len[2] &&
			temp[3] == p->len[3]) {

			/* With equal shape and two equal suit lengths
			 * duplicates can be generated that needs to be
			 * filtered.
			 */
			for (j = 0; j < 3; j++) {
				if (temp[j] != temp[j+1])
					continue;
				if (j < 2 && temp[j] == temp[j+2])
					break;
				unsigned x = (j + 2) % 4;
				unsigned y = (j + 3) % 4;
				if (x > y)
					std::swap(x, y);

				if (p->len[x + i*4] == temp[y] &&
						p->len[y + i*4] == temp[j]) {
					filter = 1 << 12;
					goto out;
				}
			}
		}
	}
	printf("%4u: ", p->handidx);

	/* Count how many ways this pair needs to be generated.
	 * All 4 in both unique length (5431 opposite 6431) can be done
	 * 4*3*2=24 ways. There is 24 unique entries.
	 * There is two equals in one of hands (5431 opposite 4432) can be done
	 * 4*3*2=24 ways. There is 12 unique entries.
	 * There is three equals in one of hands (5431 opposite 4333) can be
	 * done 4*3*2=24 ways. There is 4 unique entries.
	 * There is two equals in both hands (4432 opposite 4432) can be done
	 * (3+2+1)*2=12 ways. There is 6 unique entries three with double factor
	 * and one with quadruple factor.
	 * There is two equals and three equals (4432 opposite 4333) can be done
	 * (3+2+1)*2*2=24 ways. There is 3 unique entries with one having double
	 * factor.
	 * There is three and three equals (4333 oposite 4333) can be done
	 * 4 ways. There is 2 unique entries with one having triple factor.
	 */

	for (i = 0; i < 4; i++) {

		unsigned temp[4] = {
			p->len[0 + i*4],
			p->len[1 + i*4],
			p->len[2 + i*4],
			p->len[3 + i*4],
		};

		if (i > 0)
			std::sort(temp, temp + 4, [](unsigned a, unsigned b) {return a > b;});

		if (i == 1)
			same = temp[0] == p->len[0] &&
				temp[1] == p->len[1] &&
				temp[2] == p->len[2];

		for (j = 0; j < 3; j++) {
			if (temp[j] == temp[j + 1])
				equals[i]++;
		}
	}
	if (equals[0] == 0 || equals[1] == 0) {
		idxchange = 24;
		if (!same)
			idxchange *= 2;
	} else if (equals[0] == 2 && equals[1] == 2) {
		idxchange = 4;
		i = 3;
		if (p->len[0] != p->len[1])
			i = 0;

		if (p->len[4] != p->len[5]) {
			j = p->len[5] == p->len[6] ? 4 : 5;
		} else {
			j = p->len[6] != p->len[4] ? 6 : 7;
		}

		if (!same)
			idxchange *= 2;
		if (i + 4 != j)
			idxchange *= 3;
	} else if (equals[0] == 1 && equals[1] == 1) {
		idxchange = 24;
		unsigned x, y;
		for (x = 0; x < 3; x++) {
			if (p->len[x] == p->len[x+1])
				break;
		}
		y = x + 1;

		for (i = 0; i < 3; i++) {
			for (j = i+1; j < 4; j++) {
				if (p->len[i+4] == p->len[j+4])
					break;
			}
			if (j < 4)
				break;
		}

		unsigned imatch = i == x || i == y;
		unsigned jmatch = j == x || j == y;

		if (!imatch && !jmatch) {
			idxchange = 24;
			if (!same)
				idxchange *= 2;
		} else if (imatch ^ jmatch) {
			idxchange = 24;
			if (!same)
				idxchange *= 2;
			else {
				x = 0;
				if (x == i)
					x = j == 1 ? 2 : 1;
				y = x + 1;
				if (y == i || y == j)
					y = j == y + 1 ? y + 2 : y + 1;

				if (p->len[x] != p->len[x + 4] &&
						p->len[y] != p->len[y + 4])
					idxchange *= 2;
			}
		} else {
			idxchange = 12;
			if (!same)
				idxchange *= 2;
		}

	} else if (equals[0] == 2) {
		idxchange = 24;
		unsigned uniq = 3;
		if (p->len[0] != p->len[1])
			uniq = 0;

		for (i = 0; i < 3; i++) {
			for (j = i+1; j < 4; j++) {
				if (p->len[i+4] == p->len[j+4])
					break;
			}
			if (j < 4)
				break;
		}
		if (uniq == i || uniq == j)
			idxchange *= 2;
	} else {
		idxchange = 24;
		unsigned uniq = 0;
		assert(equals[0] == 1);
		assert(equals[1] == 2);
		for (uniq = 1; uniq < 4; uniq++) {
			if (p->len[4] != p->len[uniq+4])
					break;
		}
		if (p->len[(uniq+1) % 4 + 4] != p->len[4])
			uniq = 0;

		for (i = 0; i < 4; i++) {
			if (i == uniq)
				continue;
			if (p->len[i] == p->len[uniq])
				idxchange = 48;
		}
	}

	for (i = 3; i < 4; i--) {
		if (p->count[i] == 0)
			continue;
		cnt *= p->count[i];
		printf("%x%x%x%x ",
				p->len[0 + i*4],
				p->len[1 + i*4],
				p->len[2 + i*4],
				p->len[3 + i*4]);
	}
	p->total += cnt;
	printf("%10" PRIu64": %16" PRIx64" %16" PRIx64", %u\n",
			(uint64_t)cnt, (uint64_t)(p->total >> 64),
			(uint64_t)p->total, idxchange);
	p->handidx+=idxchange;
out:
	if (filter && false) {
		printf("%4x: ", filter);
		for (i = 3; i < 4; i--) {
			if (p->count[i] == 0)
				continue;
			cnt *= p->count[i];
			printf("%x%x%x%x ",
					p->len[0 + i*4],
					p->len[1 + i*4],
					p->len[2 + i*4],
					p->len[3 + i*4]);
		}
		printf("%lu\n", p->count[0]);
	}
	p->count[0] = 1;
}

static void patternprintlookup(struct patternparam *p) {
	uint64_t cnt = 1;
	unsigned i;
	for (i = 3; i < 4; i--) {
		if (p->count[i] == 0)
			continue;
		cnt *= p->count[i];
	}
	printf("%" PRIu64",%s",
			cnt,((p->handidx % 8) == 0 && p->handidx != 0) ? "\n\t": " ");
	p->handidx++;
	p->count[0] = 1;
}

static void patternprintstored(struct patternparam *p, int uniq)
{
	p->count[0] = p->count[1] = p->count[2] = p->count[3] = 0;
	unsigned i;
	for (i = 0; i < p->nrhands; i++)
		p->count[i] = 1;

	for (const patternentry &e : p->tosort) {
		for (i = 0; i < p->nrhands; i++) {
			p->len[0 + i*4] = e.len[i] & 0xF;
			p->len[1 + i*4] = e.len[i] >> 4 & 0xF;
			p->len[2 + i*4] = e.len[i] >> 8 & 0xF;
			p->len[3 + i*4] = 13 -
				p->len[0 + i*4] -
				p->len[1 + i*4] -
				p->len[2 + i*4];
		}
		p->count[0] = e.count;
		if (uniq)
			patternprintstatsuniq(p);
		else
			patternprintstats(p);
	}
}

static void patternhand(struct patternparam *p)
{
	unsigned i;
	if (p->nrhands == 1) {
		p->complete(p);
		return;
	}
	p->nrhands--;
	for (i = 11; i > 3; i--)
		p->len[4+i] = p->len[i];
	for (i = 0; i < 4; i++) {
		p->len[4+i] = p->len[i];
		p->max[i] = p->max[i] - p->len[i];
	}
	for (i = 3; i > 0; i--)
		p->count[i] = p->count[i - 1];
	patternrunloop(p);
	p->nrhands++;
	for (i = 0; i < 3; i++)
		p->count[i] = p->count[i + 1];
	p->count[3] = 0;
	for (i = 0; i < 4; i++) {
		p->len[i] = p->len[4+i];
		p->max[i] = p->max[i] + p->len[i];
	}
	for (i = 4; i < 12; i++)
		p->len[i] = p->len[i+4];
}

static int patternstatistics(int nrhands, int uniq, int sort)
{
	struct patternparam param;
	param.nrhands = nrhands;
	param.max[0] = param.max[1] = param.max[2] = param.max[3] = 13;
	param.complete = sort ? patternstore :
		(uniq ? patternprintstatsuniq : patternprintstats);

	printf("idx  shdc%s%s %10s %20s\n",
		(nrhands > 1 ? " shdc" : ""),
		(nrhands > 2 ? " shdc" : ""),
		"count", "total");
	patternrunloop(&param);
	if (sort) {
		std::sort(param.tosort.begin(), param.tosort.end(),
				[](const patternentry &a, const patternentry &b) {
					return a.count > b.count;
				});
		patternprintstored(&param, uniq);
	}
	return 0;
}

static int rngtablestatistics(void)
{
	int64_t cards;
	int64_t pot;
	int64_t total = 0;
	int64_t missrate = 400;
	int64_t best = 0;
	int64_t maxrate[53] = {0};
	printf("rate\tbytes\taverate\tbytes/rate\n");
	for (missrate = 1; missrate < 1LL << 32;) {
		total = 0;

		for (cards = 2; cards < 53; cards++) {
			for (pot = 1; pot < (1LL << 56); pot <<= 1) {
				if (cards > pot)
					continue;

				if (pot % cards == 0)
					break;

				if (pot / (pot % cards) > missrate)
					break;
			}
			if (pot % cards != 0 && maxrate[cards] < pot/(pot % cards)) {
				maxrate[cards] = pot/(pot % cards);
				printf("cards %" PRId64": %" PRId64" / %" PRId64" = %" PRId64"\n",
						cards, pot, pot % cards, maxrate[cards]);
			}
			total += pot;
		}
		int64_t nextrate = INT64_MAX;
		int64_t avgrate = 0, count = 0;

		for (cards = 0; cards < 53; cards++) {
			if (maxrate[cards] > 0) {
				avgrate += maxrate[cards];
				count++;
			}
			if (maxrate[cards] > 0 && maxrate[cards] < nextrate)
				nextrate = maxrate[cards];
		}

		avgrate += 4*maxrate[52];
		count += 4;

		if (best < total) {
			printf("%" PRId64"\t%" PRId64"\t%" PRId64"\t%" PRId64"\n",
					nextrate, total,
					(avgrate+count/2)/count,
					total/((avgrate+count/2)/count));
		}
		missrate = nextrate;
		best = total;
	}

	return 0;
}

static int help(const char *prog)
{
	printf("%s - is a helper tool to generate probability and randomization tables\n"
			"\n"
			"\t-h\t\tPrint this help\n"
			"\t-s\t<param>\tPrint statistics for a table [rngtable,1hpattern,2hpattern]\n"
			"\t-r\t\tCreates optimized lookup tables for shuffling\n"
			"\t-p\t<number>\tCreate hand pattern lookup tables for <number> hands\n",
			prog);
	return 0;
}

static int dopatterntables(const char *prog, const char *arg)
{
	if (arg[0] >= '1' && arg[0] <= '3') {
		struct patternparam param;
		param.nrhands = arg[0] - '0';
		param.max[0] = param.max[1] = param.max[2] = param.max[3] = 13;
		param.complete = patternprintlookup;

		printf("const uint64_t parttern%dh[] = {\n\t", param.nrhands);
		patternrunloop(&param);
		puts("};");
	} else {
		printf("Unssupported (%s) hand number. 1-3 are supported.\n", arg);
		help(prog);
		exit(1);
	}
	return 0;
}

static int hcpstatistics(void)
{
	unsigned matchingcount[41] = {0};
	uint16_t hcpbits;
	const uint16_t acemask = 0x8888;
	for (hcpbits = 0; hcpbits < UINT16_MAX; hcpbits++) {
		unsigned honours = popcount(hcpbits);
		if (honours > 13)
			continue;
		unsigned kingcount = popcount(hcpbits & (acemask | acemask >> 1));
		unsigned queencount = popcount(hcpbits & (acemask | acemask >> 2));
		unsigned hcp = honours + queencount + kingcount*2;

		matchingcount[hcp]++;
	}
	printf("hcp count\n");
	for (hcpbits = 0; hcpbits < 41; hcpbits++)
		printf("%2d: %4d\n", hcpbits, matchingcount[hcpbits]);
	return 0;
}

static int dostatistics(const char *prog, const char *arg)
{
	if (strcmp("rngtable", arg) == 0)
		return rngtablestatistics();
	else if (strncmp("hpattern", arg + 1, 8) == 0)
		return patternstatistics(arg[0] - '0',
				!!strstr(arg + 9,"uniq"),
				!!strstr(arg + 9,"sort"));
	else if (strcmp("hcp", arg) == 0)
		return hcpstatistics();

	return help(prog);
}

int main(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "s:rp:nih")) != - 1) {
		switch(c) {
		case 's':
			dostatistics(argv[0], optarg);
			break;
		case 'r':
			doprngtables();
			break;
		case 'p':
			dopatterntables(argv[0], optarg);
			break;
		case 'n':
			doncrtables();
			break;
		case 'i':
			doincludes();
			break;
		default:
			return help(argv[0]);
		}
	}
	return 0;
}
