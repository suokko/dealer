
#pragma once

#include "card.h"
#include "pointcount.h"

#include "entry.h"

#include <stdio.h>

extern const char* const player_name[4];
extern const char* const suit_name[5];

struct handstat {
    int hs_points[(NSUITS + 1)*2];  /* 4321 HCP per suit or total */
    int hs_bits[2];                 /* Distribution bit position */
    int hs_loser[(NSUITS + 1)*2];   /* Losers in a suit */
    int hs_control[(NSUITS + 1)*2]; /* Controls in a suit or total */
} ;

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

enum { STAT_MODE, EXHAUST_MODE };

extern const struct globals *gptr;

[[noreturn]] void  yyerror (const char*);
extern "C" int yywrap ();
int yyparse (void);

[[gnu::pure]] int imps (int scorediff);
int scoreone (int vuln, int suit, int level, int dbl, int tricks);
[[noreturn]] void error (const char* s);
void  setshapebit (struct shape *, int, int, int, int);
void  predeal (int, card);
void clearpointcount ();
void clearpointcount_alt (int cin);
void pointcount (int index, int value);
void newpack (union pack *d, const char *initialpack);
[[gnu::pure]] card make_card (char rankchar, char suitchar);
[[gnu::pure]] int make_contract (char suitchar, char trickchar, char dbl);
void fprintcompact (FILE * f, const union board *d, int ononeline, int disablecompass);
void printdeal (const union board *d);
void printhands (int boardno, const union board *dealp, int player, int nhands);
void printew (const union board *d);
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


