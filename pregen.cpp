#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "pregen.h"

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

struct patternparam {
	int128 total;
	void (*complete)(struct patternparam *p);
	uint64_t count[4];
	unsigned max[4];
	unsigned len[16];
	unsigned handidx;
	unsigned nrhands;
	patternparam()
	{
		memset(this, 0, sizeof *this);
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

static int patternstatistics(int nrhands)
{
	struct patternparam param;
	param.nrhands = nrhands;
	param.max[0] = param.max[1] = param.max[2] = param.max[3] = 13;
	param.complete = patternprintstats;

	printf("idx  cdhs%s%s %10s %20s\n",
		(nrhands > 1 ? " cdhs" : ""),
		(nrhands > 2 ? " cdhs" : ""),
		"count", "total");
	patternrunloop(&param);
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

static int dostatistics(const char *prog, const char *arg)
{
	if (strcmp("rngtable", arg) == 0)
		return rngtablestatistics();
	else if (strcmp("1hpattern", arg) == 0)
		return patternstatistics(1);
	else if (strcmp("2hpattern", arg) == 0)
		return patternstatistics(2);
	else if (strcmp("3hpattern", arg) == 0)
		return patternstatistics(3);
	else if (strcmp("4hpattern", arg) == 0)
		return patternstatistics(4);

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
