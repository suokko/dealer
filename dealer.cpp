#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "bittwiddle.h"
#include "pregen.h"
#include "dds.h"

#include <getopt.h>

#include "tree.h"
#include "pointcount.h"
#include "dealer.h"
#include "c4.h"
#include "pbn.h"
#include "genlib.h"
#include "shuffle.h"

#include "card.h"

#if __BMI2__
#include <x86intrin.h>
#endif

void yyerror (char *);

#define DEFAULT_MODE STAT_MODE

#ifdef MSDOS
  static const char * const crlf = "\r\n";
#else
  static const char * const crlf = "\n";
#endif /* MSDOS */

/* Global variables */

static struct handstat hs[4];

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/

/* Function definitions */
static int true_dd (const union board *d, int l, int c); /* prototype */

static struct globals *gp;

static void initevalcontract () {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      for (k = 0; k < 14; k++)
        gp->results[i][j][k] = 0;
}

static int dd (const union board *d, int l, int c) {
  /* results-cached version of dd() */
  /* the dd cache, and the gp->ngen it refers to */
  static int cached_ngen = -1;
  static char cached_tricks[4][5];

  /* invalidate cache if it's another deal */
  if (gp->ngen != cached_ngen) {
      memset (cached_tricks, -1, sizeof (cached_tricks));
      cached_ngen = gp->ngen;
  }
  if (cached_tricks[l][c] == -1) {
      /* cache the costly computation's result */
      cached_tricks[l][c] = true_dd (d, l, c);
  }

  /* return the cached value */
  return cached_tricks[l][c];
}

static struct value lead_dd (const union board *d, int l, int c) {
  char res[13];
  struct value r;
  struct value_array *arr = (value_array *)mycalloc(1, sizeof(*arr));
  unsigned idx, fill = 0;
  memset(res, -1, sizeof(res));
  card hand = gptr->predealt.hands[(l+1) % 4];
  if (hand_count_cards(hand) != 13) {
    char errmsg[512];
    snprintf(errmsg, sizeof(errmsg),
        "Opening lead hand (%s) doesn't have 13 cards defined with predeal when calling leadtricks(%s, %s).\n"
        "Compass parameter to leadtriks function is the declarer of hand like tricks function.\n",
        player_name[(l+1) % 4], player_name[l], suit_name[c]);
    error(errmsg);
  }
  solveLead(d, l, c, hand, res);
  for (idx = 0; idx < sizeof(res); idx++) {
    card c = hand_extract_card(hand);
    hand &= ~c;
    if (res[idx] == -1)
      continue;
    arr->key[fill] = C_BITPOS(c);
    arr->value[fill] = res[idx];
    fill++;
  }
  for (; fill < sizeof(res); fill++)
    arr->value[fill] = -1;
  r.type = VAL_INT_ARR;
  r.array = arr;
  return r;
}

static int get_tricks (int pn, int dn) {
  int tk = gp->libtricks[dn];
  int resu;
  resu = (pn ? (tk >> (4 * pn)) : tk) & 0x0F;
  return resu;
}

static int true_dd (const union board *d, int l, int c) {
  if (gp->loading) {
    int resu = get_tricks (l, c);
    /* This will get the number of tricks EW can get.  If the user wanted NS,
       we have to subtract 13 from that number. */
    return ((l == 0) || (l == 2)) ? 13 - resu : resu;
  } else {
    return solve(d, l, c);
  }
}

static void evalcontract () {
  int s;
  for (s = 0; s < 5; s++) {
    gp->results[1][s][dd (&gp->curboard, 2, s)]++;      /* south declarer */
    gp->results[0][s][dd (&gp->curboard, 0, s)]++;      /* north declarer */
  }
}

static int checkshape (int nr, struct shape *s)
{
  int idx = nr / 32;
  int bit = nr % 32;
  int r = (s->bits[idx] & (1 << bit)) != 0;
  return r;
}

static card statichascard (const union board *d, int player, card onecard) {
  return hand_has_card(d->hands[player], onecard);
}

/*
 * Calculate and cache hcp for a player
 *
 * @param d       Current board to analyze
 * @param hsbase  The base of handstat cache
 * @param compass The player to calculate stats for
 * @param suit    The suit to calculate or 4 for complete hand
 */
static int hcp (const union board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      /* Cache interleaves generation and actual value */
      if (hs->hs_points[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        const hand h = suit == 4 ? d->hands[compass] :
                                   d->hands[compass] & suit_masks[suit];

        hs->hs_points[suit*2]   = gp->ngen;
        hs->hs_points[suit*2+1] = getpc(idxHcp, h);
      }
      return hs->hs_points[suit*2+1];
}

static int control (const union board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      if (hs->hs_control[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        const hand h = suit == 4 ? d->hands[compass] :
                                   d->hands[compass] & suit_masks[suit];

        hs->hs_control[suit*2]   = gp->ngen;
        hs->hs_control[suit*2+1] = getpc(idxControls, h);
      }
      return hs->hs_control[suit*2+1];
}

/**
 * Assume that popcnt is fast enough operation not needing caching.
 */
static inline int staticsuitlength (const union board* d,
        int compass,
        int suit)
{
  assert (suit >= SUIT_CLUB && suit <= SUIT_SPADE);
  assert(compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
  hand h = d->hands[compass] & suit_masks[suit];
  return hand_count_cards(h);
}

int suitlength (const union board* d,
        int compass,
        int suit)
{
  return staticsuitlength(d, compass, suit);
}

/**
 * The nit position in shape bitmap for this board is calculated from length of suits.
 */
static int distrbit (const union board* d, struct handstat *hsbase, int compass)
{
  assert(compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
  struct handstat *hs = &hsbase[compass];

  if (hs->hs_bits[0] != gp->ngen) {
    hs->hs_bits[0] = gp->ngen;
    hs->hs_bits[1] = getshapenumber(staticsuitlength(d, compass, SUIT_CLUB),
      staticsuitlength(d, compass, SUIT_DIAMOND),
      staticsuitlength(d, compass, SUIT_HEART));
  }
  return hs->hs_bits[1];
}
static int loser (const union board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      if (hs->hs_loser[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        hs->hs_loser[suit*2] = gp->ngen;
        if (suit == 4) {
          hs->hs_loser[4*2+1] = 0;
          for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
            hs->hs_loser[4*2+1] += loser(d, hsbase, compass, suit);
        } else {
          const int length = staticsuitlength(d, compass, suit);
          const hand h = d->hands[compass] & suit_masks[suit];
          int control = getpc(idxControlsInt, h);
          int winner = getpc(idxWinnersInt, h);

          switch (length) {
          case 0:
            /* A void is 0 losers */
            hs->hs_loser[suit*2+1] = 0;
            break;
          case 1:
            {
              /* Singleton A 0 losers, K or Q 1 loser */
              int losers[] = {1, 1, 0};
              assert (control < 3);
              hs->hs_loser[suit*2+1] = losers[control];
              break;
            }
          case 2:
            {
              /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
              int losers[] = {2, 1, 1, 0};
              assert (control <= 3);
              hs->hs_loser[suit*2+1] = losers[control];
              break;
            }
          default:
            /* Losers, first correct the number of losers */
            hs->hs_loser[suit*2+1] = 3 - winner;
            break;
          }
        }
      }
      return hs->hs_loser[suit*2+1];
}

static void initprogram (void)
{
  initpc();

  /* clear the handstat cache */
  memset(hs, -1, sizeof(hs));
}

/* Specific routines for EXHAUST_MODE */

static void exh_get2players (unsigned exh_player[2]) {
  /* Just finds who are the 2 players for whom we make exhaustive dealing */
  int player, player_bit;
  for (player = COMPASS_NORTH, player_bit = 0; player<=COMPASS_WEST; player++) {
    if (hand_count_cards(gp->predealt.hands[player]) != 13) {
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

static unsigned exh_map_cards (union board* b,
    unsigned exh_player[2],
    unsigned* exh_vect_length,
    card *exh_card_at_bit) {
  int i;
  int bit_pos;
  hand predeal = 0;
  hand p1 = 0;
  hand p0 = 0;
  unsigned exh_vectordeal = 0;
#if __BMI2__
  hand allbits = 0;
#endif

  for (i = 0; i < 4; i++) {
    predeal |= gp->predealt.hands[i];
    b->hands[i] = gp->predealt.hands[i];
  }

  /* Fill in all cards not predealt */
  hand shuffle = ~predeal & all_suits_mask;
  for (bit_pos = 0; shuffle;) {
    // remove lowest set bit
    hand next_shuffle = (shuffle - 1) & shuffle;
    // Isolate the changed card
    card onecard = next_shuffle ^ shuffle;
    shuffle = next_shuffle;
#if __BMI2__
    allbits |= onecard;
#else
    exh_card_at_bit[bit_pos] = onecard;
#endif
    bit_pos++;
  }

  int p1cnt = 13 - hand_count_cards(gp->predealt.hands[exh_player[1]]);

  *exh_vect_length = bit_pos;
  /* Set N lower bits that will be moved to high bits */
  exh_vectordeal = (1u << p1cnt) - 1;

  // Map cards to hands
#if __BMI2__
  exh_card_at_bit[0] = allbits;

  p1 = _pdep_u64(exh_vectordeal, allbits);
  unsigned mask = (1u << bit_pos) - 1;
  p0 = _pdep_u64(~exh_vectordeal & mask, allbits);
#else
  for (i = 0; i < p1cnt; i++)
    p1 |= exh_card_at_bit[i];
  for (; i < bit_pos; i++)
    p0 |= exh_card_at_bit[i];
#endif

  assert(hand_count_cards(p0 | b->hands[exh_player[0]]) == 13);
  assert(hand_count_cards(p1 | b->hands[exh_player[1]]) == 13);
  b->hands[exh_player[0]] |= p0;
  b->hands[exh_player[1]] |= p1;

  return exh_vectordeal;
}

static inline void exh_print_stats (struct handstat *hs, int hs_length[4]) {
  int s;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    printf ("  Suit %d: ", s);
    printf ("Len = %2d, Points = %2d\n", hs_length[s], hs->hs_points[s*2+1]);
  }
  printf ("  Totalpoints: %2d\n", hs->hs_points[s*2+1]);
}

static inline void exh_print_cards (hand vector,
    unsigned exh_vect_length,
    card *exh_card_at_bit)
{
#if __BMI2__
  (void)exh_vect_length;
  hand hand = _pdep_u64(vector, exh_card_at_bit[0]);
  while (hand) {
    card c = hand_extract_card(hand);
    printcard(c);
    hand = hand_remove_card(hand, c);
  }
#else
  for (unsigned i = 0; i < exh_vect_length; i++) {
    if (vector & (1u << i)) {
      card onecard = exh_card_at_bit[i];
      printcard(onecard);
    }
  }
#endif
}

static inline void exh_print_vector (struct handstat *hs,
    unsigned exh_player[2],
    unsigned exh_vectordeal,
    unsigned exh_vect_length,
    card *exh_card_at_bit)
{
  int s;
  int hs_length[4];

  hand mask = (1u << exh_vect_length) - 1;

  printf ("Player %d: ", exh_player[0]);
  exh_print_cards(~exh_vectordeal & mask, exh_vect_length, exh_card_at_bit);
  printf ("\n");
  for (s = SUIT_CLUB; s < NSUITS; s++) {
    hs_length[s] = staticsuitlength(&gp->curboard, exh_player[0], s);
    hcp(&gp->curboard, hs, exh_player[0],  s);
    hcp(&gp->curboard, hs, exh_player[1],  s);
  }
  exh_print_stats (hs + exh_player[0], hs_length);
  printf ("Player %d: ", exh_player[1]);
  exh_print_cards(exh_vectordeal, exh_vect_length, exh_card_at_bit);
  printf ("\n");
  for (s = SUIT_CLUB; s < NSUITS; s++)
    hs_length[s] = staticsuitlength(&gp->curboard, exh_player[1], s);
  exh_print_stats (hs + exh_player[1], hs_length);
}

static unsigned bitpermutate(unsigned vector)
{
  /* set all lowest zeros to one */
  unsigned bottomones = vector | (vector - 1);
  /* Set the lowest zero bit in original */
  unsigned nextvector = bottomones + 1;
  /* flip bits */
  unsigned moveback = ~bottomones;
  /* select the lowest zero bit in bottomones */
  moveback &= 0 - moveback;
  /* set to ones all low bits that are ones in bottomones */
  moveback--;
  /* move back set bits the number of trailing zeros plus one in original
   * number. This removes the lowest set bit in original vector and moves
   * all ones next to it to bottom.
   */
  moveback >>= __builtin_ctz(vector) + 1;
  /* combine the result vector to get next permutation */
  return nextvector | moveback;
}

static unsigned exh_shuffle (union board* b,
    unsigned exh_player[2],
    unsigned prevvect,
    card *exh_card_at_bit) {
  hand bitstoflip = 0;

  unsigned exh_vectordeal = bitpermutate(prevvect);
  unsigned changed = exh_vectordeal ^ prevvect;

#if __BMI2__
  bitstoflip = _pdep_u64(changed, exh_card_at_bit[0]);
#else
  do {
    unsigned last = __builtin_ctz(changed);
    bitstoflip |= exh_card_at_bit[last];
    changed &= changed - 1;
  } while (changed);
#endif

  b->hands[exh_player[0]] ^= bitstoflip;
  b->hands[exh_player[1]] ^= bitstoflip;

  return exh_vectordeal;
}

/* End of Specific routines for EXHAUST_MODE */

static struct value evaltree (struct treebase *b) {
  struct tree *t = (struct tree*)b;
  struct value r;
  switch (b->tr_type) {
    default:
      assert (0);
    case TRT_NUMBER:
      r.type = VAL_INT;
      r.intvalue = t->tr_int1;
      return r;
    case TRT_VAR:
      if (gp->ngen != t->tr_int1) {
        r = evaltree(t->tr_leaf1);
        t->tr_int1 = gp->ngen;
        t->tr_int2 = r.type;
        t->tr_leaf2 = (treebase*)r.array;
        return r;
      }
      r.type = (value_type)t->tr_int2;
      r.array = (value_array*)t->tr_leaf2;
      return r;
    case TRT_AND2:
      {
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        if (!r.intvalue)
          return r;
        r = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = !!r.intvalue;
        return r;
      }
    case TRT_OR2:
      {
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        if (r.intvalue) {
          r.intvalue = 1;
          return r;
        }
        r = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = !!r.intvalue;
        return r;
      }
    case TRT_ARPLUS:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r2 = evaltree(t->tr_leaf2);
        if (r2.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r.intvalue += r2.intvalue;
        return r;
      }
    case TRT_ARMINUS:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r2 = evaltree(t->tr_leaf2);
        if (r2.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r.intvalue -= r2.intvalue;
        return r;
      }
    case TRT_ARTIMES:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r2 = evaltree(t->tr_leaf2);
        if (r2.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r.intvalue *= r2.intvalue;
        return r;
      }
    case TRT_ARDIVIDE:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r2 = evaltree(t->tr_leaf2);
        if (r2.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r.intvalue /= r2.intvalue;
        return r;
      }
    case TRT_ARMOD:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r2 = evaltree(t->tr_leaf2);
        if (r2.type != VAL_INT)
          error("Only int supported for arithmetic\n");
        r.intvalue %= r2.intvalue;
        return r;
      }
    case TRT_CMPEQ:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue == r2.intvalue;
        return r;
      }
    case TRT_CMPNE:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue != r2.intvalue;
        return r;
      }
    case TRT_CMPLT:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue < r2.intvalue;
        return r;
      }
    case TRT_CMPLE:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue <= r2.intvalue;
        return r;
      }
    case TRT_CMPGT:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue > r2.intvalue;
        return r;
      }
    case TRT_CMPGE:
      {
        struct value r2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r2 = evaltree(t->tr_leaf2);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = r.intvalue >= r2.intvalue;
        return r;
      }
    case TRT_NOT:
      {
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for comparison\n");
        r.intvalue = !r.intvalue;
        return r;
      }
    case TRT_LENGTH:      /* suit, compass */
      assert (t->tr_int1 >= SUIT_CLUB && t->tr_int1 <= SUIT_SPADE);
      assert (t->tr_int2 >= COMPASS_NORTH && t->tr_int2 <= COMPASS_WEST);
      {
        r.intvalue = staticsuitlength(&gp->curboard, t->tr_int2, t->tr_int1);
        r.type = VAL_INT;
        return r;
      }
    case TRT_HCPTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      {
        r.intvalue = hcp(&gp->curboard, hs, t->tr_int1, 4);
        r.type = VAL_INT;
        return r;
      }
    case TRT_PT0TOTAL:      /* compass */
    case TRT_PT1TOTAL:      /* compass */
    case TRT_PT2TOTAL:      /* compass */
    case TRT_PT3TOTAL:      /* compass */
    case TRT_PT4TOTAL:      /* compass */
    case TRT_PT5TOTAL:      /* compass */
    case TRT_PT6TOTAL:      /* compass */
    case TRT_PT7TOTAL:      /* compass */
    case TRT_PT8TOTAL:      /* compass */
    case TRT_PT9TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      {
        r.intvalue = getpc(idxTens + (b->tr_type - TRT_PT0TOTAL) / 2, gp->curboard.hands[t->tr_int1]);
        r.type = VAL_INT;
        return r;
      }
    case TRT_HCP:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      {
        r.intvalue = hcp(&gp->curboard, hs, t->tr_int1, t->tr_int2);
        r.type = VAL_INT;
        return r;
      }
    case TRT_PT0:      /* compass, suit */
    case TRT_PT1:      /* compass, suit */
    case TRT_PT2:      /* compass, suit */
    case TRT_PT3:      /* compass, suit */
    case TRT_PT4:      /* compass, suit */
    case TRT_PT5:      /* compass, suit */
    case TRT_PT6:      /* compass, suit */
    case TRT_PT7:      /* compass, suit */
    case TRT_PT8:      /* compass, suit */
    case TRT_PT9:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      {
        r.intvalue = getpc(idxTens + (b->tr_type - TRT_PT0) / 2, gp->curboard.hands[t->tr_int1] & suit_masks[t->tr_int2]);
        r.type = VAL_INT;
        return r;
      }
    case TRT_SHAPE:      /* compass, shapemask */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      {
        struct treeshape *s = (struct treeshape *)b;
        r.intvalue = (checkshape(distrbit(&gp->curboard, hs, s->compass), &s->shape));
        r.type = VAL_INT;
        return r;
      }
    case TRT_HASCARD:      /* compass, card */
      {
        struct treehascard *hc = (struct treehascard *)t;
        assert (hc->compass >= COMPASS_NORTH && hc->compass <= COMPASS_WEST);
        r.intvalue = statichascard (&gp->curboard, hc->compass, hc->c) > 0;
        r.type = VAL_INT;
        return r;
      }
    case TRT_LOSERTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      r.intvalue = loser(&gp->curboard, hs, t->tr_int1, 4);
      r.type = VAL_INT;
      return r;
    case TRT_LOSER:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      r.intvalue = loser(&gp->curboard, hs, t->tr_int1, t->tr_int2);
      r.type = VAL_INT;
      return r;
    case TRT_CONTROLTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      r.intvalue = control(&gp->curboard, hs, t->tr_int1, 4);
      r.type = VAL_INT;
      return r;
    case TRT_CONTROL:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      r.intvalue = control(&gp->curboard, hs, t->tr_int1, t->tr_int2);
      r.type = VAL_INT;
      return r;
    case TRT_CCCC:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      r.intvalue = cccc (t->tr_int1);
      r.type = VAL_INT;
      return r;
    case TRT_QUALITY:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      r.intvalue = quality (t->tr_int1, t->tr_int2);
      r.type = VAL_INT;
      return r;
    case TRT_IF:
      assert (t->tr_leaf2->tr_type == TRT_THENELSE);
      {
        struct tree *leaf2 = (struct tree *)t->tr_leaf2;
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for branching");
        return (r.intvalue ? evaltree (leaf2->tr_leaf1) :
              evaltree (leaf2->tr_leaf2));
      }
    case TRT_TRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      r.intvalue = dd (&gp->curboard, t->tr_int1, t->tr_int2);
      r.type = VAL_INT;
      return r;
    case TRT_LEADTRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      return lead_dd (&gp->curboard, t->tr_int1, t->tr_int2);
    case TRT_SCORE:      /* vul/non_vul, contract, tricks in leaf1 */
      assert (t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
      {
        int cntr = t->tr_int2 & (64 - 1);
        return score (t->tr_int1, cntr % 5, cntr / 5, t->tr_int2 & 64, evaltree (t->tr_leaf1));
      }
    case TRT_IMPS:
      r = evaltree (t->tr_leaf1);
      if (r.type != VAL_INT)
        error("Only int support for imps");
      r.intvalue = imps (r.intvalue);
      return r;
    case TRT_AVG:
      r.intvalue = (int)(gptr->average*1000000);
      r.type = VAL_INT;
      return r;
    case TRT_ABS:
      r = evaltree (t->tr_leaf1);
      if (r.type != VAL_INT)
        error("Only int support for abs");
      r.intvalue = abs(r.intvalue);
      return r;
    case TRT_RND:
      {
        r = evaltree(t->tr_leaf1);
        if (r.type != VAL_INT)
          error("Only int supported for RND");
        unsigned random = DEFUN(random32)(gptr->shuffle, r.intvalue - 1);
        int rv = (int)random;
        r.intvalue = rv;
        return r;
      }
  }
}

/* This is a macro to replace the original code :
static int interesting () {
  return evaltree (decisiontree);
}
*/
#define interesting() ((int)evaltree(gp->decisiontree).intvalue)

static void setup_action () {
  struct action *acp;

  /* Initialize all actions */
  for (acp = gp->actionlist; acp != 0; acp = acp->ac_next) {
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
        gp->deallist = (struct stored_board *) mycalloc (gp->maxproduce, sizeof (gp->deallist[0]));
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

static void frequency2dout(struct action *acp, int expr,  int expr2) {
        int val1, val2, high1 = 0, high2 = 0, low1 = 0, low2 = 0;

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
}

static void frequency_to_lead(struct action *acp, struct value val) {
        int low1 = acp->ac_u.acu_f.acuf_lowbnd;
        int high1 = acp->ac_u.acu_f.acuf_highbnd;
        int low2 = 0;
        int high2 = 12;
        card *h;
        struct value_array *arr = val.array;

        free(acp->ac_u.acu_f.acuf_freqs);

        acp->ac_type = ACT_FREQUENCYLEAD;
        acp->ac_u.acu_f2d.acuf_lowbnd_expr1 = low1;
        acp->ac_u.acu_f2d.acuf_highbnd_expr1 = high1;
        acp->ac_u.acu_f2d.acuf_lowbnd_expr2 = low2;
        acp->ac_u.acu_f2d.acuf_highbnd_expr2 = high2;
        acp->ac_u.acu_f2d.acuf_freqs = (long *)mycalloc(
                        (high1 - low1 + 3) * (high2 - low2 + 3),
                        sizeof(long));
        h = (card*)mycalloc(high2 - low2 + 1, sizeof(card));
        for (;low2 <= high2; low2++) {
               card c = 1LL << arr->key[low2];
               h[low2] = c;
        }
        acp->ac_expr2 = (treebase*)h;
};

static void action () {
  struct action *acp;
  struct value expr, expr2;

  for (acp = gp->actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        evalcontract ();
        break;
      case ACT_PRINTCOMPACT:
        printcompact (&gp->curboard);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr.intvalue);
        }
        break;
      case ACT_PRINTONELINE:
        printoneline (&gp->curboard);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr.intvalue);
        }
        break;

      case ACT_PRINTES:
        { struct expr *pex = (struct expr *) acp->ac_expr1;
          while (pex) {
            if (pex->ex_tr) {
              expr = evaltree (pex->ex_tr);
              printf ("%d", expr.intvalue);
            }
            if (pex->ex_ch) {
              printf ("%s", pex->ex_ch);
            }
            pex = pex->next;
          }
        }
        break;
      case ACT_PRINTALL:
        printdeal (&gp->curboard);
        break;
      case ACT_PRINTEW:
        printew (&gp->curboard);
        break;
      case ACT_PRINTPBN:
        if (! gp->quiet)
          printpbn (gp->nprod, &gp->curboard);
        break;
      case ACT_PRINT:
        board_to_stored(&gp->deallist[gp->nprod], &gp->curboard);
        break;
      case ACT_AVERAGE:
        expr = evaltree(acp->ac_expr1);
        acp->ac_int1 += expr.intvalue;
        break;
      case ACT_FREQUENCY:
        expr = evaltree (acp->ac_expr1);
        if (expr.type == VAL_INT_ARR) {
          frequency_to_lead(acp, expr);
          goto frequencylead;
        }
        if (expr.intvalue < acp->ac_u.acu_f.acuf_lowbnd)
          acp->ac_u.acu_f.acuf_uflow++;
        else if (expr.intvalue > acp->ac_u.acu_f.acuf_highbnd)
          acp->ac_u.acu_f.acuf_oflow++;
        else
          acp->ac_u.acu_f.acuf_freqs[expr.intvalue - acp->ac_u.acu_f.acuf_lowbnd]++;
        break;
      case ACT_FREQUENCYLEAD:
        expr = evaltree (acp->ac_expr1);
frequencylead:
        {
                unsigned idx;
                struct value_array *arr = (value_array*)expr.array;
                assert(expr.type == VAL_INT_ARR);
                for (idx = 0; idx < 13; idx++) {
                        if (arr->value[idx] < 0)
                                continue;
                        frequency2dout(acp, arr->value[idx], idx);
                }
        }
        free(expr.array);
        break;
      case ACT_FREQUENCY2D:
        expr = evaltree (acp->ac_expr1);
        expr2 = evaltree (acp->ac_expr2);
        frequency2dout(acp, expr.intvalue, expr2.intvalue);
        break;
      }
    }
}

int DEFUN(deal_main) (struct globals *g) {

  assert(0x107f == bitpermutate(0x0ff0));
  assert(0xfe02 == bitpermutate(0xfe01));
  assert(0xf061 == bitpermutate(0xf058));
  assert(0x017f == bitpermutate(0x00ff));

  assert(0 == popcount(0x0000));
  assert(1 == popcount(0x0100));
  assert(2 == popcount(0x1001));
  assert(16 == popcount(0xffff));
  assert(0 == popcountl(0x0000));
  assert(1 == popcountl(0x0100));
  assert(2 == popcountl(0x1001));
  assert(16 == popcountl(0xffff));
  assert(0 == popcountll(0x0000));
  assert(1 == popcountll(0x0100));
  assert(2 == popcountll(0x1001));
  assert(16 == popcountll(0xffff));

  gp = g;
  evaltreefunc = evaltree;
  hascard = statichascard;

  initprogram ();

  setup_action ();

  if (g->progressmeter)
    fprintf (stderr, "Calculating...  0%% complete\r");

  switch (gp->computing_mode) {
    case STAT_MODE:
    {
      g->shuffle = DEFUN(shuffle_factory)(g);
      if (!g->shuffle) break;
      while (!DEFUN(shuffle_next_hand) (g->shuffle, &g->curboard, g)) {
        if (interesting ()) {
          action ();
          gp->nprod++;
          if (g->progressmeter) {
            if ((100 * gp->nprod / g->maxproduce) > 100 * (gp->nprod - 1) / g->maxproduce)
              fprintf (stderr, "Calculating... %2d%% complete\r",
               100 * gp->nprod / g->maxproduce);
          }
        }
      }
      DEFUN(shuffle_close)(g->shuffle);
      break;
    }
    case EXHAUST_MODE:
      {
        /* Special variables for exhaustive mode
           Exhaustive mode created by Francois DELLACHERIE, 01-1999  */

        /* the two players that have unknown cards */
        unsigned exh_player[2];
        exh_get2players (exh_player);

#if __BMI2__
        /* bit mask for all shuffled cards */
        hand exh_card_at_bit[1];
#else
        /* exh_card_at_bit[i] is the card pointed by the bit #i */
        card exh_card_at_bit[26];
#endif
        unsigned exh_vect_length;
        /* a exh_vectordeal is a binary *word* of at most 26 bits: there are at most
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
           A suitable exh_vectordeal is a vector deal with hamming weight equal to 13.
           For computing those vectors, we use a straightforward
           meet in the middle approach.  */
        unsigned exh_vectordeal = exh_map_cards (&g->curboard, exh_player, &exh_vect_length, exh_card_at_bit);
        unsigned end = 1u << exh_vect_length;
        for (; exh_vectordeal < end;
            exh_vectordeal = exh_shuffle(&g->curboard, exh_player, exh_vectordeal, exh_card_at_bit)) {

          assert(hand_count_cards(g->curboard.hands[exh_player[0]]) == 13);
          assert(hand_count_cards(g->curboard.hands[exh_player[1]]) == 13);

          gp->ngen++;
          if (interesting ()) {
            /*  exh_print_vector(hs, exh_player, exh_vectordeal, exh_vect_length); */
            action ();
            gp->nprod++;
          }
        }
      }
      break;
    default:
      fprintf (stderr, "Unrecognized computation mode...\n");
      exit (-1); /*NOTREACHED */
    }
  if (g->progressmeter)
    fprintf (stderr, "                                      \r");
  return 0;
}
