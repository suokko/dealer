#if __WIN32
#if __GNUC__
#define WINVER 0x0600
#endif
#include <winnls.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "bittwiddle.h"

#include <random>
#include <regex>

char* input_file = 0;

#if defined(_MSC_VER) || defined(__WIN32) || defined(WIN32)
  /* with VC++6, winsock2 declares ntohs and struct timeval */
#ifdef _MSC_VER
  #pragma warning (disable : 4115)
#endif
  #include <winsock2.h>
  #include <fcntl.h>
#ifdef _MSC_VER
  #pragma warning (default : 4115)
#endif
#else
  /* else we assume we can get ntohs/ntohl from netinet */
  #include <netinet/in.h>
#endif /* _MSC_VER */

#if defined(WIN32) || defined(win32)
  #ifdef _MSC_VER
  struct timeval {
    long tv_sec;            /* seconds */
    long tv_usec;            /* and microseconds */
  };
  #pragma warning (disable : 4100)
  #endif /* _MSC_VER */
  void gettimeofday (struct timeval *tv, void *vp) {
    (void)vp;
    tv->tv_sec = time (0);
    tv->tv_usec = 0;
  }
  #ifdef _MSC_VER
  #pragma warning (default : 4100)
#endif
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif /* WIN32 */

#include <getopt.h>

#include "dealer.h"
#include "tree.h"
#include "cpudetect/detect.h"

#define DEFAULT_MODE STAT_MODE

#ifdef MSDOS
  const char * const crlf = "\r\n";
#else
  const char * const crlf = "\n";
#endif /* MSDOS */

/* Global variables */


const char ucrep[14] = "23456789TJQKA";
const char *utf8_ucsep[]  = {u8"♣",u8"♦",u8"♥",u8"♠"};
const char *noutf8_ucsep[] = {"C","D","H","S"};
const char **ucsep = utf8_ucsep;


static const int imparr[24] = { 10,   40,   80,  120,  160,  210,  260,  310,  360,
                  410,  490,  590,  740,  890, 1090, 1190, 1490, 1740,
                 1990, 2240, 2490, 2990, 3490, 3990};

const char* const suit_name[] = {"Club", "Diamond", "Heart", "Spade", "Notrump"};


const char * const player_name[] = { "North", "East", "South", "West" };

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/
#define MAXDISTR 8*sizeof(int)

static struct tree defaulttree = {{TRT_NUMBER}, NIL, NIL, 1, 0, 0};
static struct action defaultaction = {(struct action *) 0, ACT_PRINTALL, NULL, NULL, 0, 0, {{0}}};

struct globals *gp;
const struct globals *gptr;

/* Function definitions */
int imps (int scorediff) {
  int i, j;
  j = abs (scorediff);
  for (i = 0; i < 24; i++)
    if (imparr[i] >= j) return scorediff < 0 ? -i : i;
  return scorediff < 0 ? -i : i;
}

int scoreone (int vuln, int suit, int level, int dbl, int tricks) {
  int total = 0;

  /* going down */
  if (tricks < 6 + level) {
    if (!dbl)
      return -50 * (1 + vuln) * (6 + level - tricks);
    if (vuln) {
      return -200 - 300 * (5 + level - tricks);
    } else {
      switch (6 + level - tricks) {
        case 1:
          return -100;
        case 2:
          return -300;
        default:
          return -500 - 300 * (3 + level - tricks);
      }
    }
  }

  int trickpts = ((suit >= SUIT_HEART) ? 30 : 20);
  int contractpts = level * trickpts;
  if (suit == SUIT_NT) contractpts += 10;

  if (dbl) contractpts *= 2;
  /* Contracted trick points */
  total = contractpts;
  /* part score bonus */
  total += 50;

  if (dbl) total += 50;

  /* game bonus */
  if (contractpts >= 100) total += 250 + 200 * vuln;

  /* small slam bonus */
  if (level == 6) total += 500 + 250 * vuln;

  /* grand slam bonus */
  if (level == 7) total += 1000 + 500 * vuln;

  /* Over tricks */
  if (dbl)
    total += 100 * (1 + vuln) * (tricks - 6 - level);
  else
    total += trickpts * (tricks - 6 - level);

  return total;
}

void error (const char* s) {
  fprintf (stderr, "%s%s", s, crlf);
  exit (10);
}

/* implementations of regular & alternate pointcounts */

static int countindex = -1;

static void zerocount (int points[13]) {
  int i;
  for (i = 12; i >= 0; i--) points[i] = 0;
}

void clearpointcount () {
  zerocount (tblPointcount[idxHcp]);
  countindex = idxHcp;
}

void clearpointcount_alt (int cin) {
  if (cin + idxBase >= idxEnd || cin < 0)
    yyerror("Alternative point count out of range");
  zerocount (tblPointcount[cin + idxBase]);
  countindex = cin + idxBase;
}

void pointcount (int index, int value) {
  assert (index <= 12);
  if (index < 0) {
      yyerror ("too many pointcount values");
  }
  tblPointcount[countindex][index] = value;
}

void * mycalloc (unsigned nel, unsigned siz) {
  void *p = calloc (nel, siz);
  if (p) return p;
  fprintf (stderr, "Out of memory\n");
  exit (-1); /*NOTREACHED */
}

static void initdistr (struct globals *g) {
  unsigned clubs;

  int shape = 0;

  for (clubs = 0; clubs <= 13; clubs++) {
    unsigned max = 14 - clubs;
    unsigned min = 0;
    unsigned toadd = ((max+min)*(max-min+1))/2;
    g->distrbitmaps[clubs] = shape;
    shape += toadd;
  }
}

void  setshapebit (struct shape *s, int cl, int di, int ht, int sp)
{
  (void)sp;
  unsigned nr = getshapenumber(cl, di, ht);
  int idx = nr / 32;
  int bit = nr % 32;
  s->bits[idx] |= 1 << bit;
}

struct rng {
  FILE *f;
  unsigned char random[70];
  unsigned idx;
};

card make_card (char rankchar, char suitchar);

void newpack (union pack *d, const char *initialpack) {
  int suit, rank, place;

  place = 0;
  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
    for (rank = 0; rank < 13; rank++)
      d->c[place++] = MAKECARD (suit, rank);

  /* If user wants random order initial pack do a single shuffle using
   * user values or system random number generator.
   */
  if (initialpack) {

    if (strcmp(initialpack, "rng") == 0) {
      std::random_device rd;

      for (place = 51; place > 0; place--) {
        std::uniform_int_distribution<> dist(0, place);
        int pos = dist(rd);
        std::swap(d->c[pos], d->c[place]);
      }
    } else {

      // It is important to remember that multibyte characters are actual
      // strings when compiling regular expression.
      std::regex parser(
        u8"^(?:([Cc]|♣|♧)|([Dd]|♦|♢)|([Hh]|♥|♡)|([Ss]|♠|♤))([2-9tTjJqQkKaA])",
        std::regex::ECMAScript | std::regex::optimize);

      const char *begin = initialpack;
      const char *end = initialpack + strlen(initialpack);
      std::cmatch match;
      hand allcards{0};

      for (place = 0; place < 52 && std::regex_search(begin, end, match, parser); ++place) {
        if (match[1].matched)
          suit = SUIT_CLUB;
        else if (match[2].matched)
          suit = SUIT_DIAMOND;
        else if (match[3].matched)
          suit = SUIT_HEART;
        else if (match[4].matched)
          suit = SUIT_SPADE;
        else
          assert(0 && "Regular expression matched but none of required suit subexpressions match");
        assert(match[5].matched && "Rank subexpression doesn't hold match like expected");
        char rc = match[5].first[0];
        switch (rc) {
        default:
          assert(0 && "Unknown character for card rank");
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          rank = rc - '2';
          break;
        case 't':
        case 'T':
          rank = 10 - 2;
          break;
        case 'j':
        case 'J':
          rank = 11 - 2;
          break;
        case 'q':
        case 'Q':
          rank = 12 - 2;
          break;
        case 'k':
        case 'K':
          rank = 13 - 2;
          break;
        case 'a':
        case 'A':
          rank = 14 - 2;
          break;
        }

        d->c[place] = MAKECARD(suit, rank);
        if (hand_has_card(allcards, d->c[place]))
          error("Same card provide twice for the initial pack order");

        allcards = hand_add_card(allcards, d->c[place]);

        begin = match.suffix().first;
      }
      if (begin != end && place < 52) {
        std::regex suit_regex(u8"^(?:[CcDdHhSs]|♣|♧|♦|♢|♥|♡|♠|♤)");
        if (!std::regex_search(begin, end, suit_regex))
          error("Unknown suit symbol for the initial pack order");
        else
          error("Unknown rank symbol for the initial pack order");
      }
      if (place < 52)
        error("Need 52 cards for the initial pack order");
      if (begin != end)
        error("More than 52 cards provided for the initial pack order");
    }

    if (!gptr->quiet) {
      char statetext[52*5+1];
      char *iter = statetext;
      for (place = 0; place < 52; ++place) {
        card t = d->c[place];
        iter += sprintf(iter, "%s%c", ucsep[C_SUIT(t)], ucrep[C_RANK(t)]);
      }
      printf("Initial pack order: %s\n", statetext);
    }

    free((char *)initialpack);
  }
}

card make_card (char rankchar, char suitchar) {
  int rank, suit = 0;

  for (rank = 0; rank < 13 && ucrep[rank] != rankchar; rank++) ;
  assert (rank < 13);
  switch (suitchar) {
    case 'C':
      suit = 0;
      break;
    case 'D':
      suit = 1;
      break;
    case 'H':
      suit = 2;
      break;
    case 'S':
      suit = 3;
      break;
    default:
      assert (0);
    }
  return MAKECARD (suit, rank);
}

int make_contract (char suitchar, char trickchar, char dbl) {
  int trick, suit;
  trick = (int) trickchar - '0';
  switch (suitchar) {
    case 'C':
      suit = 0;
      break;
    case 'D':
      suit = 1;
      break;
    case 'H':
      suit = 2;
      break;
    case 'S':
      suit = 3;
      break;
    case 'N':
      suit = 4;
      break;
    default:
      suit = 0;
      printf ("%c", suitchar);
      assert (0);
  }
  return MAKECONTRACT (suit, trick) + (dbl == 'x' ? 64 : 0);
}

void fprintcompact (FILE * f, const union board *d, int ononeline, int disablecompass) {
  char pt[] = "nesw";
  int s, p, r;
  for (p = COMPASS_NORTH; p <= COMPASS_WEST; p++) {
    if (!disablecompass) fprintf (f, "%c ", pt[p]);
    for (s = SUIT_SPADE; s >= SUIT_CLUB; s--) {
      for (r = 12; r >= 0; r--)
        if (HAS_CARD (d, p, MAKECARD (s, r)))
          fprintf (f, "%c", ucrep[r]);
      if (s > 0) fprintf (f, ".");
    }
    /* OK to use \n as this is mainly intended for internal dealer use. */
    fprintf (f, ononeline && p != COMPASS_WEST ? " " : "\n");
  }
}

void printdeal (const union board *d) {
  int suit, player, rank, cards;

  printf ("%4d.\n", (gptr->nprod+1));

  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
      while (cards < 10) {
        printf ("  ");
        cards++;
      }
      cards = 0;
      for (rank = 12; rank >= 0; rank--) {
        if (HAS_CARD (d, player, MAKECARD (suit, rank))) {
          printf ("%c ", ucrep[rank]);
          cards++;
        }
      }
      if (cards == 0) {
        printf ("- ");
        cards++;
      }
    }
    printf ("\n");
  }
  printf ("\n");
}

void printcard(card c) {
  assert(hand_count_cards(c) == 1);
  printf("%s%c", ucsep[C_SUIT(c)], ucrep[C_RANK(c)]);
}

void predeal (int player, card onecard) {

  int i;
  card c = 0;
  for (i = 0; i < 4; i++) {
    c = hand_has_card(gp->predealt.hands[i], onecard);
    if (c == onecard)
      yyerror ("Card predealt twice");
  }
  gp->predealt.hands[player] = hand_add_card(gp->predealt.hands[player], onecard);
  if (hand_count_cards(gp->predealt.hands[player]) > 13)
    yyerror("More than 13 cards for one player");
}

void printhands (int boardno, const union board *dealp, int player, int nhands) {
  int i, suit, rank, cards;

  for (i = 0; i < nhands; i++)
    printf ("%4d.%15c", boardno + i + 1, ' ');
  printf ("\n");
  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (i = 0; i < nhands; i++) {
      while (cards < 10) {
        printf ("  ");
        cards++;
      }
      cards = 0;
      for (rank = 12; rank >= 0; rank--) {
        if (HAS_CARD (&dealp[i], player, MAKECARD (suit, rank))) {
          printf ("%c ", ucrep[rank]);
          cards++;
        }
      }
      if (cards == 0) {
        printf ("- ");
        cards++;
      }
    }
    printf ("\n");
  }
  printf ("\n");
}

int yywrap () {
  /* Necessary if you do not have a -ll library */
  return 1;
}

void printew (const union board*d) {
  /* This function prints the east and west hands only (with west to the
     left of east), primarily intended for examples of auctions with 2
     players only.  HU.  */
  int suit, player, rank, cards;

  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (player = COMPASS_WEST; player >= COMPASS_EAST; player--) {
      if (player != COMPASS_SOUTH) {
        while (cards < 10) {
          printf ("  ");
          cards++;
        }
        cards = 0;
        for (rank = 12; rank >= 0; rank--) {
          if (HAS_CARD (d, player, MAKECARD (suit, rank))) {
            printf ("%c ", ucrep[rank]);
            cards++;
          }
        }
        if (cards == 0) {
          printf ("- ");
          cards++;
        }
      }
   }
   printf ("\n");
  }
  printf ("\n");
}

void showevalcontract (int nh) {
  int s, l, i, v;
  for (v = 0; v < 2; v++) {
    printf ("%sVulnerable%s", v ? "" : "Not ", crlf);
    printf ("   ");
    for (l = 1; l < 8; l++) printf ("  %d       ", l);
    printf ("%s", crlf);
    for (s = 0; s < 5; s++) {
      printf ("%c: ", "cdhsn"[s]);
      for (l = 1; l < 8; l++) {
        int t = 0, tn = 0;
        for (i = 0; i < 14; i++) {
          t += gptr->results[0][s][i] * scoreone (v, s, l, 0, i);
          tn += gptr->results[1][s][i] * scoreone (v, s, l, 0, i);
        }
        printf ("%4d/%4d ", t / nh, tn / nh);
      }
      printf ("%s", crlf);
    }
    printf ("%s", crlf);
  }
}

static void initglobals(struct globals *g)
{
  gptr = g;
  gp = g;
  memset(g->biasdeal, -1, sizeof(g->biasdeal));
  g->curboard.hands[0] = club_mask;
  g->curboard.hands[1] = diamond_mask;
  g->curboard.hands[2] = heart_mask;
  g->curboard.hands[3] = spade_mask;
  g->decisiontree = &defaulttree.base;
  g->actionlist = &defaultaction;
  g->verbose = 1;
  g->maxdealer = -1;
}

static const char *default_locale = "";

int main (int argc, char **argv) {
  int seed_provided = 0;
  extern int optind;
  extern char *optarg;
  int c;
  int errflg = 0;
  bool set_locale = true;

  struct timeval tvstart, tvstop;

  struct globals global = {.seed = 0};

  initglobals(&global);


  gettimeofday (&tvstart, nullptr);

  while ((c = getopt (argc, argv, "023ehvmqp:g:s:l:Vi:C")) != -1) {
    switch (c) {
      case '0':
      case '2':
      case '3':
        gp->swapping = c - '0';
        break;
      case 'e':
        gp->computing_mode = EXHAUST_MODE;
        break;
      case 'l':
        gp->loading = 1;
        gp->loadindex = atoi (optarg);
        break;
      case 'g':
        gp->maxgenerate = atoi (optarg);
        break;
      case 'm':
        gp->progressmeter ^= 1;
        break;
      case 'p':
        gp->maxproduce = atoi (optarg);
        break;
      case 'i':
        /* 96bit number translated to initial pack order or rng for secure
         * random number from system.
         */
        gp->initialpack = strdup(optarg);
        break;
      case 's':
        seed_provided = 1;
        gp->seed = atol (optarg);
        if (gp->seed == LONG_MIN || gp->seed == LONG_MAX) {
            fprintf (stderr, "Seed overflow: seed must be between %ld and %ld\n",
              LONG_MIN, LONG_MAX);
            exit (-1);
        }
        break;
      case 'v':
        gp->verbose ^= 1;
        break;
      case 'q':
        gp->quiet ^= 1;
        break;
      case 'V':
        printf ("Version info....\n");
        printf ("$Revision: 1.24 $\n");
        printf ("$Date: 2003/08/05 19:53:04 $\n");
        printf ("$Author: henk $\n");
        return 1;
      case 'C':
        set_locale = false;
        break;
      case '?':
      case 'h':
        errflg = 1;
        break;
      }
    }
  if (argc - optind > 2 || errflg) {
    fprintf (stderr, "Usage: %s [-emv] [-s seed] [-i rng|52 cards] [-p num] [-v num] [inputfile]\n", argv[0]);
    exit (-1);
  }
  if (optind < argc && freopen (input_file = argv[optind], "r", stdin) == NULL) {
    perror (argv[optind]);
    exit (-1);
  }

  // User user preferred locale
  if (set_locale) {
    try {
      std::locale::global(std::locale(default_locale));
    } catch(...) {
      ucsep = noutf8_ucsep;
    }
  } else {
    ucsep = noutf8_ucsep;
  }
  initdistr (gp);
  gp->maxvuln = -1;

  yyparse ();

  /* The most suspect part of this program */
  if (!seed_provided) {
    time_t seed;
    (void) time (&seed);
    gp->seed = seed;
  }

  if (gp->maxgenerate == 0)
    gp->maxgenerate = 10000000;
  if (gp->maxproduce == 0)
    gp->maxproduce = ((gp->actionlist == &defaultaction) || gp->will_print) ? 40 : gp->maxgenerate;

  int r = deal_main(gp);

  gettimeofday (&tvstop, nullptr);
  if (gp->verbose) {
    printf ("Generated %d hands\n", gp->ngen);
    printf ("Produced %d hands\n", gp->nprod);
    printf ("Initial random seed %zu\n", gp->seed);
    printf ("Time needed %8.3f sec%s",
             (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 -
             (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)), crlf);
  }
  return r;
}

// Windows wrapper main to convert input
#if UNICODE
int wmain(int argc, wchar_t** wargv)
{
  int i;
  unsigned size = 0;

  // Count output bytes
  for (i = 0; i < argc; ++i)
    size += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);

  // Convert arguments
  char buffer[size];
  char* iter = buffer;
  char* argv[argc+1];

  for (i = 0; i < argc; ++i) {
    int res = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, iter, size, NULL, NULL);
    argv[i] = iter;
    iter += res;
    size -= res;
  }
  argv[argc] = NULL;

  // Check if output supports utf-8
  unsigned cp = GetConsoleOutputCP();
  if (cp != CP_UTF8) {
    // Disable utf-8 output
    ucsep = noutf8_ucsep;
  }

  // Get user default locale
  size = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SNAME, nullptr, 0);
  char locale[size];
  GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SNAME, locale, size);
  default_locale = locale;
  return main(argc, argv);
}
#endif
