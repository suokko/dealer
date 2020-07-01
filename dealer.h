
#pragma once

#include "card.h"
#include "pointcount.h"

#include "entry.h"

#include <stdio.h>

/**
 * Convert compass direction a human readable string
 */
extern const char* const player_name[4];
/**
 * Convert suits and contracts to human readable string
 */
extern const char* const suit_name[5];

/**
 * Cache for hand statistics
 * @TODO should be removed and implemented using implicit tree variables
 */
struct handstat {
    int hs_points[(NSUITS + 1)*2];  /* 4321 HCP per suit or total */
    int hs_bits[2];                 /* Distribution bit position */
    int hs_loser[(NSUITS + 1)*2];   /* Losers in a suit */
    int hs_control[(NSUITS + 1)*2]; /* Controls in a suit or total */
} ;

/**
 * Command line argument and script requested runtime state
 * @TODO should be constant after initialization
 */
struct globals {
    time_t seed;
    const char *initialpack;
    int maxgenerate;
    int maxproduce;
    int nprod;
    int ngen;
    int biastotal;
    int loadindex;
    int maxdealer;
    int maxvuln;

    char biasdeal[4][4];
    int results[2][5][14];
    struct stored_board *deallist;
    double average;

    union board predealt;
    union board curboard;
    uint16_t libtricks[5];

    struct treebase *decisiontree;
    struct action *actionlist;

    unsigned quiet          : 1;
    unsigned verbose        : 1;
    unsigned progressmeter  : 1;
    unsigned will_print     : 1;
    unsigned swapping       : 3;
    unsigned loading        : 1;
    unsigned use_compass    : 4;
    unsigned computing_mode : 2;
};

/**
 * STAT_MODE generates random hands. EXHAUST_MODE generates all possible hands
 * remaining hands when two hands are completely defined with predeal.
 */
enum { STAT_MODE, EXHAUST_MODE };

/// @TODO remove this
extern const struct globals *gptr;

/// Error reporting from the parser
[[noreturn]] void  yyerror (const char*);
/// Required parser callback
extern "C" int yywrap ();
/// Run the script parser
int yyparse (void);

/// Calculate imp based on score difference
[[gnu::pure]] int imps (int scorediff);
/// Calculate a score based vulnerability, contract and tricks
int scoreone (int vuln, int suit, int level, int dbl, int tricks);
/// Report a runtime error
[[noreturn]] void error (const char* s);
/// Helper to build bit mask for a shape function in scripts
void  setshapebit (struct shape *, int, int, int, int);
/// Make a card always appear in a hand
void  predeal (int, card);
/// Clear the HCP calculation state
void clearpointcount ();
/// Clear the cin point calculation state
void clearpointcount_alt (int cin);
/// Define a point value for a rank
void pointcount (int index, int value);
/// create an initial pack based on command line arguments
void newpack (union pack *d, const char *initialpack);
/// Set a card bit mask based on supplied rank and suit character
[[gnu::pure]] card make_card (char rankchar, char suitchar);
/// Make contract based denomination, level and double
[[gnu::pure]] int make_contract (char suitchar, char trickchar, char dbl);
/// Print a deal in a compact format
void fprintcompact (FILE * f, const union board *d, int ononeline, int disablecompass);
/// Print a deal in multi-line format
void printdeal (const union board *d);
/// Print nhands from dealp array for player starting from boardno. Support at
/// most 4 deals per call.
void printhands (int boardno, const union board *dealp, int player, int nhands);
/// Print east and west
void printew (const union board *d);
/// Print expected score for each level for evalcontract action
void showevalcontract (int nh);

struct value_array {
  int key[13];
  int value[13];
};

void * mycalloc (unsigned nel, unsigned siz);

extern const char ucrep[14];
extern const char **ucsep;
extern const char *const crlf;

#include "card.h"

#define printcompact(d) (fprintcompact(stdout, d, 0, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1, 0))

void printcard(card c);

#define HAS_CARD(d,p,c) hand_has_card((d)->hands[p], c)


extern entry<int (*)(struct globals *g)> deal_main;


namespace DEFUN() {
int suitlength (const union board* d, int compass,int suit);
}


