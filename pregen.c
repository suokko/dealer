#include <stdio.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

static unsigned long ncrtable[14*13 + 1];

static unsigned ncridx(int n, int r)
{
	return n*13 + r;
}

static unsigned long ncr(int n, int r)
{
	if (0)
		printf("n: %d, r: %d = %ld\n", n, r, ncrtable[ncridx(n,r)]);
	return ncrtable[ncridx(n, r)];
}

static __attribute__((constructor)) void initncr(void)
{
	int n,r;
	for (n = 0; n < 14; n++) {
		for (r = 0; r < n + 1; r++) {
			if (r == 0 || r == n)
				ncrtable[ncridx(n, r)] = 1;
			else
				ncrtable[ncridx(n, r)] = ncr(n - 1, r - 1) +
					ncr(n - 1, r);
		}
	}
}

static int doincludes(void)
{
	puts("/* This file is generated source. There is no point editing it. */\n"
		"\n#include \"../pregen.h\"\n");
	return 0;
}

static int doprngtables(void)
{
	long cards, pot, idx = 32; // Starts with pot table
	const long ratelimit = 64;

	struct entry {
		unsigned mask;
		unsigned idx;
	};

	struct entry results[52] = {{0}};

	for (cards = 2; cards < 52; cards++) {
		long bestreminder = LONG_MAX;
		long bestrate = ratelimit - 1;
		long bestpot = 0;
		if (((cards - 1) & cards) == 0)
			continue;
		for (pot = ratelimit; pot < (1L << 56); pot <<= 1) {
			if (cards > pot)
				continue;
			long reminder = pot % cards;
			long missrate = pot / (reminder);
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

		iter += sprintf(iter, "%ld,", cards - 1);

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

		printf("\t\t{.idx = %u, // %lu\n"
			"\t\t.mask = %u,},\n",
			lidx, cards, mask);
	}
	puts("\t}\n};");
	return 0;
}

struct patternparam {
	__int128 total;
	void (*complete)(struct patternparam *p);
	unsigned long count[4];
	unsigned max[4];
	unsigned len[16];
	unsigned i;
	unsigned nrhands;
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
				p->count[0] = ncr(p->max[0], p->len[0]) *
					ncr(p->max[1], p->len[1]) *
					ncr(p->max[2], p->len[2]) *
					ncr(p->max[3], p->len[3]);
				p->complete(p);
			}
		}
	}
}

static void patternprintstats(struct patternparam *p)
{
	unsigned i;
	__int128 cnt = 1;
	printf("%03u: ", p->i);

	for (i = 3; i < 4; i--) {
		if (p->count[i] == 0)
			continue;
		cnt *= p->count[i];
		printf("%x%x%x%x ",
				p->len[3 + i*4],
				p->len[2 + i*4],
				p->len[1 + i*4],
				p->len[0 + i*4]);
	}
	p->total += cnt;
	printf("%23lu %16lx %16lx\n",
			(unsigned long)cnt, (unsigned long)(p->total >> 64), (unsigned long)p->total);
	p->i++;
	p->count[0] = 1;
}

static void patternhand(struct patternparam *p)
{
	unsigned i;
	if (p->nrhands == 1) {
		patternprintstats(p);
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
	struct patternparam param = {
		.nrhands = nrhands,
		.max = {13,13,13,13},
		.complete = patternhand,
	};
	patternrunloop(&param);
	return 0;
}

static int rngtablestatistics(void)
{
	long cards;
	long pot;
	long total = 0;
	long missrate = 400;
	long best = 0;
	long maxrate[53] = {0};
	printf("rate\tbytes\taverate\tbytes/rate\n");
	for (missrate = 1; missrate < 1L << 32;) {
		total = 0;

		for (cards = 2; cards < 53; cards++) {
			for (pot = 1; pot < (1L << 56); pot <<= 1) {
				if (cards > pot)
					continue;

				if (pot % cards == 0)
					break;

				if (pot / (pot % cards) > missrate)
					break;
			}
			if (pot % cards != 0 && maxrate[cards] < pot/(pot % cards)) {
				maxrate[cards] = pot/(pot % cards);
				printf("cards %ld: %ld / %ld = %ld\n",
						cards, pot, pot % cards, maxrate[cards]);
			}
			total += pot;
		}
		long nextrate = LONG_MAX;
		long avgrate = 0, count = 0;

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
			printf("%ld\t%ld\t%ld\t%ld\n",
					nextrate, total,
					(avgrate+count/2)/count,
					total/((avgrate+count/2)/count));
		}
		missrate = nextrate;
		best = total;
	}

	return 0;
}

static int help(char *prog)
{
	printf("%s - is a helper tool to generate probability and randomization tables\n"
			"\n"
			"\t-h\t\tPrint this help\n"
			"\t-s\t<param>\tPrint statistics for a table [rngtable,1hpattern,2hpattern]\n"
			"\t-r\t\tCreates optimized lookup tables for shuffling\n",
			prog);
	return 0;
}

static int dostatistics(char *prog, char *arg)
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

	while ((c = getopt(argc, argv, "s:rih")) != - 1) {
		switch(c) {
		case 's':
			dostatistics(argv[0], optarg);
			break;
		case 'r':
			doprngtables();
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
