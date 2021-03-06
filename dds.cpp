
#include "dds.h"

#include <dll.h>

#include <cstdio>
#include <cstring>

#if !defined(_WIN32)
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#include "dealer.h"

namespace dds {

/// Function pointer to SolveBoard function in libdds
static decltype(SolveBoard)* ddSolveBoard = NULL;
/// Function pointer to helper converting libdds error codes to error text
static decltype(ErrorMessage)* ddErrorMessage = NULL;

/**
 * Convert dealer hand representation to libdds representation.
 */
static void cardsToDeal(struct deal *dl, const union board *d)
{
	unsigned h, s;
	for (h = 0; h < 4; h++) {
		// libdds format has bit indexes matching to rank while our format has
		// rank 2 at bit index 0
		dl->remainCards[h][3] = (d->hands[h] << 2) & 0xFFFF;
		for (s = 1; s < 4; s++)
			dl->remainCards[h][3-s] = (d->hands[h] >> (SUIT_WIDTH*s - 2)) & 0xFFFF;
	}
}

/**
 * Run double dummy analyse for a board and declarer. Implementation for
 * tricks() function in scripts.
 */
static int Solve(const union board *d, int declarer, int contract)
{
	int r;
	// Initialize deal structure requesting solution
	struct deal dl = {
		contract < 4 ? 3 - contract : 4,
		(declarer + 1) % 4,
		{0,0,0},
		{0,0,0},
		{{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}},
	};
	// Find the best possible trick count
	int target = -1;
	// We want only the best solution
	int solutions = 1;
	// No reusing tables
	int mode = 1;
	// We only have one thread
	int thread = 0;
	// Initialize result structure
	struct futureTricks futp;
	std::memset(&futp, 0, sizeof(futp));
	// Copy hands from our format to libdds format in deal structure
	cardsToDeal(&dl, d);
	// Do the double dummy analyze
	r = ddSolveBoard(dl, target, solutions, mode, &futp, thread);
	if (r != RETURN_NO_FAULT) {
		char line[80];
		// Report the error in human readable format
		ddErrorMessage(r, line);
		std::fprintf(stderr, "DD error: %s\n", line);
		return 0;
	}
	// Double dummy analyze solves tricks for player on lead. Return value is
	// expected to tell tricks for declarer so we need to convert the trick
	// count for opposite side.
	return 13 - futp.score[0];
}

/**
 * Solve double dummy results for all potential leads. Implementation for
 * leadtricks() function in scripts.
 */
static void SolveLead(const union board *d, int declarer, int contract, hand cards, char res[13])
{
	int r;
	struct deal dl = {
		contract < 4 ? 3 - contract : 4,
		(declarer + 1) % 4,
		{0,0,0},
		{0,0,0},
		{{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0},
		{0,0,0,0}},
	};
	// Search for the best trick count
	int target = -1;
	// Return solutions for all playable cards
	int solutions = 3;
	// Avoid reusing cached results
	int mode = 1;
	// We have only one thread
	int thread = 0;
	struct futureTricks futp;
	std::memset(&futp, 0, sizeof(futp));
	// Convert our format to libdds format
	cardsToDeal(&dl, d);
	r = ddSolveBoard(dl, target, solutions, mode, &futp, thread);
	if (r != RETURN_NO_FAULT) {
		char line[80];
		ddErrorMessage(r, line);
		std::fprintf(stderr, "DD error: %s\n", line);
		return;
	}
	// Convert results to tricks possible by declarer and store them in result
	// array.
	for (target = 0; target < futp.cards; target++) {
		card t = MAKECARD(3 - futp.suit[target], futp.rank[target] - 2);
		if ((t & cards) == 0)
			continue;
		card mask = 1LL << (SUIT_WIDTH*3+CARDS_IN_SUIT);
		mask -= t << 1;
		unsigned pos = hand_count_cards(cards & mask);
		res[pos] = 13 - futp.score[target];
	}
}

/**
 * Loads libdds library dynamically and links SoveBoard to the function pointer.
 *
 * Not thread safe.
 */
static void loadLib()
{
#if !defined(_WIN32)
#ifndef __APPLE__
#define LIBNAME "libdds.so.2"
#else
#define LIBNAME "libdds.2.dylib"
#endif
	void *handle = dlopen(LIBNAME, RTLD_LAZY);
#else
#if UNICODE
#define LIBNAME L"libdds.dll"
#else
#define LIBNAME "libdds.dll"
#endif
#define dlsym GetProcAddress
	HMODULE handle = LoadLibrary(LIBNAME);
#endif
	if (handle) {
		decltype(SetMaxThreads)* ddSetMaxThreads = NULL;
		ddSetMaxThreads = (decltype(SetMaxThreads) *)dlsym(handle, "SetMaxThreads");
		ddSolveBoard = (decltype(SolveBoard) *)dlsym(handle, "SolveBoard");
		ddErrorMessage = (decltype(ErrorMessage) *)dlsym(handle, "ErrorMessage");
		ddSetMaxThreads(1);
		solve = Solve;
		solveLead = SolveLead;
	} else {
#if !_WIN32
		fprintf(stderr, "%s\n", dlerror());
#endif
		error("Error: No libdds library found. DD support disabled.\n");
	}
}

/**
 * Make sure library is loaded and assigned to solve pointer. Then forward call
 * to the call handling function.
 */
static int loadSolve(const union board *d, int declarer, int contract)
{
	loadLib();
	return solve(d, declarer, contract);
}

static void loadSolveLead(const union board *d, int declarer, int contract, hand cards, char res[13])
{
	loadLib();
	solveLead(d, declarer, contract, cards, res);
}

}

int (*solve)(const union board *d, int declarer, int contract) = dds::loadSolve;
void (*solveLead)(const union board *d, int declarer, int contract, hand cards, char res[13]) = dds::loadSolveLead;
