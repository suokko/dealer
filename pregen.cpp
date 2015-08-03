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
#include <set>
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

struct Pattern {
	unsigned lengths[4];

	Pattern() : lengths{0,0,0,0}
	{}

	Pattern(unsigned s, unsigned h, unsigned d, unsigned c) :
		lengths{s,h,d,c}
	{}

	void sort()
	{
		std::sort(lengths, lengths + 4, std::greater<unsigned>());
	}

	bool operator<(const Pattern &b) const
	{
		for (unsigned i = 0; i < 4; i++) {
			if (lengths[i] < b.lengths[i])
				return true;
			if (lengths[i] != b.lengths[i])
				return false;
		}
		return false;
	}
};

struct GenericPattern {

	GenericPattern();
	/**
	 * @return nth specific pattern of the generic pattern
	 */
	Pattern lengths(unsigned permutation) const;

	unsigned maxPermutations() const;

	/**
	 * @return single suit length of the generic pattern
	 */
	unsigned suit(unsigned idx) const;

	void print() const;

	static unsigned addShape(unsigned s, unsigned h, unsigned d);
	static bool isAllowedPattern(Pattern &p);
	static unsigned fromPattern(const Pattern &p);
	static unsigned nrGP;
	static GenericPattern gpList[39];

private:
	void permutateDouble(Pattern &p, unsigned permutation,
		unsigned x0, unsigned x1, unsigned x2, unsigned x3) const;

	uint16_t suit0_ : 4; /* 4-13 */
	uint16_t suit1_ : 4; /* 0-6 */
	uint16_t suit2_ : 4; /* 0-4 */
	uint16_t equals_ : 4; /* two same: c, 6, 3, three same: e, 7 */
};

GenericPattern::GenericPattern() :
	suit0_(0),
	suit1_(0),
	suit2_(0),
	equals_(0)
{
}

unsigned GenericPattern::addShape(unsigned s, unsigned h, unsigned d)
{
	unsigned c = 13 - s - h - d;
	unsigned equal = 0;
	GenericPattern &cur = gpList[nrGP];

	cur.suit0_ = s;
	cur.suit1_ = h;
	cur.suit2_ = d;

	if (s == h)
		equal |= 0xc;
	if (h == d)
		equal |= 0x6;
	if (d == c)
		equal |= 0x3;

	cur.equals_ = equal;

	return nrGP++;
}

bool GenericPattern::isAllowedPattern(Pattern &p)
{
	p.sort();
	GenericPattern &cur = gpList[nrGP-1];

	if (cur.suit0_ < p.lengths[0])
		return false;
	if (cur.suit0_ > p.lengths[0])
		return true;

	if (cur.suit1_ < p.lengths[1])
		return false;
	if (cur.suit1_ > p.lengths[1])
		return true;

	if (cur.suit2_ < p.lengths[2])
		return false;
	return true;
}

unsigned GenericPattern::fromPattern( const Pattern &p)
{
	return 0;
}

void GenericPattern::print() const
{
	unsigned c = 13 - suit0_ - suit1_ - suit2_;
	unsigned bits = suit0_ << 12 |
		suit1_ << 8 |
		suit2_ << 4 |
		c;

	printf("%X %d", bits, popcount(equals_));
}

unsigned GenericPattern::maxPermutations() const
{
	switch(equals_) {
	case 3:
	case 6:
	case 12:
		return 12;
	case 14:
	case 7:
		return 4;
	default:
		return 24;
	}
}

void GenericPattern::permutateDouble(Pattern &p, unsigned permutation,
		unsigned x0, unsigned x1, unsigned x2, unsigned x3) const
{
	if (permutation & 0x1)
		std::swap(p.lengths[x2], p.lengths[x3]);

	permutation >>= 1;

	switch (permutation) {
	case 0:
	case 1:
	case 2:
		std::swap(p.lengths[x1], p.lengths[(x1+permutation) % 4]);
		break;
	case 3:
		std::swap(p.lengths[x1], p.lengths[x2]);
		std::swap(p.lengths[x0], p.lengths[x1]);
		break;
	case 4:
		std::swap(p.lengths[x1], p.lengths[x3]);
		std::swap(p.lengths[x0], p.lengths[x1]);
		break;
	case 5:
		std::swap(p.lengths[x1], p.lengths[x3]);
		std::swap(p.lengths[x0], p.lengths[x2]);
		break;
	}
}

Pattern GenericPattern::lengths(unsigned permutation) const
{
	unsigned c = 13 - suit0_ - suit1_ - suit2_;
	unsigned idxh, idxm;
	Pattern rv(suit0_, suit1_, suit2_, c);

	switch (equals_) {
	default:
		if (permutation & 0x1)
			std::swap(rv.lengths[2], rv.lengths[3]);

		permutation >>= 1;

		idxm = permutation % 3;
		idxh = permutation / 3;
		std::swap(rv.lengths[1], rv.lengths[1+idxm]);
		std::swap(rv.lengths[0], rv.lengths[0+idxh]);
		break;
	case 14:
		idxh = permutation;
		std::swap(rv.lengths[3], rv.lengths[3-idxh]);
		break;
	case 7:
		idxh = permutation;
		std::swap(rv.lengths[0], rv.lengths[0+idxh]);
		break;
	case 3:
		permutateDouble(rv, permutation, 2, 3, 0, 1);
		break;
	case 6:
		permutateDouble(rv, permutation, 1, 2, 0, 3);
		break;
	case 12:
		permutateDouble(rv, permutation, 0, 1, 2, 3);
		break;
	}

	return rv;
}

/** 4333, 4441, 7222, a111, d000 */

/**
 * 4432,
 * 5332, 5422, 5440, 5521, 5530,
 * 6322, 6331, 6511, 6610,
 * 7330, 7411, 7600,
 * 8221, 8311, 8500
 * 9211, 9400,
 * a300,
 * b110, b200,
 * c100,
 */

/**
 * 5431
 */
GenericPattern GenericPattern::gpList[39];
unsigned GenericPattern::nrGP;

/**
 */
struct SpecificPattern {

	SpecificPattern();
	SpecificPattern(unsigned id, unsigned permutation);

	const GenericPattern &ghp() const;
	unsigned id() const {return ghpidx_;}
	unsigned permutation() const;

	SpecificPattern &operator++();
	bool operator<(const SpecificPattern &b) const;
	bool operator!=(const SpecificPattern &b) const;

	operator unsigned() const;

	void print() const;
private:
	uint16_t ghpidx_ : 6; /* 0-37 */
	uint16_t permutation_ : 5; /* 0-3, 0-11, 0-23 */
};

SpecificPattern::SpecificPattern(unsigned id, unsigned permutation) :
	ghpidx_(id),
	permutation_(permutation)
{
}

SpecificPattern::SpecificPattern()
{
}

unsigned SpecificPattern::permutation() const
{
	return permutation_;
}

const GenericPattern &SpecificPattern::ghp() const
{
	return GenericPattern::gpList[ghpidx_];
}

SpecificPattern::operator unsigned int() const
{
	return permutation_;
}

SpecificPattern &SpecificPattern::operator++()
{
	permutation_++;
	return *this;
}

bool SpecificPattern::operator<(const SpecificPattern &b) const
{
	return ghpidx_ < b.ghpidx_;
}

bool SpecificPattern::operator!=(const SpecificPattern &b) const
{
	return ghpidx_ != b.ghpidx_;
}

void SpecificPattern::print() const
{
	const GenericPattern &gp = ghp();

	Pattern p = gp.lengths(permutation());

	printf("%X%X%X%X",
			p.lengths[0],
			p.lengths[1],
			p.lengths[2],
			p.lengths[3]
			);
}

template<class CountType, unsigned nrhands>
struct Combinations {
	Combinations() :
		count_(1),
		totals_{13,13,13,13},
		factor_(0)
	{
	}

	bool operator<(const Combinations &b) const;

	bool addPattern(unsigned nr, const SpecificPattern &sp);
	bool filterPattern(const SpecificPattern &sp) const;

	bool samePatterns() const;

	void print() const
	{
		unsigned h;
		for (h = 0; h < nrhands; h++) {
			sp_[h].print();
			putchar(' ');
		}
		if (nrhands == 3) {
			printf("%X%X%X%X ",
					totals_.lengths[0],
					totals_.lengths[1],
					totals_.lengths[2],
					totals_.lengths[3]);
		}
		const unsigned lshift = sizeof(count_) > 8 ? 64 : 31;
		if (sizeof(count_) > 8 && UINT64_MAX < count_)
			printf("%02u %" PRIx64"%016" PRIx64, factor_, (uint64_t)(count_ >> lshift), (uint64_t)count_);
		else
			printf("%02u %" PRIx64, factor_, (uint64_t)count_);
	}

	void factor() {factor_++; }
	void factor() const {const_cast<Combinations<CountType, nrhands>*>(this)->factor(); }

	void sort()
	{
		suitprint_.sort();
	}
private:
	CountType count_; /* Number of combinations */
	Pattern totals_;
	Pattern suitprint_;
	SpecificPattern sp_[nrhands == 3 ? 4 : nrhands]; /* Patterns for hands */
	unsigned factor_;
};

template<class CountType, unsigned nrhands>
bool Combinations<CountType, nrhands>::filterPattern(const SpecificPattern &sp) const
{
	Pattern p = sp.ghp().lengths(sp.permutation());
	unsigned i;
	for (i = 0; i < 4; i++) {
		if (totals_.lengths[i] < p.lengths[i])
			return true;
	}
	return false;
}

template<class CountType, unsigned nrhands>
bool Combinations<CountType, nrhands>::addPattern(unsigned nr,
		const SpecificPattern &sp)
{
	Pattern p = sp.ghp().lengths(sp.permutation());
	unsigned i;
	uint64_t comb = 1;
	Pattern totalsCopy(totals_);
	for (i = 0; i < 4; i++) {
		totalsCopy.lengths[i] -= p.lengths[i];
	}

	if (nr == 2 && !GenericPattern::isAllowedPattern(totalsCopy))
		return false;

	for (i = 0; i < 4; i++) {
		comb *= ncrgen(totals_.lengths[i], p.lengths[i]);
		totals_.lengths[i] -= p.lengths[i];
		if (nr == 2) {
			unsigned h4 = totals_.lengths[i];
			unsigned h3 = p.lengths[i];
			unsigned h2 = suitprint_.lengths[i] >> 4;
			unsigned h1 = suitprint_.lengths[i] & 0xf;

			if (h4 < h3)
				std::swap(h3, h4);
			if (h4 < h2)
				std::swap(h2, h4);
			if (h3 < h2) {
				std::swap(h2, h3);
				if (h2 < h1)
					std::swap(h1, h2);
			}

			suitprint_.lengths[i] = h4 << 12 |
				h3 << 8 |
				h2 << 4 |
				h1;
			continue;
		}
		if (nr > 0 && (suitprint_.lengths[i] & 0xf) < p.lengths[i]) {
			suitprint_.lengths[i] |= p.lengths[i] << 4;
		} else {
			suitprint_.lengths[i] = (suitprint_.lengths[i] & 0xf00) |
				(suitprint_.lengths[i] & 0xf) << 4 |
				p.lengths[i];
		}
	}

	if (nr == 2)
	printf("%X %X %X %X\n",
			suitprint_.lengths[0],
			suitprint_.lengths[1],
			suitprint_.lengths[2],
			suitprint_.lengths[3]);

	sp_[nr] = sp;
	if (nr == 2) {
		unsigned id = GenericPattern::fromPattern(totalsCopy);
		SpecificPattern newsp(id, 0);
		unsigned max = newsp.ghp().maxPermutations();
		for (; newsp < max; ++newsp) {
			if (std::equal(totals_.lengths, totals_.lengths + 4,
						newsp.ghp().lengths(newsp.permutation()).lengths))
				break;
		}
		sp_[3] = newsp;
		std::sort(sp_ + 1, sp_ + 4,
				[](const SpecificPattern &a, const SpecificPattern &b)
				{return a.id() > b.id();});
	}
	count_ *= comb;
	return true;
}

template<class CountType, unsigned nrhands>
bool Combinations<CountType, nrhands>::operator<(
		const Combinations<CountType, nrhands> &b) const
{
	if (sp_[0] < b.sp_[0])
		return true;
	if (sp_[0] != b.sp_[0] || nrhands == 1)
		return false;
	if (sp_[1] < b.sp_[1])
		return true;
	if (sp_[1] != b.sp_[1])
		return false;

	if (nrhands == 3) {
		if (sp_[2] < b.sp_[2])
			return true;
		if (sp_[2] != b.sp_[2])
			return false;
	}

	Pattern x(suitprint_);
	Pattern y(b.suitprint_);

	if (false && !(sp_[0] != SpecificPattern(6,0)) &&
		!(sp_[1] != SpecificPattern(6,0)))
	printf("%x %x %x %x < %x %x %x %x\n",
			x.lengths[0],
			x.lengths[1],
			x.lengths[2],
			x.lengths[3],
			y.lengths[0],
			y.lengths[1],
			y.lengths[2],
			y.lengths[3]
			);

	return x < y;
}

template<class CountType, unsigned nrhands>
bool Combinations<CountType, nrhands>::samePatterns() const
{
	return !(sp_[0] != sp_[1]);
}

/**
 * @param Type std::dequeue or std::set to store generated hands
 * @param nrhands The number of hands to include in the generation
 */
template<class Storage, unsigned nrhands, bool unique>
struct PatternGenerator {

	typedef typename Storage::value_type Entry;

	void operator()();

	void doGenericPatterns();

	template<bool first>
	void handleGenericPattern(Entry &entry, SpecificPattern sp,
			unsigned gpId, unsigned remainingHands)
	{
		unsigned max = unique && first ?
			1 : sp.ghp().maxPermutations();
		for (; (unsigned)sp < max; ++sp) {
			if (!first && entry.filterPattern(sp))
				continue;
			Entry entryNext(entry);
			doSpecificPattern(entryNext, gpId, remainingHands - 1, sp);
		}
	}

	void doSpecificPattern(Entry &entry,
			unsigned gpId, unsigned remainingHands,
			const SpecificPattern &sp) {
		unsigned i;
		if (!entry.addPattern(nrhands - remainingHands - 1, sp))
			return;

		if (remainingHands == 0) {
			entry.sort();
			auto pos = outList.insert(entry);
			pos.first->factor();
			if (nrhands == 2 && !entry.samePatterns())
				pos.first->factor();
			return;
		}
		for (i = 0; i <= gpId; i++) {
			SpecificPattern sp(i, 0);
			handleGenericPattern<false>(entry, sp, gpId, remainingHands);
		}
	}

	void print() const
	{
		for (const Entry &e : outList) {
			e.print();
			putchar('\n');
		}
	}

private:
	Storage outList;
};

template<class Storage, unsigned nrhands, bool unique>
void PatternGenerator<Storage, nrhands, unique>::doGenericPatterns()
{
	unsigned s, h, d;

	for (s = 4; s < 14; s++) {
		/* remaining cards divided by 3 and modulo divided by higher
		 * suits. */
		unsigned hmax = std::min(s, 13 - s);
		for (h = (13 - s + 2)/3; h <= hmax; h++) {
			unsigned dmax = std::min(h, 13 - s - h);
			for (d = (13 - s - h + 1)/2; d <= dmax; d++) {
				unsigned gpId = GenericPattern::addShape(s, h, d);

				Entry entry;
				SpecificPattern sp(gpId, 0);
				handleGenericPattern<true>(entry, sp, gpId, nrhands);
			}
		}
	}
}

template <class S, unsigned nr, bool uniq>
static int patternstatistics3()
{
	PatternGenerator<S, nr, uniq> gen;

	gen.doGenericPatterns();
	gen.print();
	return 0;
}

template <class S, unsigned nr>
static int patternstatistics2(bool uniq)
{
	if (uniq)
		return patternstatistics3<S, nr, true>();
	else
		return patternstatistics3<S, nr, false>();
}

template <unsigned nr>
static int patternstatistics1(bool uniq)
{
	if (nr < 3)
		return patternstatistics2<std::set<Combinations<uint64_t, nr> >, nr>(uniq);
	else
		return patternstatistics2<std::set<Combinations<int128, nr> >, nr>(uniq);
}

static int patternstatistics(int nrhands, int uniq, int sort)
{
	(void)nrhands;
	(void)uniq;
	(void)sort;

	switch (nrhands) {
		case 1:
			patternstatistics1<1>(uniq);
			break;
		case 2:
			patternstatistics1<2>(uniq);
			break;
		case 3:
			patternstatistics1<3>(uniq);
			break;
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
	(void)prog;
	(void)arg;
	if (arg[0] >= '1' && arg[0] <= '3') {
#if 0
		struct patternparam param;
		param.nrhands = arg[0] - '0';
		param.max[0] = param.max[1] = param.max[2] = param.max[3] = 13;
		param.complete = patternprintlookup;

		printf("const uint64_t parttern%dh[] = {\n\t", param.nrhands);
		patternrunloop(&param);
		puts("};");
#endif
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
