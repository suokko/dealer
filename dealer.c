#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

long seed = 0;
static int quiet = 0;
char* input_file = 0;

#ifdef _MSC_VER
  /* with VC++6, winsock2 declares ntohs and struct timeval */
  #pragma warning (disable : 4115)
  #include <winsock2.h>
  #pragma warning (default : 4115)
#else
  /* else we assume we can get ntohs/ntohl from netinet */
  #include <netinet/in.h>
#endif /* _MSC_VER */

#ifdef WIN32
  #ifndef _MSC_VER
  struct timeval {
    long tv_sec;            /* seconds */
    long tv_usec;            /* and microseconds */
  };
  #endif /* _MSC_VER */
  #pragma warning (disable : 4100)
  void gettimeofday (struct timeval *tv, void *pv) {
    tv->tv_sec = time (0);
    tv->tv_usec = 0;
  }
  #pragma warning (default : 4100)
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif /* WIN32 */

#include <getopt.h>

#include "tree.h"
#include "pointcount.h"
#include "dealer.h"
#include "c4.h"
#include "pbn.h"
#include "genlib.h"

void yyerror (char *);

#define TWO_TO_THE_13 (1<<13)
#define DEFAULT_MODE STAT_MODE
#define RANDBITS 16
#define NRANDVALS (1<<RANDBITS)
#define NRANDMASK (NRANDVALS-1)

#ifdef MSDOS
  char *crlf = "\r\n";
#else
  char *crlf = "\n";
#endif /* MSDOS */

/* Global variables */

enum { STAT_MODE, EXHAUST_MODE };
static int computing_mode = DEFAULT_MODE;

static char ucrep[14] = "23456789TJQKA";

static int biastotal = 0;
int biasdeal[4][4] = { {-1, -1, -1, -1}, {-1, -1, -1, -1},
                       {-1, -1, -1, -1}, {-1, -1, -1, -1}};
static int predealt[5][4] = { {0, 0, 0, 0}, {0, 0, 0, 0},
                       {0, 0, 0, 0}, {0, 0, 0, 0},
                       {0, 0, 0, 0}};

static int imparr[24] = { 10,   40,   80,  120,  160,  210,  260,  310,  360,
                  410,  490,  590,  740,  890, 1090, 1190, 1490, 1740,
                 1990, 2240, 2490, 2990, 3490, 3990};

static const char * const suit_name[] = {"Club", "Diamond", "Heart", "Spade"};
const char * const player_name[] = { "North", "East", "South", "West" };
static deal fullpack;
static deal stacked_pack;

static int swapping = 0;
static int swapindex = 0;
static int loading = 0;
static int loadindex = 0;
static double average = 0.0;

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/
#define MAXDISTR 8*sizeof(int)
static int **distrbitmaps[14];

static int results[2][5][14];
int use_compass[NSUITS];
int use_vulnerable[NSUITS];
static int nprod;
int maxproduce;
static int ngen;

static struct tree defaulttree = {{TRT_NUMBER}, NIL, NIL, 1, 0};
struct treebase *decisiontree = &defaulttree.base;
static struct action defaultaction = {(struct action *) 0, ACT_PRINTALL};
struct action *actionlist = &defaultaction;
static unsigned char zero52[NRANDVALS];
static deal *deallist;

/* Function definitions */
static void fprintcompact (FILE *, deal, int, int);
static void error (char *);
static void printew (deal d);
void yyparse ();
static int true_dd (deal d, int l, int c); /* prototype */

#ifdef FRANCOIS
  /* Special variables for exhaustive mode 
     Exhaustive mode created by Francois DELLACHERIE, 01-1999  */
  int vectordeal;
  /* a vectordeal is a binary *word* of at most 26 bits: there are at most
     26 cards to deal between two players.  Each card to deal will be
     affected to a given position in the vector.  Example : we need to deal
     the 26 minor cards between north and south (what a double fit !!!)
     Then those cards will be represented in a binary vector as shown below:
          Card    : DA DK ... D2 CA CK ... C4 C3 C2
          Bit-Pos : 25 24     13 12 11      2  1  0  
     The two players will be respectively represented by the bit values 0 and
     1. Let's say north gets the 0, and south the 1. Then the vector
     11001100000001111111110000 represents the following hands :
          North: D QJ8765432 C 5432
          South: D AKT9      C AKQJT9876
     A suitable vectordeal is a vector deal with hamming weight equal to 13. 
     For computing those vectors, we use a straightforward
     meet in the middle approach.  */
  int exh_predealt_vector = 0;
  char exh_card_map[256];
  /* exh_card_map[card] is the coordinate of the card in the vector */
  card exh_card_at_bit[26];
  /* exh_card_at_bit[i] is the card pointed by the bit #i */
  unsigned char exh_player[2];
  /* the two players that have unknown cards */
  unsigned char exh_empty_slots[2];
  /* the number of empty slots of those players */
  unsigned char exh_suit_length_at_map[NSUITS][26];
  /* exh_suit_length_at_map[SUIT][POS] = 1 if the card at 
     position POS is from the suit SUIT */
  unsigned char exh_suit_points_at_map[NSUITS][26];
  /* same as above with the hcp-value instead of 1 */
  unsigned char exh_msb_suit_length[NSUITS][TWO_TO_THE_13];
  /* we split the the vector deal into 2 sub-vectors of length 13. Example for
     the previous vector: 1100110000000 and 11111111110000
                          msb               lsb
     exh_msb_suit_length[SUIT][MSB_VECTOR] represents the msb-contribution of
     the MSB_VECTOR for the length of the hand for suit SUIT, and for player 0. 
     In the trivial above example, we have: exh_msb_suit_length[SUIT][MSBs] = 0
     unless SUIT == DIAMOND, = the number of 0 in MSB, otherwise.*/
  unsigned char exh_lsb_suit_length[NSUITS][TWO_TO_THE_13];
   /* same as above for the lsb-sub-vector */
  unsigned char exh_msb_suit_points[NSUITS][TWO_TO_THE_13];
   /* same as above for the hcp's in the suits */
  unsigned char exh_lsb_suit_points[NSUITS][TWO_TO_THE_13];
   /* same as above for the lsb-sub-vector */
  unsigned char exh_msb_totalpoints[TWO_TO_THE_13];
   /* the total of the points of the cards represented by the
      13 most significant bits in the vector deal */
  unsigned char exh_lsb_totalpoints[TWO_TO_THE_13];
   /* same as above for the lsb */
  unsigned char exh_total_cards_in_suit[NSUITS];
   /* the total of cards remaining to be dealt in each suit */
  unsigned char exh_total_points_in_suit[NSUITS];
   /* the total of hcps  remaining to be dealt in each suit */
  unsigned char exh_total_points;
   /* the total of hcps  remaining to be dealt */
  int *HAM_T[14], Tsize[14];
   /* Hamming-related tables.  See explanation below... */
  int completely_known_hand[4] = {0, 0, 0, 0};
#endif /* FRANCOIS */

static int uniform_random(int max) {
  unsigned rnd;
  unsigned val;
#if STD_RAND
  const int rand_max = RAND_MAX;
#else
  const unsigned rand_max = RAND_MAX << 1;
#endif
  do {
    val = RANDOM ();
    rnd = val % max;
  } while (max - rnd > rand_max - val);
  return (int)rnd;
}

static void initevalcontract () {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      for (k = 0; k < 14; k++)
        results[i][j][k] = 0;
}

static int imps (int scorediff) {
  int i, j;
  j = abs (scorediff);
  for (i = 0; i < 24; i++)
    if (imparr[i] >= j) return scorediff < 0 ? -i : i;
  return scorediff < 0 ? -i : i;
}

static int score (int vuln, int suit, int level, int tricks) {
  int total = 0;

  /* going down */
  if (tricks < 6 + level) return -50 * (1 + vuln) * (6 + level - tricks);  

  /* Tricks */
  total = total + ((suit >= SUIT_HEART) ? 30 : 20) * (tricks - 6);

  /* NT bonus */
  if (suit == SUIT_NT) total += 10;

  /* part score bonus */
  total += 50;

  /* game bonus for NT */
  if ((suit == SUIT_NT) && level >= 3) total += 250 + 200 * vuln;

  /* game bonus for major-partscore bns */
  if ((suit == SUIT_HEART || suit == SUIT_SPADE) && level >= 4) total += 250 + 200 * vuln;

  /* game bonus for minor-partscore bns */
  if ((suit == SUIT_CLUB || suit == SUIT_DIAMOND) && level >= 5) total += 250 + 200 * vuln;

  /* small slam bonus */
  if (level == 6) total += 500 + 250 * vuln;

  /* grand slam bonus */
  if (level == 7) total += 1000 + 500 * vuln;

  return total;
}

static void showevalcontract (int nh) {
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
          t += results[0][s][i] * score (v, s, l, i);
          tn += results[1][s][i] * score (v, s, l, i);
        }
        printf ("%4d/%4d ", t / nh, tn / nh);
      }
      printf ("%s", crlf);
    }
    printf ("%s", crlf);
  }
}

static int dd (deal d, int l, int c) {
  /* results-cached version of dd() */
  /* the dd cache, and the ngen it refers to */
  static int cached_ngen = -1;
  static char cached_tricks[4][5];

  /* invalidate cache if it's another deal */
  if (ngen != cached_ngen) {
      memset (cached_tricks, -1, sizeof (cached_tricks));
      cached_ngen = ngen;
  }
  if (cached_tricks[l][c] == -1) {
      /* cache the costly computation's result */
      cached_tricks[l][c] = true_dd (d, l, c);
  }

  /* return the cached value */
  return cached_tricks[l][c];
}

struct tagLibdeal libdeal;

static int get_tricks (int pn, int dn) {
  int tk = libdeal.tricks[dn];
  int resu;
  resu = (pn ? (tk >> (4 * pn)) : tk) & 0x0F;
  return resu;
}

static int true_dd (deal d, int l, int c) {
  if (loading && libdeal.valid) {
    int resu = get_tricks ((l + 1) % 4, (c + 1) % 5);
    /* This will get the number of tricks EW can get.  If the user wanted NS, 
       we have to subtract 13 from that number. */
    return ((l == 0) || (l == 2)) ? 13 - resu : resu;
  } else {
#ifdef MSDOS
    /* Ugly fix for MSDOS. Requires a file called in.txt and will create the
       files tst.pbn and out.txt. Note that we need not user crlf here, as it's
       only dealer that will read the files anyway, Micke Hovmöller 990310 */
    FILE *f;
    char tn1[] = "tst.pbn";
    char tn2[] = "out.txt";
    char res;

    f = fopen (tn1, "w+");
    if (f == 0) error ("Can't open temporary file");
    fprintcompact (f, d, 0);
    /* Write the player to lead and strain. Note that since the player to lead
       sits _behind_ declarer, the array is "eswn" instead of "nesw".
       /Micke Hovmöller 990312 */
    fprintf (f, "%c %c\n", "eswn"[l], "cdhsn"[c]);
    fclose (f);

    fflush (stdout);
    system ("bridge.exe < in.txt > out.txt");
    fflush (stdout);
    f = fopen (tn2, "r");
    if (f == 0) error ("Can't read output of analysis");

    fscanf (f, "%*[^\n]\nEnter argument line: %c", &res);
    fclose (f);
    /* This will get the number of tricks EW can get.  If the user wanted NW, 
       we have to subtract 13 from that number. */
    return ((l == 1) || (l == 3)) ? 13 - res : res;
#else
    FILE *f;
    int r;
    char cmd[1024];
    char tn1[] = "input.XXXXXX",  tn2[] = "output.XXXXXX";
    int res;

    int fd = mkstemp (tn1);
    if (fd < 0 ) error ("Can't create temporary file");
    f = fdopen (fd, "w+");
    if (f == 0 ) error ("Can't open temporary file");
    fprintcompact (f, d, 1, 1);
    fclose (f);
    fd = mkstemp (tn2);
    if (fd < 0 ) error ("Can't create temporary file");
    sprintf (cmd, "dds %s -trumps=%c -leader=%c -sol=1 | tail -n1 > %s;", tn1, "cdhsn"[c], "eswn"[l], tn2);
    system (cmd);
    f = fdopen (fd, "r");
    if (f == 0) error ("Can't read output of analysis");
    r = fscanf (f, " %d: ", &res);
    if (r != 1) fprintf(stderr, "Error reading number of triks from dds\n");
    fclose (f);
    remove (tn1);
    remove (tn2);
    return res;
#endif /* MSDOS */
  }
}

static void evalcontract () {
  int s;
  for (s = 0; s < 5; s++) {
    results[1][s][dd (curdeal, 2, s)]++;      /* south declarer */ 
    results[0][s][dd (curdeal, 0, s)]++;      /* north declarer */
  }
}

static void error (char *s) {
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
  countindex = -1;
}

void clearpointcount_alt (int cin) {
  zerocount (tblPointcount[cin]);
  countindex = cin;
}

void pointcount (int index, int value) {
  assert (index <= 12);
  if (index < 0) {
      yyerror ("too many pointcount values");
  }
  if (countindex < 0)
    tblPointcount[idxHcp][index] = value;
  else
    tblPointcount[countindex][index] = value;
}

char * mycalloc (unsigned nel, unsigned siz) {
  char *p;

  p = calloc (nel, siz);
  if (p) return p;
  fprintf (stderr, "Out of memory\n");
  exit (-1); /*NOTREACHED */
}

static void initdistr () {
  int **p4, *p3;
  int clubs, diamonds, hearts;

  int shape = 0;

  /* Allocate the four dimensional pointer array */

  for (clubs = 0; clubs <= 13; clubs++) {
    p4 = (int **) mycalloc ((unsigned) 14 - clubs, sizeof (*p4));
    distrbitmaps[clubs] = p4;
    for (diamonds = 0; diamonds <= 13 - clubs; diamonds++) {
      p3 = (int *) mycalloc ((unsigned) 14 - clubs - diamonds, sizeof (*p3));
      p4[diamonds] = p3;
      for (hearts = 0; hearts <= 13 - clubs - diamonds; hearts++) {
        p3[hearts] = ++shape;
      }
    }
  }
}

static int getshapenumber (int cl, int di, int ht, int sp)
{
  (void)sp;
  return distrbitmaps[cl][di][ht];
}

void  setshapebit (struct shape *s, int cl, int di, int ht, int sp)
{
  int nr = getshapenumber(cl, di, ht, sp);
  int idx = nr / 32;
  int bit = nr % 32;
  s->bits[idx] |= 1 << bit;
}

static int checkshape (int nr, struct shape *s)
{
  int idx = nr / 32;
  int bit = nr % 32;
  int r = (s->bits[idx] & (1 << bit)) != 0;
  return r;
}

static void newpack (deal d) {
  int suit, rank, place;

  place = 0;
  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
    for (rank = 0; rank < 13; rank++)
      d[place++] = MAKECARD (suit, rank);
}

#ifdef FRANCOIS
int hascard (deal d, int player, card onecard, int vectordeal){
  int i;
  int who;
  switch (computing_mode) {
    case STAT_MODE:
      for (i = player * 13; i < (player + 1) * 13; i++)
        if (d[i] == onecard) return 1;
      return 0;
      break;
    case EXHAUST_MODE:
      if (exh_card_map[onecard] == -1) return 0;
      who = 1 & (vectordeal >> exh_card_map[onecard]);
      return (exh_player[who] == player);
  }
  return 0;
#else
int hascard (deal d, int player, card onecard){
  int i;

  for (i = player * 13; i < (player + 1) * 13; i++)
    if (d[i] == onecard) return 1;
  return 0;
#endif /* FRANCOIS */
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

int make_contract (char suitchar, char trickchar) {
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
  return MAKECONTRACT (suit, trick);
}

static void analyze (deal d, struct handstat *hsbase) {

  /* Analyze a hand.  Modified by HU to count controls and losers. */
  /* Further mod by AM to count several alternate pointcounts too  */

  int player, next, c, r, s, t;
  card curcard;
  struct handstat *hs;

  /* for each player */
  for (player = COMPASS_NORTH; player <= COMPASS_WEST; ++player) {
    /* If the expressions in the input never mention a player
       we do not calculate his hand statistics. */
    if (use_compass[player] == 0) {

#ifdef _DEBUG
      /* In debug mode, blast the unused handstat, so that we can recognize it
         as garbage should if we accidently read from it */
      hs = hsbase + player;
      memset (hs, 0xDF, sizeof (struct handstat));
#endif /* _DEBUG_ */

      continue;
    }
    /* where are the handstats for this player? */
    hs = hsbase + player;

    /* Initialize the handstat structure */
    memset (hs, 0x00, sizeof (struct handstat));

#ifdef _DEBUG
    /* To debug, blast it with garbage.... */
    memset (hs, 0xDF, sizeof (struct handstat));

    /* then overwrite those specific counters which need to be incremented */
    for (t = idxHcp; t < idxEnd; ++t) {
      /* clear the points for each suit */
      for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
        hs->hs_counts[t][s] = 0;
      }
      /* and the total points as well */
      hs->hs_totalcounts[t] = 0;
    }

    /* clear the length for each suit */
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      hs->hs_length[s] = 0;
    }
    /* clear the total losers */
    hs->hs_totalloser = 0;
#endif /* _DEBUG_ */

    /* start from the first card for this player, and walk through them all -
       use the player offset to jump to the first card.  Can't just increment
       through the deck, because we skip those players who are not part of the
       analysis */
    next = 13 * player;
    for (c = 0; c < 13; c++) {
      curcard = d[next++];
      s = C_SUIT (curcard);
      r = C_RANK (curcard);

      /* Enable this #if to dump a visual look at the hands as they are 
         analysed.  Best bet is to use test.all, or something else that
         generates only one hand, lest you quickly run out of screen
         realestate */
#if 0
#ifdef _DEBUG
#define VIEWCOUNT
#endif /* _DEBUG_ */
#endif /* 0 */

#ifdef VIEWCOUNT
      printf ("%c%c", "CDHS"[s], "23456789TJQKA"[r]);
#endif /* VIEWCOUNT */
      hs->hs_length[s]++;
      for (t = idxHcp; t < idxEnd; ++t) {
#ifdef VIEWCOUNT
        printf (" %d ", tblPointcount[t][r]);
#endif /* VIEWCOUNT */
        hs->hs_counts[t][s] += tblPointcount[t][r];
      }
#ifdef VIEWCOUNT
      printf ("\n");
#endif /* VIEWCOUNT */
    }
#ifdef VIEWCOUNT
    printf ("---\n");
#undef VIEWCOUNT
#endif /* VIEWCOUNT */

    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      assert (hs->hs_length[s] < 14);
      assert (hs->hs_length[s] >= 0);
      switch (hs->hs_length[s]) {
        case 0: {
          /* A void is 0 losers */
          hs->hs_loser[s] = 0;
          break;
        }
        case 1: {
          /* Singleton A 0 losers, K or Q 1 loser */
          int losers[] = {1, 1, 0};
          assert (hs->hs_control[s] < 3);
          hs->hs_loser[s] = losers[hs->hs_counts[idxControls][s]];
          break;
        }
        case 2: {
          /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
          int losers[] = {2, 1, 1, 0};
          assert (hs->hs_control[s] <= 3);
          hs->hs_loser[s] = losers[hs->hs_counts[idxControls][s]];
          break;
        }
        default: {
          /* Losers, first correct the number of losers */
          assert (hs->hs_counts[idxWinners][s] < 4);
          assert (hs->hs_counts[idxWinners][s] >= 0);
          hs->hs_loser[s] = 3 - hs->hs_counts[idxWinners][s];
          break;
        }
      }

      /* Now add the losers to the total. */
      hs->hs_totalloser += hs->hs_loser[s];

      /* total up the other flavors of points */
      for (t = idxHcp; t < idxEnd; ++t) {
        hs->hs_totalcounts[t] += hs->hs_counts[t][s];
      }

      /* Now, using the values calculated already, load those pointcount
         values which are common enough to warrant a non array lookup */
      hs->hs_points[s] = hs->hs_counts[idxHcp][s];
      hs->hs_control[s] = hs->hs_counts[idxControls][s];

    } /* end for each suit */

    hs->hs_totalpoints = hs->hs_totalcounts[idxHcp];
    hs->hs_totalcontrol = hs->hs_totalcounts[idxControls];

    hs->hs_bits = distrbitmaps[hs->hs_length[SUIT_CLUB]]
      [hs->hs_length[SUIT_DIAMOND]]
      [hs->hs_length[SUIT_HEART]];
  } /* end for each player */
}

static void fprintcompact (FILE * f, deal d, int ononeline, int disablecompass) {
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

static void printdeal (deal d) {
  int suit, player, rank, cards;

  printf ("%4d.\n", (nprod+1));
  
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

static void setup_bias (void) {
  int p, s;
  char err[256];
  int len[4] = {0}, pretotal[4] = {0}, lenset[4] = {0};
  for (p = 0; p < 4; p++) {
    int totset = 0;
    int tot = 0;
    for (s = 0; s < 4; s++) {
      if (biasdeal[p][s] >= 0) {
        if (predealt[p][s] > biasdeal[p][s]) {
          sprintf(err, "More predeal cards than bias allows for %s in %s\n", player_name[p], suit_name[s]);
          error(err);
        }
        len[s] += biasdeal[p][s];
        lenset[s]++;
        tot += biasdeal[p][s];
        totset++;
        /* Remove predealt cards from bias length */
        biasdeal[p][s] -= predealt[p][s];
        biastotal += biasdeal[p][s];
      } else {
        len[s] += predealt[p][s];
        tot += predealt[p][s];
      }
      pretotal[p] += predealt[p][s];
    }
    if (tot > 13 || (totset == 4 && tot < 13)) {
      sprintf(err, "%d cards predealt for %s in %d suits\n", tot, player_name[p], totset);
      error(err);
    }
  }
  for (s = 0; s < 4; s++) {
    if (len[s] > 13 || (lenset[s] == 4 && len[s] < 13)) {
      sprintf(err, "%d cards predealt to %s in %d hands\n", len[s], suit_name[s], lenset[s]);
      error(err);
    }
  }

  if (biastotal == 0)
    return;

  /* Fill in suit reservations to stacked_pack */
  for (p = 0; p < 4; p++) {
    for (s = 0; s < 4; s++) {
      int b;
      predealt[4][s] += predealt[p][s];
      if (biasdeal[p][s] <= 0)
        continue;
      for (b = 0; b < biasdeal[p][s]; b++) {
        stacked_pack[p*13 + pretotal[p]] = CARD_C + s;
        pretotal[p]++;
      }
    }
  }
}

static int bias_pickcard(deal d, int player, int suit, int pos, int pack[4])
{
  int p, c;
  for (p = 0; p < 4; p++) {
    for (c = pack[p]; c < 13*(p+1); c++) {
      if (C_SUIT(d[c]) != suit)
        continue;
      if (pos-- == 0)
        return c;
    }
  }
  error("No card found for bias dealing\n");
  return -1;
}

static int biasfiltercounter;

static void shuffle_bias(deal d) {
  int pack[4], p, s, cards_left[4];
  if (biastotal == 0)
    return;

  biasfiltercounter = 5200 * 1000;
  for (p = 0; p < 4; p++) {
    cards_left[p] = 13 - predealt[4][p];
    pack[p] = p*13;
    while (stacked_pack[pack[p]] < CARD_C) pack[p]++;
  }
  for (p = 0; p < 4; p++) {
    for (s = 0; s < 4; s++) {
      int bias = biasdeal[p][s], b, check;
      if (bias <= 0)
        continue;
      check = CARD_C + s;
      (void)check;

      for (b = 0; b < biasdeal[p][s]; b++) {
        int pos;
        card t;
        assert (stacked_pack[pack[p]] == check);
        pos = uniform_random(cards_left[s]--);
        pos = bias_pickcard(d, p, s, pos, pack);
        t = d[pos];
        d[pos] = d[pack[p]];
        d[pack[p]] = t;
        pack[p]++;
      }
    }
  }
}

static int bias_filter(deal d, int pos, int idx) {
  const int player1 = pos / 13;
  const int suit1 = C_SUIT(d[idx]);
  const int player2 = idx / 13;
  const int suit2 = C_SUIT(d[pos]);
  if (biastotal == 0)
    return 0;
  if (biasfiltercounter-- == 0)
    error("Bias filter called too many time for a single dealer. Likely impossible bias compination.\n");
  return biasdeal[player1][suit1] >= 0 || biasdeal[player2][suit2] >= 0;
}

static void setup_deal () {
  register int i, j;

  j = 0;
  for (i = 0; i < 52; i++) {
    if (stacked_pack[i] < CARD_C) {
      curdeal[i] = stacked_pack[i];
    } else {
      while (fullpack[j] == NO_CARD)
        j++;
      curdeal[i] = fullpack[j++];
      assert (j <= 52);
    }
  }
}

void predeal (int player, card onecard) {
  int i, j;

  for (i = 0; i < 52; i++) {
    if (fullpack[i] == onecard) {
      fullpack[i] = NO_CARD;
      for (j = player * 13; j < (player + 1) * 13; j++)
        if (stacked_pack[j] == NO_CARD) {
        stacked_pack[j] = onecard;
        predealt[player][C_SUIT(onecard)]++;
        return;
        }
      yyerror ("More than 13 cards for one player");
    }
  }
  yyerror ("Card predealt twice");
}

static void initprogram () {
  int i, i_cycle;
  int val;

  setup_bias();

  /* Now initialize array zero52 with numbers 0..51 repeatedly. This whole
     charade is just to prevent having to do divisions. */
  val = 0;
  for (i = 0, i_cycle = 0; i < NRANDVALS; i++) {
    while (stacked_pack[val] != NO_CARD) {
      /* this slot is predealt, do not use it */
      val++;
      if (val == 52) {
        val = 0;
        i_cycle = i;
      }
    }
    zero52[i] = val++;
    if (val == 52) {
      val = 0;
      i_cycle = i + 1;
    }
  }
  /* Fill the last part of the array with 0xFF, just to prevent
     that 0 occurs more than 51. This is probably just for hack value */
  while (i > i_cycle) {
    zero52[i - 1] = 0xFF;
    i--;
  }
}

static void swap2 (deal d, int p1, int p2) {
  /* functions to assist "simulated" shuffling with player
     swapping or loading from Ginsberg's library.dat -- AM990423 */
  card t;
  int i;
  p1 *= 13;
  p2 *= 13;
  for (i = 0; i < 13; ++i) {
    t = d[p1 + i];
    d[p1 + i] = d[p2 + i];
    d[p2 + i] = t;
  }
}

static FILE * find_library (const char *basename, const char *openopt) {
  static const char *prefixes[] = { "", "./", "../", "../../", "c:/", "c:/data/",
    "d:/myprojects/dealer/", "d:/arch/games/gib/", 0 };
  int i;
  char buf[256];
  FILE *result = 0;
  for (i = 0; prefixes[i]; ++i) {
    strcpy (buf, prefixes[i]);
    strcat (buf, basename);
    result = fopen (buf, openopt);
    if (result) break;
  }
  return result;
}

static int shuffle (deal d) {
  int i, j, k;
  card t;

  if (loading) {
    static FILE *lib = 0;
    if (!lib) {
      lib = find_library ("library.dat", "rb");
      if (!lib) {
        fprintf (stderr, "Cannot find or open library file\n");
        exit (-1);
      }
      fseek (lib, 26 * loadindex, SEEK_SET);
    }
    if (fread (&libdeal, 26, 1, lib)) {
      int ph[4], i, suit, rank, pn;
      unsigned long su;
      libdeal.valid = 1;
      for (i = 0; i < 4; ++i)
        ph[i] = 13 * i;
      for (i = 0; i <= 4; ++i) {
        libdeal.tricks[i] = ntohs (libdeal.tricks[i]);
      }
      for (suit = 0; suit < 4; ++suit) {
        su = libdeal.suits[suit];
        su = ntohl (su);
        for (rank = 0; rank < 13; ++rank) {
          int idx;
          pn = su & 0x03;
          su >>= 2;
          idx = ph[pn]++;
          if (idx >= 52 || idx < 0) {
            fprintf(stderr, "Deal %d is broken\n", ngen);
            libdeal.valid = 0;
            return 0;
          }
          d[idx] = MAKECARD (suit, 12 - rank);
        }
      }
      return 1;
    } else {
      libdeal.valid = 0;
      return 0;
    }
  }

  if (swapindex) {
    switch (swapindex) {
      case 1:
        swap2 (d, 1, 3);
        break;
      case 2:
        swap2 (d, 2, 3);
        break;
      case 3:
        swap2 (d, 1, 2);
        break;
      case 4:
        swap2 (d, 1, 3);
        break;
      case 5:
        swap2 (d, 2, 3);
        break;
    }
  } else {
    /* Algorithm according to Knuth. For each card exchange with a random
       other card. This is supposed to be the perfect shuffle algorithm. 
       It only depends on a valid random number generator.  */
    shuffle_bias(d);
    for (i = 0; i < 52; i++) {
      if (stacked_pack[i] == NO_CARD) { 
        /* Thorvald Aagaard 14.08.1999 don't switch a predealt card */
        do {
          do {
#ifdef STD_RAND
             k = RANDOM ();
#else
             /* Upper bits most random */
             k = (RANDOM () >> (31 - RANDBITS));
#endif /* STD_RAND */
             j = zero52[k & NRANDMASK];
           } while (j == 0xFF || bias_filter(d, i, j));
        } while (stacked_pack[j] != NO_CARD);

        t = d[j];
        d[j] = d[i];
        d[i] = t;
      }
    }
  }
  if (swapping) {
    ++swapindex;
    if ((swapping == 2 && swapindex > 1) || (swapping == 3 && swapindex > 5))
      swapindex = 0;
  }
  return 1;
}

#ifdef FRANCOIS
/* Specific routines for EXHAUST_MODE */

void exh_get2players (void) {
  /* Just finds who are the 2 players for whom we make exhaustive dealing */
  int player, player_bit;
  for (player = COMPASS_NORTH, player_bit = 0; player<=COMPASS_WEST; player++) {
    if (completely_known_hand[player]) {
      if (use_compass[player]) {
        /* We refuse to compute anything for a player who has already his (her)
           13 cards */
        fprintf (stderr, 
         "Exhaust-mode error: cannot compute anything for %s (known hand)%s",
         player_name[player],crlf);
        exit (-1); /*NOTREACHED */
      }
    } else {
      if (player_bit == 2) {
        /* Exhaust mode only if *exactly* 2 hands have unknown cards */
        fprintf (stderr,
         "Exhaust-mode error: more than 2 unknown hands...%s",crlf);
        exit (-1); /*NOTREACHED */
      }
      exh_player[player_bit++] = player;
    }
  }
  if (player_bit < 2) {
    /* Exhaust mode only if *exactly* 2 hands have unknown cards */
    fprintf (stderr, "Exhaust-mode error: less than 2 unknown hands...%s",crlf);
    exit (-1); /*NOTREACHED */
  }
}

void exh_set_bit_values (int bit_pos, card onecard) {
  /* only sets up some tables (see table definitions above) */
  int suit, rank;
  int suitloop;
  suit = C_SUIT (onecard);
  rank = C_RANK (onecard);
  for (suitloop = SUIT_CLUB; suitloop <= SUIT_SPADE; suitloop++) {
    exh_suit_points_at_map[suitloop][bit_pos] = (suit == suitloop ? 
          tblPointcount[0][rank] :0);
    exh_suit_length_at_map[suitloop][bit_pos] = (suit == suitloop ? 1 : 0);
  }
  exh_card_map[onecard] = bit_pos;
  exh_card_at_bit[bit_pos] = onecard;
}

void exh_setup_card_map (void) {
  int i;
  for (i = 0; i < 256; i++) {
    exh_card_map[i] = -1; /* undefined */
  }
}

void exh_map_cards (void) {
  register int i, i_player;
  int bit_pos;

  for (i = 0, bit_pos = 0; i < 52; i++) {
    if (fullpack[i] != NO_CARD) {
      exh_set_bit_values (bit_pos, fullpack[i]);
      bit_pos++;
    }
  }
  /* Some cards may also have been predealt for the exh-players. In that case,
     those cards are "put" in the msb positions of the vectordeal. The value
     of the exh_predealt_vector is the constant part of the vector deal. */
  for (i_player = 0; i_player < 2; i_player++) {
    int player = exh_player[i_player];
    exh_empty_slots[i_player] = 0;
    for (i = 13 * player; i < 13 * (player + 1); i++) {
      if (stacked_pack[i] == NO_CARD) {
        exh_empty_slots[i_player]++;
      } else {
        exh_set_bit_values (bit_pos, stacked_pack[i]);
        exh_predealt_vector |= (i_player << bit_pos);
        bit_pos++;
      }
    }
  }
}

void exh_print_stats (struct handstat *hs) {
  int s;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    printf ("  Suit %d: ", s);
    printf ("Len = %2d, Points = %2d\n", hs->hs_length[s], hs->hs_points[s]);
  }
  printf ("  Totalpoints: %2d\n", hs->hs_totalpoints);
}

void exh_print_vector (struct handstat *hs) {
  int i, s, r;
  int onecard;
  struct handstat *hsp;

  printf ("Player %d: ", exh_player[0]);
  for (i = 0; i < 26; i++) {
    if (!(1 & (vectordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[0];
  exh_print_stats (hsp);
  printf ("Player %d: ", exh_player[1]);
  for (i = 0; i < 26; i++) {
    if ((1 & (vectordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[1];
  exh_print_stats (hsp);
}

void exh_precompute_analyse_tables (void) {
  /* This routine precomputes the values of the tables exh_lsb_... and
     exh_msb_... These tables will allow very fast computation of
     hand-primitives given the value of the vector deal.
     Example: the number of hcp in diamonds of the player 0 will be :
            exh_lsb_suit_length[vectordeal & (TWO_TO_THE_13-1)]
          + exh_msb_suit_length[vectordeal >> 13];
     The way those table are precomputed may seem a little bit ridiculous.
     This may be the reminiscence of Z80-programming ;-) FD-0499
  */
  int vec_13;
  int i_bit;
  int suit;
  unsigned char *elsp, *emsp, *elt, *emt, *elsl, *emsl;
  unsigned char *mespam, *meslam;
  unsigned char *lespam, *leslam;

  for (vec_13 = 0; vec_13 < TWO_TO_THE_13; vec_13++) {
    elt = exh_lsb_totalpoints + vec_13;
    emt = exh_msb_totalpoints + vec_13;
    *elt = *emt = 0;
    for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++) {
      elsp = exh_lsb_suit_points[suit] + vec_13;
      emsp = exh_msb_suit_points[suit] + vec_13;
      elsl = exh_lsb_suit_length[suit] + vec_13;
      emsl = exh_msb_suit_length[suit] + vec_13;
      lespam = exh_suit_points_at_map[suit] + 12;
      leslam = exh_suit_length_at_map[suit] + 12;
      mespam = exh_suit_points_at_map[suit] + 25;
      meslam = exh_suit_length_at_map[suit] + 25;
      *elsp = *emsp = *elsl = *emsl = 0;
      for (i_bit = 13; i_bit--; lespam--, leslam--, mespam--, meslam--) {
        if (!(1 & (vec_13 >> i_bit))) {
          *(elsp) += *lespam;
          *(emsp) += *mespam;
          *(elt)  += *lespam;
          *(emt)  += *mespam;
          *(elsl) += *leslam;
          *(emsl) += *meslam;
        }
      }
    }
  }
  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++) {
    exh_total_points_in_suit[suit] = exh_lsb_suit_points[suit][0] + exh_msb_suit_points[suit][0];
    exh_total_cards_in_suit[suit] = exh_lsb_suit_length[suit][0] + exh_msb_suit_length[suit][0];
  }
  exh_total_points = exh_lsb_totalpoints[0] + exh_msb_totalpoints[0];
}

void exh_analyze_vec (int high_vec, int low_vec, struct handstat *hs) {
  /* analyse the 2 remaining hands with the vectordeal data-structure.
     This is VERY fast !!!  */
  int s;
  struct handstat *hs0;
  struct handstat *hs1;
  hs0 = hs + exh_player[0];
  hs1 = hs + exh_player[1];
  hs0->hs_totalpoints = hs1->hs_totalpoints = 0;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    hs0->hs_length[s] = exh_lsb_suit_length[s][low_vec] + exh_msb_suit_length[s][high_vec];
    hs0->hs_points[s] = exh_lsb_suit_points[s][low_vec] + exh_msb_suit_points[s][high_vec];
    hs1->hs_length[s] = exh_total_cards_in_suit[s] - hs0->hs_length[s];
    hs1->hs_points[s] = exh_total_points_in_suit[s] - hs0->hs_points[s];
  }
  hs0->hs_totalpoints = exh_lsb_totalpoints[low_vec] + exh_msb_totalpoints[high_vec];
  hs1->hs_totalpoints = exh_total_points - hs0->hs_totalpoints;
  hs0->hs_bits = distrbitmaps
    [hs0->hs_length[SUIT_CLUB]]
    [hs0->hs_length[SUIT_DIAMOND]]
    [hs0->hs_length[SUIT_HEART]];
  hs1->hs_bits = distrbitmaps
    [hs1->hs_length[SUIT_CLUB]]
    [hs1->hs_length[SUIT_DIAMOND]]
    [hs1->hs_length[SUIT_HEART]];
}

/* End of Specific routines for EXHAUST_MODE */
#endif /* FRANCOIS */

static int evaltree (struct treebase *b) {
  struct tree *t = (struct tree*)b;
  switch (b->tr_type) {
    default:
      assert (0);
    case TRT_NUMBER:
      return t->tr_int1;
    case TRT_VAR:
      if (ngen != t->tr_int1) {
        t->tr_int2 = evaltree(t->tr_leaf1);
        t->tr_int1 = ngen;
      }
      return t->tr_int2;
    case TRT_AND2:
      return evaltree (t->tr_leaf1) && evaltree (t->tr_leaf2);
    case TRT_OR2:
      return evaltree (t->tr_leaf1) || evaltree (t->tr_leaf2);
    case TRT_ARPLUS:
      return evaltree (t->tr_leaf1) + evaltree (t->tr_leaf2);
    case TRT_ARMINUS:
      return evaltree (t->tr_leaf1) - evaltree (t->tr_leaf2);
    case TRT_ARTIMES:
      return evaltree (t->tr_leaf1) * evaltree (t->tr_leaf2);
    case TRT_ARDIVIDE:
      return evaltree (t->tr_leaf1) / evaltree (t->tr_leaf2);
    case TRT_ARMOD:
      return evaltree (t->tr_leaf1) % evaltree (t->tr_leaf2);
    case TRT_CMPEQ:
      return evaltree (t->tr_leaf1) == evaltree (t->tr_leaf2);
    case TRT_CMPNE:
      return evaltree (t->tr_leaf1) != evaltree (t->tr_leaf2);
    case TRT_CMPLT:
      return evaltree (t->tr_leaf1) < evaltree (t->tr_leaf2);
    case TRT_CMPLE:
      return evaltree (t->tr_leaf1) <= evaltree (t->tr_leaf2);
    case TRT_CMPGT:
      return evaltree (t->tr_leaf1) > evaltree (t->tr_leaf2);
    case TRT_CMPGE:
      return evaltree (t->tr_leaf1) >= evaltree (t->tr_leaf2);
    case TRT_NOT:
      return !evaltree (t->tr_leaf1);
    case TRT_LENGTH:      /* suit, compass */
      assert (t->tr_int1 >= SUIT_CLUB && t->tr_int1 <= SUIT_SPADE);
      assert (t->tr_int2 >= COMPASS_NORTH && t->tr_int2 <= COMPASS_WEST);
      return hs[t->tr_int2].hs_length[t->tr_int1];
    case TRT_HCPTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalpoints;
    case TRT_PT0TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxTens];
    case TRT_PT1TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxJacks];
    case TRT_PT2TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxQueens];
    case TRT_PT3TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxKings];
    case TRT_PT4TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxAces];
    case TRT_PT5TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxTop2];
    case TRT_PT6TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxTop3];
    case TRT_PT7TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxTop4];
    case TRT_PT8TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxTop5];
    case TRT_PT9TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcounts[idxC13];
    case TRT_HCP:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_points[t->tr_int2];
    case TRT_PT0:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxTens][t->tr_int2];
    case TRT_PT1:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxJacks][t->tr_int2];
    case TRT_PT2:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxQueens][t->tr_int2];
    case TRT_PT3:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxKings][t->tr_int2];
    case TRT_PT4:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxAces][t->tr_int2];
    case TRT_PT5:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxTop2][t->tr_int2];
    case TRT_PT6:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxTop3][t->tr_int2];
    case TRT_PT7:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxTop4][t->tr_int2];
    case TRT_PT8:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxTop5][t->tr_int2];
    case TRT_PT9:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_counts[idxC13][t->tr_int2];
    case TRT_SHAPE:      /* compass, shapemask */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      /* assert (t->tr_int2 >= 0 && t->tr_int2 < MAXDISTR); */
      {
        struct treeshape *s = (struct treeshape *)b;
        return (checkshape(hs[s->compass].hs_bits, &s->shape));
      }
    case TRT_HASCARD:      /* compass, card */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
#ifdef FRANCOIS
      return hascard (curdeal, t->tr_int1, (card)t->tr_int2, vectordeal);
#else
      return hascard (curdeal, t->tr_int1, (card)t->tr_int2);
#endif /* FRANCOIS */
    case TRT_LOSERTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalloser;
    case TRT_LOSER:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_loser[t->tr_int2];
    case TRT_CONTROLTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcontrol;
    case TRT_CONTROL:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_control[t->tr_int2];
    case TRT_CCCC:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return cccc (t->tr_int1);
    case TRT_QUALITY:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return quality (t->tr_int1, t->tr_int2);
    case TRT_IF:
      assert (t->tr_leaf2->tr_type == TRT_THENELSE);
      {
        struct tree *leaf2 = (struct tree *)t->tr_leaf2;
        return (evaltree (t->tr_leaf1) ? evaltree (leaf2->tr_leaf1) :
              evaltree (leaf2->tr_leaf2));
      }
    case TRT_TRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      return dd (curdeal, t->tr_int1, t->tr_int2);
    case TRT_SCORE:      /* vul/non_vul, contract, tricks in leaf1 */
      assert (t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
      return score (t->tr_int1, t->tr_int2 % 5, t->tr_int2 / 5, evaltree (t->tr_leaf1));
    case TRT_IMPS:
      return imps (evaltree (t->tr_leaf1));
    case TRT_AVG:
      return (int)(average*1000000);
    case TRT_RND:
      {
        double eval = evaltree(t->tr_leaf1);
        int random = RANDOM() & ((RAND_MAX - 1) | RAND_MAX);
        double mul = eval * random;
        double res = mul / (RAND_MAX + 1.0);
        int rv = (int)(res);
        return rv;
      }
  }
}

/* This is a macro to replace the original code :
static int interesting () {
  return evaltree (decisiontree);
}
*/
#define interesting() ((int)evaltree(decisiontree))

static void setup_action () {
  struct action *acp;

  /* Initialize all actions */
  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        initevalcontract ();
        break;
      case ACT_PRINTCOMPACT:
      case ACT_PRINTPBN:
      case ACT_PRINTEW:
      case ACT_PRINTALL:
      case ACT_PRINTONELINE:
      case ACT_PRINTES:
        break;
      case ACT_PRINT:
        deallist = (deal *) mycalloc (maxproduce, sizeof (deal));
        break;
      case ACT_AVERAGE:
        break;
      case ACT_FREQUENCY:
        acp->ac_u.acu_f.acuf_freqs = (long *) mycalloc (
           acp->ac_u.acu_f.acuf_highbnd - acp->ac_u.acu_f.acuf_lowbnd + 1,
         sizeof (long));
        break;
      case ACT_FREQUENCY2D:
        acp->ac_u.acu_f2d.acuf_freqs = (long *) mycalloc (
           (acp->ac_u.acu_f2d.acuf_highbnd_expr1 - acp->ac_u.acu_f2d.acuf_lowbnd_expr1 + 3) *
           (acp->ac_u.acu_f2d.acuf_highbnd_expr2 - acp->ac_u.acu_f2d.acuf_lowbnd_expr2 + 3),
           sizeof (long));
        break;

      }
    }
}

static void action () {
  struct action *acp;
  int expr, expr2, val1, val2, high1 = 0, high2 = 0, low1 = 0, low2 = 0;

  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        evalcontract ();
        break;
      case ACT_PRINTCOMPACT:
        printcompact (curdeal);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr);
        }
        break;
      case ACT_PRINTONELINE:
        printoneline (curdeal);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr);
        }
        break;

      case ACT_PRINTES: 
        { struct expr *pex = (struct expr *) acp->ac_expr1;
          while (pex) {
            if (pex->ex_tr) {
              expr = evaltree (pex->ex_tr);
              printf ("%d", expr);
            }
            if (pex->ex_ch) {
              printf ("%s", pex->ex_ch);
            }
            pex = pex->next;
          }
        }
        break;
      case ACT_PRINTALL:
        printdeal (curdeal);
        break;
      case ACT_PRINTEW:
        printew (curdeal);
        break;
      case ACT_PRINTPBN:
        if (! quiet)
          printpbn (nprod, curdeal);
        break;
      case ACT_PRINT:
        memcpy (deallist[nprod], curdeal, sizeof (deal));
        break;
      case ACT_AVERAGE:
        acp->ac_int1 += evaltree (acp->ac_expr1);
        break;
      case ACT_FREQUENCY:
        expr = evaltree (acp->ac_expr1);
        if (expr < acp->ac_u.acu_f.acuf_lowbnd)
          acp->ac_u.acu_f.acuf_uflow++;
        else if (expr > acp->ac_u.acu_f.acuf_highbnd)
          acp->ac_u.acu_f.acuf_oflow++;
        else
          acp->ac_u.acu_f.acuf_freqs[expr - acp->ac_u.acu_f.acuf_lowbnd]++;
        break;
      case ACT_FREQUENCY2D:
        expr = evaltree (acp->ac_expr1);
        expr2 = evaltree (acp->ac_expr2);

        high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
        high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
        low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
        low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
        if (expr > high1)
          val1 = high1 - low1 + 2;
        else {
          val1 = expr - low1 + 1;
          if (val1 < 0) val1 = 0;
        }
        if (expr2 > high2)
          val2 = high2 - low2 + 2;
        else {
          val2 = expr2 - low2 + 1;
          if (val2 < 0) val2 = 0;
        }
        acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * val1 + val2]++;
        break;
      }
    }
}

static void printhands (int boardno, deal * dealp, int player, int nhands) {
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
        if (HAS_CARD (dealp[i], player, MAKECARD (suit, rank))) {
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

static void cleanup_action () {
  struct action *acp;
  int player, i;

  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_PRINTALL:
      case ACT_PRINTCOMPACT:
      case ACT_PRINTPBN:
      case ACT_PRINTEW:
      case ACT_PRINTONELINE:
      case ACT_PRINTES:
        break;
      case ACT_EVALCONTRACT:
        showevalcontract (nprod);
        break;
      case ACT_PRINT:
        for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
          if (!(acp->ac_int1 & (1 << player)))
          continue;
          printf ("\n\n%s hands:\n\n\n\n", player_name[player]);
          for (i = 0; i < nprod; i += 4)
          printhands (i, deallist + i, player, nprod - i > 4 ? 4 : nprod - i);
            printf ("\f");
        }
        break;
      case ACT_AVERAGE:
        average = (double)acp->ac_int1 / nprod;
        if (acp->ac_expr2 && !evaltree(acp->ac_expr2))
          break;
        if (acp->ac_str1)
          printf ("%s: ", acp->ac_str1);
        printf ("%g\n", average);
        break;
      case ACT_FREQUENCY:
        printf ("Frequency %s:\n", acp->ac_str1 ? acp->ac_str1 : "");
        if (acp->ac_u.acu_f.acuf_uflow)
          printf ("Low\t%8ld\n", acp->ac_u.acu_f.acuf_uflow);
        for (i = acp->ac_u.acu_f.acuf_lowbnd; i <= acp->ac_u.acu_f.acuf_highbnd; i++) {
          if (acp->ac_u.acu_f.acuf_freqs[i - acp->ac_u.acu_f.acuf_lowbnd] > 0)
            printf ("%5d\t%8ld\n", i, acp->ac_u.acu_f.acuf_freqs[i - acp->ac_u.acu_f.acuf_lowbnd]);
        }
        if (acp->ac_u.acu_f.acuf_oflow)
          printf ("High\t%8ld\n", acp->ac_u.acu_f.acuf_oflow);
        break;
      case ACT_FREQUENCY2D: {
        int j, n = 0, low1 = 0, high1 = 0, low2 = 0, high2 = 0, sumrow,
          sumtot, sumcol, *toprintcol, *toprintrow;
        printf ("Frequency %s:%s", acp->ac_str1 ? acp->ac_str1 : "", crlf);
        high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
        high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
        low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
        low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
        toprintcol = (int*)mycalloc((high2 - low2 + 3), sizeof(int));
        toprintrow = (int*)mycalloc((high1 - low1 + 3), sizeof(int));
        for (i = 0; i < (high1 - low1) + 3; i++) {
          for (j = 0; j < (high2 - low2) + 3; j++) {
            toprintcol[j] |= acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
            toprintrow[i] |= acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
          }
        }
        if (toprintcol[0] != 0)
          printf ("        Low");
        for (j = 1; j < (high2 - low2) + 2; j++) {
          if (toprintcol[j] != 0)
            printf (" %6d", j + low2 - 1);
        }
        if (toprintcol[j] != 0)
          printf ("   High");
        printf ("    Sum%s", crlf);
        sumtot = 0;
        for (i = 0; i < (high1 - low1) + 3; i++) {
          sumrow = 0;
          if (toprintrow[i] == 0)
            continue;
          if (i == 0)
            printf ("Low ");
          else if (i == (high1 - low1 + 2))
            printf ("High");
          else
            printf ("%4d", i + low1 - 1);
          for (j = 0; j < (high2 - low2) + 3; j++) {
            n = acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
            sumrow += n;
            if (toprintcol[j] != 0)
              printf (" %6d", n);
          }
          printf (" %6d%s", sumrow, crlf);
          sumtot += sumrow;
        }
        printf ("Sum ");
        for (j = 0; j < (high2 - low2) + 3; j++) {
          sumcol = 0;
          for (i = 0; i < (high1 - low1) + 3; i++)
            sumcol += acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
          if (toprintcol[j] != 0)
            printf (" %6d", sumcol);
        }
        printf (" %6d%s%s", sumtot, crlf, crlf);
        free(toprintcol);
        free(toprintrow);
      }
    }
  }
}

int yywrap () {
  /* Necessary if you do not have a -ll library */
  return 1;
}

static void printew (deal d) {
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

int main (int argc, char **argv) {
  int seed_provided = 0;
  extern int optind;
  extern char *optarg;
  char c;
  int errflg = 0;
  int progressmeter = 0;
  int i=0;

  struct timeval tvstart, tvstop;

  verbose = 1;

  gettimeofday (&tvstart, (void *) 0);

  while ((c = getopt (argc, argv, "023ehvmqp:g:s:l:V")) != -1) {
    switch (c) {
      case '0':
      case '2':
      case '3':
        swapping = c - '0';
        break;
      case 'e':
#ifdef FRANCOIS
        computing_mode = EXHAUST_MODE;
        break;
#else
        /* Break if code not included in executable */
        printf ("Exhaust mode not included in this executable\n");
        return 1;
#endif /* FRANCOIS */
      case 'l':
        loading = 1;
        loadindex = atoi (optarg);
        break;
      case 'g':
        maxgenerate = atoi (optarg);
        break;
      case 'm':
        progressmeter ^= 1;
        break;
      case 'p':
        maxproduce = atoi (optarg);
        break;
      case 's':
        seed_provided = 1;
        seed = atol (optarg);
        if (seed == LONG_MIN || seed == LONG_MAX) {
            fprintf (stderr, "Seed overflow: seed must be between %ld and %ld\n", 
              LONG_MIN, LONG_MAX);
            exit (-1);
        }
        break;
      case 'v':
        verbose ^= 1;
        break;
      case 'q':
        quiet ^= 1;
        break;
      case 'V':
        printf ("Version info....\n");
        printf ("$Revision: 1.24 $\n");
        printf ("$Date: 2003/08/05 19:53:04 $\n");
        printf ("$Author: henk $\n");
        return 1;
      case '?':
      case 'h':
        errflg = 1;
        break;
      }
    }
  if (argc - optind > 2 || errflg) {
    fprintf (stderr, "Usage: %s [-emv] [-s seed] [-p num] [-v num] [inputfile]\n", argv[0]);
    exit (-1);
  }
  if (optind < argc && freopen (input_file = argv[optind], "r", stdin) == NULL) {
    perror (argv[optind]);
    exit (-1);
  }
  newpack (fullpack);
  /* Empty pack */
  for (i = 0; i < 52; i++) stacked_pack[i] = NO_CARD;
  initdistr ();
  maxdealer = -1;
  maxvuln = -1;

  yyparse ();

  /* The most suspect part of this program */
  if (!seed_provided) {
    (void) time (&seed);
  }
  SRANDOM (seed);

  initprogram ();
  if (maxgenerate == 0)
    maxgenerate = 10000000;
  if (maxproduce == 0)
    maxproduce = ((actionlist == &defaultaction) || will_print) ? 40 : maxgenerate;

  setup_action ();

  if (progressmeter)
    fprintf (stderr, "Calculating...  0%% complete\r");

  switch (computing_mode) {
    case STAT_MODE:
      setup_deal ();
      for (ngen = nprod = 0; ngen < maxgenerate && nprod < maxproduce; ngen++) {
        shuffle (curdeal);
        analyze (curdeal, hs);
        if (interesting ()) {
          action ();
          nprod++;
          if (progressmeter) {
            if ((100 * nprod / maxproduce) > 100 * (nprod - 1) / maxproduce)
              fprintf (stderr, "Calculating... %2d%% complete\r",
               100 * nprod / maxproduce);
          }
        }
      }
      break;
#ifdef FRANCOIS
    case EXHAUST_MODE: {
      int i, j, half, ham13, c_tsize[14], highvec, *lowvec_ptr;
      int emptyslots;

      exh_get2players ();
      exh_setup_card_map ();
      exh_map_cards ();
      exh_precompute_analyse_tables ();
      emptyslots = exh_empty_slots[0] + exh_empty_slots[1];
      /* building the T table :
         o HAM_T[ham13] will contain all the 13-bits vector that have a hamming weight 'ham13'
         o Tsize[ham13] will contain the number of such possible vectors: 13/(ham13!(13-ham13)!)
         o Tsize will be mirrored into c_tsize
       */
      for (c_tsize[i = 13] = Tsize[i = 13] = 1, HAM_T[13] = (int *) malloc (sizeof (int)); i--;)
        HAM_T[i] = (int *) malloc (sizeof (int) * (c_tsize[i] = Tsize[i] = Tsize[i + 1] * (i + 1) / (13 - i)));
      /*
         one generate the 2^13 possible half-vectors and :
         o compute its hamming weight;
         o store it into the table T
       */
      for (half = 1 << 13; half--;) {
        for (ham13 = 0, i = 13; i--;)
          ham13 += (half >> i) & 1;
        HAM_T[ham13][--c_tsize[ham13]] = half;
      }

      for (ham13 = 14; ham13--;)
        for (i = Tsize[ham13]; i--;) {
          highvec = HAM_T[ham13][i];
          if (!(((highvec << 13) ^ exh_predealt_vector) >> emptyslots)) {
            for (lowvec_ptr = HAM_T[13 - ham13], j = Tsize[ham13]; j--; lowvec_ptr++) {
              ngen++;
              vectordeal = (highvec << 13) | *lowvec_ptr;
              exh_analyze_vec (highvec, *lowvec_ptr, hs);
              if (interesting ()) {
                /*  exh_print_vector(hs); */
                action ();
                nprod++;
              }
            }
          }
        }
      }
      break;
#endif /* FRANCOIS */
    default:
      fprintf (stderr, "Unrecognized computation mode...\n");
      exit (-1); /*NOTREACHED */
    }
  if (progressmeter)
    fprintf (stderr, "                                      \r");
  gettimeofday (&tvstop, (void *) 0);
  cleanup_action ();
  if (verbose) {
    printf ("Generated %d hands\n", ngen);
    printf ("Produced %d hands\n", nprod);
    printf ("Initial random seed %lu\n", seed);
    printf ("Time needed %8.3f sec%s", 
             (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 -
             (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)), crlf);
  }
  return 0;
}
