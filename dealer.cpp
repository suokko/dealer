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

#include <algorithm>
#include <limits>

#if 1
#define debugf noop
#else
/**
 * Trace eval_tree and value operations
 */
#define debugf printf
#endif

/**
 * Do nothing function to replace printf if not wanting debug prints
 */
template<typename... Args>
void noop(Args... args) {}

namespace DEFUN() {

/**
 * Implement dynamic value typing for scripts. It supports 32bit integers and
 * an associative table mapping each opening lead card to tricks taken by
 * declarer.
 *
 * Implementation enforces unique ownership to underlying data. This requirement
 * helps memory management to avoid leeks from pointer to array which may have
 * to be copied.
 *
 * @TODO: Convert to template type with evaltree using static typing
 */
struct value {
    /**
     * Construct an integer typed value
     */
    value(int v = 0);
    /**
     * Construct an array typed value
     */
    value(value_array *varr);
    /**
     * Create a copy from a variable
     */
    value(treebase *tb);

    /**
     * Assignment takes ownership of underlying data
     */
    value& operator=(value&& other);
    /**
     * Move constructor takes ownership of underlying data
     */
    value(value&& other);

    /**
     * Destructor makes sure underlying data is freed if there is an owned
     * pointer
     */
    ~value();

    /**
     * Helper to check if content is a pointer to array or an integer
     */
    inline bool is_array() const;

    /// Access the key information (card) from the array
    inline int key(unsigned index) const;

    /**
     * Apply a function object to all underlying values. If underlying value is
     * an integer then visit calls fn once with an integer value. If underlying
     * value is an array then visit calls fn one to thirteen times with an
     * integer value.
     *
     * @param fn Function object used to process stored value(s)
     * @return Reference to this object
     */
    template<typename fn_t>
    inline const value& visit(fn_t fn) const;

    /**
     * Use a function object to modify all underlying values. It calls fn for
     * each underlying value and returns the modified value object.
     *
     * @param fn Function object which modifies stored value(s)
     * @return The modified value object after the transform operation
     */
    template<typename fn_t>
    inline value transform(fn_t fn);

    /**
     * Use a function object to modify two value objects. It calls fn with two
     * arguments which match to value from this object and o object.
     *
     * If this or o object are different types then scalar value stays constant
     * while array value changes for each call to fn
     *
     * @param o Other value object which provides values to the second argument
     * @param fn Function object which modifies stored value(s)
     * @return The modified value after the transform operation.
     */
    template<typename fn_t>
    inline value transform(value& o, fn_t fn);

    /**
     * Allow conversion to treebase pointer which is used to store script
     * variable values.
     */
    explicit inline operator treebase*();
    /**
     * Convert the underlying value to bool assuming all values must be true to
     * evaluate the array as true.
     */
    explicit inline operator bool() const;
    /**
     * Calculate an average integer value
     */
    explicit inline operator int() const;
    /**
     * Calculate an average floating point value
     */
    explicit inline operator double() const;

    /** Arithmetic addition
     * @return value object takes ownership of underlying array
     */
    inline value operator+(value& o);
    /** Arithmetic subtraction
     * @return value object takes ownership of underlying array
     */
    inline value operator-(value& o);
    /** Arithmetic multiplication
     * @return value object takes ownership of underlying array
     */
    inline value operator*(value& o);
    /** Arithmetic division
     *
     * Division operation checks for division by zero. It returns INT_MAX, INT_MIN
     * or zero if denominator is zero. Return value depends on numerator.
     *
     * @return value object takes ownership of underlying array
     */
    inline value operator/(value& o);
    /** Arithmetic modulo
     *
     * Modulo operation checks for division by zero. It return zero if
     * denominator is zero.
     *
     * @return value object takes ownership of underlying array
     */
    inline value operator%(value& o);

    /** Compare equality
     * @return value object takes ownership of underlying array
     */
    inline value operator==(value& o);
    /** Compare inequality
     * @return value object takes ownership of underlying array
     */
    inline value operator!=(value& o);
    /** Compare less than
     * @return value object takes ownership of underlying array
     */
    inline value operator<(value& o);
    /** Compare less than or equal
     * @return value object takes ownership of underlying array
     */
    inline value operator<=(value& o);
    /** Compare greater than
     * @return value object takes ownership of underlying array
     */
    inline value operator>(value& o);
    /** Compare greater than or equal
     * @return value object takes ownership of underlying array
     */
    inline value operator>=(value& o);
    /** Apply logical not
     * @return value object takes ownership of underlying array
     */
    inline value operator!();

private:
    /**
     * Integer values are shifted one up and lowest bit is set. This allows code
     * later to check the lowest bit for type of value.
     */
    union {
        intptr_t value_;
        value_array *varray_;
    };
};

value::value(int v) :
    value_{v << 1 | 1}
{
    debugf("%s %d\n", __func__, v);
}

value::value(value_array *varr) :
    varray_{varr}
{
    debugf("%s %p\n", __func__, varr);
    assert((value_ & 1) == 0);
}

value::~value()
{
    debugf("%s %p\n", __func__, varray_);
    if (is_array())
        free(varray_);
}

bool value::is_array() const
{
    return (value_ & 1) == 0;
}

template<typename fn_t>
const value& value::visit(fn_t fn) const
{
    if (is_array()) {
        size_t end = std::find(std::begin(varray_->key), std::end(varray_->key), 0)
            - std::begin(varray_->key);
        std::for_each(std::begin(varray_->value), std::begin(varray_->value) + end, fn);
    } else
        fn(value_ >> 1);

    return *this;
}

template<typename fn_t>
value value::transform(fn_t fn)
{
    debugf("%s %p\n", __func__, varray_);
    if (is_array()) {
        size_t end = std::find(std::begin(varray_->key), std::end(varray_->key), 0)
            - std::begin(varray_->key);
        std::transform(std::begin(varray_->value), std::begin(varray_->value) + end,
                std::begin(varray_->value), fn);
    } else {
        value_ = fn(value_ >> 1) << 1 | 1;
    }

    return std::move(*this);
}

template<typename fn_t>
value value::transform(value &o, fn_t fn)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);

    if (o.is_array()) {
        if (!is_array()) {
            int temp = value_ >> 1;
            varray_ = (value_array*)malloc(sizeof(*varray_));
            std::uninitialized_fill(std::begin(varray_->value), std::end(varray_->value), temp);
            std::uninitialized_copy(std::begin(o.varray_->key), std::end(o.varray_->key), std::begin(varray_->key));
        }
        size_t end1 = std::find(std::begin(varray_->key), std::end(varray_->key), 0)
            - std::begin(varray_->key);
        size_t end2 = std::find(std::begin(o.varray_->key), std::end(o.varray_->key), 0)
            - std::begin(o.varray_->key);
        std::transform(std::begin(varray_->value), std::begin(varray_->value) + std::min(end1, end2),
                std::begin(o.varray_->value),
                std::begin(varray_->value), fn);
    } else if (is_array()) {
        size_t end = std::find(std::begin(varray_->key), std::end(varray_->key), 0)
            - std::begin(varray_->key);
        std::transform(std::begin(varray_->value), std::begin(varray_->value) + end,
                std::begin(varray_->value),
                [&fn, &o](int value) {return fn(value, o.value_ >> 1);} );
    } else {
        value_ = fn(value_ >> 1, o.value_ >> 1) << 1 | 1;
    }

    return std::move(*this);
}

value::value(treebase *tb) :
    value_{reinterpret_cast<intptr_t>(tb)}
{
    if (is_array()) {
      varray_ = (value_array*)malloc(sizeof(*varray_));
      memcpy(varray_, tb, sizeof(*varray_));
    }
    debugf("%s %p -> %p\n", __func__, tb, varray_);
}

value& value::operator=(value&& other)
{
    std::swap(value_, other.value_);
    debugf("%s %p\n", __func__, varray_);
    return *this;
}

value::value(value &&other) :
    value_{0}
{
    std::swap(value_, other.value_);
    debugf("%s %p\n", __func__, varray_);
}

int value::key(unsigned index) const
{
    assert(is_array());
    debugf("%s %p\n", __func__, varray_);

    return varray_->key[index];
}


value::operator treebase*()
{
    debugf("%s %p\n", __func__, varray_);
    treebase* rv = reinterpret_cast<treebase*>(value_);
    // Make sure we don't free pointer in destructor;
    value_ &= 1;
    return rv;
}

value::operator bool() const
{
    debugf("%s %p\n", __func__, varray_);
    bool rv = true;
    visit([&rv](int value) { rv = rv && value; });
    return rv;
}

value::operator int() const
{
    debugf("%s %p\n", __func__, varray_);
    int rv = 0;
    unsigned cnt = 0;
    visit([&rv,&cnt](int value) { rv += value; ++cnt;});
    return rv/cnt;
}

value::operator double() const
{
    debugf("%s %p\n", __func__, varray_);
    double rv = 0;
    unsigned cnt = 0;
    visit([&rv,&cnt](int value) { rv += value; ++cnt;});
    return rv/cnt;
}

value value::operator+(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o + *this;
    return transform(o, [](int a, int b) {return a + b;});
}

value value::operator-(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    return transform(o, [](int a, int b) {return a - b;});
}

value value::operator*(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o * *this;
    return transform(o, [](int a, int b) {return a * b;});
}

value value::operator/(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    return transform(o, [](int a, int b) {
            if (b==0)
                return a > 0 ? std::numeric_limits<decltype(a)>::max() :
                        a < 0 ? std::numeric_limits<decltype(a)>::min() : 0;
            return a / b;});
}

value value::operator%(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    return transform(o, [](int a, int b) {if (b == 0) return 0; return a % b;});
}

value value::operator==(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o == *this;
    return transform(o, [](int a, int b) {return a == b;});
}

value value::operator!=(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o != *this;
    return transform(o, [](int a, int b) {return a != b;});
}

value value::operator<(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o >= *this;
    return transform(o, [](int a, int b) {return a < b;});
}

value value::operator<=(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o > *this;
    return transform(o, [](int a, int b) {return a <= b;});
}

value value::operator>(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o <= *this;
    return transform(o, [](int a, int b) {return a > b;});
}

value value::operator>=(value &o)
{
    debugf("%s %p %p\n", __func__, varray_, o.varray_);
    if (!is_array() && o.is_array())
        return o < *this;
    return transform(o, [](int a, int b) {return a >= b;});
}

value value::operator!()
{
    debugf("%s %p\n", __func__, varray_);
    return transform([](int a) {return !a;});
}

/* Global variables */

/**
 * Cache expensive computation for scripts. (e.g. hcp)
 */
static struct handstat hs[4];

/* Function definitions */
static int true_dd (const union board *d, int l, int c); /* prototype */

static struct globals *gp;

/**
 * Initialize evalcontatract action
 */
static void initevalcontract () {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      for (k = 0; k < 14; k++)
        gp->results[i][j][k] = 0;
}

/**
 * Cache double dummy results. If no cached value for the hand then it call
 * true_dd() to run the double dummy solver.
 *
 * @param d Board used for double dummy solving
 * @param l Declarer's compass direction
 * @param c Contract denomination to solve
 * @return The number of tricks declarer can take
 */
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

/**
 * Run double dummy solver for all cards in the opening lead hand.
 *
 * @param d Board used for double dummy solving
 * @param l Declarer's compass direction
 * @param c Contract denomination to solve
 * @return Array of tricks each lead would allow declarer to take
 */
static struct value lead_dd (const union board *d, int l, int c) {
  char res[13];
  struct value_array *arr = (value_array *)mycalloc(1, sizeof(*arr));
  unsigned idx, fill = 0;
  memset(res, -1, sizeof(res));
  card hand = gptr->predealt.hands[(l+1) % 4];
  // Validate that script provides 13 cards for opening lead hand
  if (hand_count_cards(hand) != 13) {
    char errmsg[512];
    snprintf(errmsg, sizeof(errmsg),
        "Opening lead hand (%s) doesn't have 13 cards defined with predeal when calling leadtricks(%s, %s).\n"
        "Compass parameter to leadtriks function is the declarer of hand like tricks function.\n",
        player_name[(l+1) % 4], player_name[l], suit_name[c]);
    error(errmsg);
  }
  // Run the double dummy solver
  solveLead(d, l, c, hand, res);
  // Convert results to associative array
  for (idx = 0; idx < sizeof(res); idx++) {
    // Results are set from the highest bit index to the lowest
    card c = hand_extract_card(hand);
    hand &= ~c;
    // If there is -1 tricks it means the card doesn't have solution. It is
    // caused by libdds solving only one of equal cards.
    if (res[idx] == -1)
      continue;
    // Convert card to the bit position index
    arr->key[fill] = C_BITPOS(c);
    arr->value[fill] = res[idx];
    fill++;
  }
  for (; fill < sizeof(res); fill++)
    arr->value[fill] = -1;
  return {arr};
}

/**
 * Read double dummy results from library.dat
 * @param pn The compass of declarer
 * @param dn The contract denomination
 * @return The number of tricks taken by declarer
 */
static int get_tricks (int pn, int dn) {
  int tk = gp->libtricks[dn];
  int resu;
  resu = (pn ? (tk >> (4 * pn)) : tk) & 0x0F;
  return resu;
}

/**
 * Run double dummy solver or read result from library.dat provided data.
 * @param d The board to analyze
 * @param l The compass of declarer
 * @param c The contract denomination
 * @return The number of tricks declarer can take
 */
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

/**
 * Collect double dummy results for evalcontract action.
 */
static void evalcontract () {
  int s;
  for (s = 0; s < 5; s++) {
    gp->results[1][s][dd (&gp->curboard, 2, s)]++;      /* south declarer */
    gp->results[0][s][dd (&gp->curboard, 0, s)]++;      /* north declarer */
  }
}

/**
 * Check if shape function has the nr shape index bit set
 * @param nr The index of hand shape to check
 * @param s The shape function to check
 * @return 1 if the shape bit is set
 */
static int checkshape (unsigned nr, struct shape *s)
{
  unsigned idx = nr / 32;
  unsigned bit = nr % 32;
  int r = (s->bits[idx] & (1 << bit)) != 0;
  return r;
}

/**
 * Check if player has a card in their hand
 */
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

/**
 * Calculate and cache control points for a player
 *
 * @param d The board to analyze
 * @param hsbase The cache structure
 * @param compass The player who's controls to calculate
 * @param suit The suit to calculate controls, if 4 the all suits
 * @return The total control points
 */
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
 * Calculate suit length for a player.
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
 * Calculate shape index for a player. Shape index is used to check shape
 * functions in scripts.
 *
 * @param d The board to analyze
 * @param hsbase The cache
 * @param compass The player to analyze
 * @return
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

/**
 * Count losers
 *
 * @param d The board to analyze
 * @param hsbase The cache
 * @param compass The player to analyze
 * @param suit The suit to analyze. If 4 then analyze all suits
 */
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
              assert (control <= 2 && "Control count for a singleton should be at most 2");
              hs->hs_loser[suit*2+1] = losers[control];
              break;
            }
          case 2:
            {
              /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
              int losers[] = {2, 1, 1, 0};
              assert (control <= 3 && "Control count for a doubleton should be at most 3");
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

/**
 * Initialize point count and cache variables
 */
static void initprogram (void)
{
  initpc();

  /* clear the handstat cache */
  memset(hs, -1, sizeof(hs));
}

/* Specific routines for EXHAUST_MODE */

/**
 * Print statistics for a hand
 */
static inline void exh_print_stats (struct handstat *hs, int hs_length[4]) {
  int s;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    printf ("  Suit %d: ", s);
    printf ("Len = %2d, Points = %2d\n", hs_length[s], hs->hs_points[s*2+1]);
  }
  printf ("  Totalpoints: %2d\n", hs->hs_points[s*2+1]);
}

/**
 * Print cards shuffled in exhaust mode
 */
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

/**
 * Prints state of hands which are shuffled in exhaust mode
 */
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

/**
 * Return score for a given number of tricks which can be an array of tricks
 *
 * @param vuln Vulnerability used for scoring
 * @param suit The denomination of contract
 * @param level The level of contract
 * @param dbl Is contract doubled
 * @param tricks Tricks taken in the contract
 * @return Score for the result
 */
static struct value score (int vuln, int suit, int level, int dbl, struct value& tricks) {
  return tricks.transform([&](int value) {
        return scoreone(vuln, suit, level, dbl, value);
      });
}

/**
 * Evaluate a script
 *
 * @param b The script in AST format
 * @param shuffle The shuffling state
 * @return The results of tree evaluation
 */
static struct value evaltree (struct treebase *b, std::unique_ptr<shuffle> &shuffle) {
  struct tree *t = (struct tree*)b;
  debugf("%s %d\n", __func__, b->tr_type);
  switch (b->tr_type) {
    default:
      assert (0);
    case TRT_NUMBER:
      return {t->tr_int1};
    case TRT_VAR:
      if (gp->ngen != t->tr_int1) {
        value r = evaltree(t->tr_leaf1, shuffle);
        t->tr_int1 = gp->ngen;
        t->tr_leaf2 = static_cast<treebase*>(r);
      }
      return {t->tr_leaf2};
    case TRT_AND2:
      {
        value r = evaltree(t->tr_leaf1, shuffle);
        if (!static_cast<bool>(r))
          return static_cast<bool>(r);
        return static_cast<bool>(evaltree(t->tr_leaf2, shuffle));
      }
    case TRT_OR2:
      {
        value r = evaltree(t->tr_leaf1, shuffle);
        if (r)
          return static_cast<bool>(r);
        return static_cast<bool>(evaltree(t->tr_leaf2, shuffle));
      }
    case TRT_ARPLUS:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) + rho;
      }
    case TRT_ARMINUS:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) - rho;
      }
    case TRT_ARTIMES:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) * rho;
      }
    case TRT_ARDIVIDE:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) / rho;
      }
    case TRT_ARMOD:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) % rho;
      }
    case TRT_CMPEQ:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) == rho;
      }
    case TRT_CMPNE:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) != rho;
      }
    case TRT_CMPLT:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) < rho;
      }
    case TRT_CMPLE:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) <= rho;
      }
    case TRT_CMPGT:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) > rho;
      }
    case TRT_CMPGE:
      {
        value rho = evaltree(t->tr_leaf2, shuffle);
        return evaltree(t->tr_leaf1, shuffle) >= rho;
      }
    case TRT_NOT:
      return !evaltree(t->tr_leaf1, shuffle);
    case TRT_LENGTH:      /* suit, compass */
      assert (t->tr_int1 >= SUIT_CLUB && t->tr_int1 <= SUIT_SPADE);
      assert (t->tr_int2 >= COMPASS_NORTH && t->tr_int2 <= COMPASS_WEST);
      return {staticsuitlength(&gp->curboard, t->tr_int2, t->tr_int1)};
    case TRT_HCPTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return {hcp(&gp->curboard, hs, t->tr_int1, 4)};
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
      return {getpc(idxTens + (b->tr_type - TRT_PT0TOTAL) / 2, gp->curboard.hands[t->tr_int1])};
    case TRT_HCP:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return {hcp(&gp->curboard, hs, t->tr_int1, t->tr_int2)};
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
      return {getpc(idxTens + (b->tr_type - TRT_PT0) / 2, gp->curboard.hands[t->tr_int1] & suit_masks[t->tr_int2])};
    case TRT_SHAPE:      /* compass, shapemask */
      {
        struct treeshape *s = (struct treeshape *)b;
        assert (s->compass >= COMPASS_NORTH && s->compass <= COMPASS_WEST);
        return {checkshape(distrbit(&gp->curboard, hs, s->compass), &s->shape)};
      }
    case TRT_HASCARD:      /* compass, card */
      {
        struct treehascard *hc = (struct treehascard *)t;
        assert (hc->compass >= COMPASS_NORTH && hc->compass <= COMPASS_WEST);
        return {statichascard (&gp->curboard, hc->compass, hc->c) > 0};
      }
    case TRT_LOSERTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return {loser(&gp->curboard, hs, t->tr_int1, 4)};
    case TRT_LOSER:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return {loser(&gp->curboard, hs, t->tr_int1, t->tr_int2)};
    case TRT_CONTROLTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return {control(&gp->curboard, hs, t->tr_int1, 4)};
    case TRT_CONTROL:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return {control(&gp->curboard, hs, t->tr_int1, t->tr_int2)};
    case TRT_CCCC:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return {cccc (t->tr_int1)};
    case TRT_QUALITY:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return {quality (t->tr_int1, t->tr_int2)};
    case TRT_IF:
      assert (t->tr_leaf2->tr_type == TRT_THENELSE);
      {
        struct tree *leaf2 = (struct tree *)t->tr_leaf2;
        return (evaltree(t->tr_leaf1, shuffle) ? evaltree (leaf2->tr_leaf1, shuffle) :
              evaltree (leaf2->tr_leaf2, shuffle));
      }
    case TRT_TRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      return {dd (&gp->curboard, t->tr_int1, t->tr_int2)};
    case TRT_LEADTRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      return lead_dd (&gp->curboard, t->tr_int1, t->tr_int2);
    case TRT_SCORE:      /* vul/non_vul, contract, tricks in leaf1 */
      assert (t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
      {
        int cntr = t->tr_int2 & (64 - 1);
        value tricks = evaltree (t->tr_leaf1, shuffle);
        return score (t->tr_int1, cntr % 5, cntr / 5, t->tr_int2 & 64, tricks);
      }
    case TRT_IMPS:
      return evaltree (t->tr_leaf1, shuffle).transform([](int value) {return imps(value);});
    case TRT_AVG:
      return {(int)(gptr->average*1000000)};
    case TRT_ABS:
      return evaltree (t->tr_leaf1, shuffle).transform([](int value) {return abs(value);});
    case TRT_RND:
      return evaltree(t->tr_leaf1, shuffle).transform([&shuffle](int value)
            {return shuffle->random32(value - 1);});
  }
}

/**
 * Check if the had passes the script condition
 */
static inline bool interesting (std::unique_ptr<shuffle> &shuffle) {
  debugf("hand: %d\n", gptr->ngen);
  return static_cast<bool>(evaltree(gp->decisiontree, shuffle));
}

/**
 * Initialize action states
 */
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
        acp->ac_double1 = 0;
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

#if __cplusplus >= 201702L
using std::clamp;
#else
template<typename T>
constexpr const T& clamp(const T& value, const T& low, const T& high)
{
    assert(low <= high && "Clamp requires low to be smaller or equal to high");
    return value < low ? low : value > high ? high : value;
}
#endif

/**
 * Add values to two dimensional histogram
 *
 * @param acp The histogram action
 * @param expr The first value
 * @param expr2 The second value
 */
static void frequency2dout(struct action *acp, int expr,  int expr2) {
        int val1, val2, high1 = 0, high2 = 0, low1 = 0, low2 = 0;

        high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
        high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
        low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
        low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
        // Make sure the value is in the range, Lowest and highest are for all
        // outside values in their respective direction.
        val1 = clamp(expr, low1 - 1, high1 + 1) - low1 + 1;
        val2 = clamp(expr2, low2 - 1, high2 + 1) - low2 + 1;

        acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * val1 + val2]++;
}

/**
 * Convert 1D frequency to 2D lead frequency
 */
static void frequency_to_lead(struct action *acp, struct value& val) {
        int low1 = acp->ac_u.acu_f.acuf_lowbnd;
        int high1 = acp->ac_u.acu_f.acuf_highbnd;
        int low2 = 0;
        int high2 = 12;

        free(acp->ac_u.acu_f.acuf_freqs);

        acp->ac_type = ACT_FREQUENCYLEAD;
        acp->ac_u.acu_f2d.acuf_lowbnd_expr1 = low1;
        acp->ac_u.acu_f2d.acuf_highbnd_expr1 = high1;
        acp->ac_u.acu_f2d.acuf_lowbnd_expr2 = low2;
        acp->ac_u.acu_f2d.acuf_highbnd_expr2 = high2;
        acp->ac_u.acu_f2d.acuf_freqs = (long *)mycalloc(
                        (high1 - low1 + 3) * (high2 - low2 + 3),
                        sizeof(long));
        card *h = (card*)mycalloc(high2 - low2 + 1, sizeof(card));
        for (;low2 <= high2; low2++) {
               card c = 1LL << val.key(low2);
               h[low2] = c;
        }
        acp->ac_expr2 = (treebase*)h;
}

/**
 * Execute script actions
 */
static void action (std::unique_ptr<shuffle> &shuffle) {
  struct action *acp;
  int expr, expr2;
  value maybelead;

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
          expr = static_cast<int>(evaltree (acp->ac_expr1, shuffle));
          printf ("%d\n", expr);
        }
        break;
      case ACT_PRINTONELINE:
        printoneline (&gp->curboard);
        if (acp->ac_expr1) {
          expr = static_cast<int>(evaltree (acp->ac_expr1, shuffle));
          printf ("%d\n", expr);
        }
        break;

      case ACT_PRINTES:
        { struct expr *pex = (struct expr *) acp->ac_expr1;
          while (pex) {
            if (pex->ex_tr) {
              expr = static_cast<int>(evaltree (pex->ex_tr, shuffle));
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
        {
            double v = static_cast<double>(evaltree(acp->ac_expr1, shuffle));
            acp->ac_double1 += (v - acp->ac_double1) / (gptr->nprod+1);
        }
        break;
      case ACT_FREQUENCY:
        maybelead = evaltree (acp->ac_expr1, shuffle);
        if (maybelead.is_array()) {
          frequency_to_lead(acp, maybelead);
          goto frequencylead;
        }
        expr = static_cast<int>(maybelead);
        if (expr < acp->ac_u.acu_f.acuf_lowbnd)
          acp->ac_u.acu_f.acuf_uflow++;
        else if (expr > acp->ac_u.acu_f.acuf_highbnd)
          acp->ac_u.acu_f.acuf_oflow++;
        else
          acp->ac_u.acu_f.acuf_freqs[expr - acp->ac_u.acu_f.acuf_lowbnd]++;
        break;
      case ACT_FREQUENCYLEAD:
        maybelead = evaltree (acp->ac_expr1, shuffle);
frequencylead:
        {
                assert(maybelead.is_array());
                unsigned idx = 0;
                maybelead.visit([&acp,&idx](int value) {
                    frequency2dout(acp, value, idx++);
                });
        }
        break;
      case ACT_FREQUENCY2D:
        expr = static_cast<int>(evaltree (acp->ac_expr1, shuffle));
        expr2 = static_cast<int>(evaltree (acp->ac_expr2, shuffle));
        frequency2dout(acp, expr, expr2);
        break;
    }
  }
}

/**
 * Complete actions after all hands has been produced
 */
static void cleanup_action (std::unique_ptr<shuffle> &shuffle) {
  struct action *acp;
  int player, i;

  for (acp = gptr->actionlist; acp != 0; acp = acp->ac_next) {
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
        showevalcontract (gptr->nprod);
        break;
      case ACT_PRINT:
        for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
          if (!(acp->ac_int1 & (1 << player)))
            continue;
          printf ("\n\n%s hands:\n\n\n\n", player_name[player]);
          for (i = 0; i < gptr->nprod; i += 4) {
            union board b[4];
            int j;
            for (j = 0; j < 4 && j < gptr->nprod - i; j++)
              board_from_stored(&b[j], &gptr->deallist[i + j]);
            printhands (i, b, player, gptr->nprod - i > 4 ? 4 : gptr->nprod - i);
          }
          printf ("\f");
        }
        break;
      case ACT_AVERAGE:
        gp->average = acp->ac_double1;
        if (acp->ac_expr2) {
          if (!evaltree(acp->ac_expr2, shuffle))
            break;
        }
        if (acp->ac_str1)
          printf ("%s: ", acp->ac_str1);
        printf ("%g\n", gp->average);
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
        free(acp->ac_u.acu_f.acuf_freqs);
        break;
      case ACT_FREQUENCYLEAD:
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
        else
          printf ("    ");
        for (j = 1; j < (high2 - low2) + 2; j++) {
          if (toprintcol[j] != 0) {
            if (acp->ac_type == ACT_FREQUENCY2D)
              printf (" %6d", j + low2 - 1);
            else {
              card * header = (card*)acp->ac_expr2;
              printf ("     ");
              printcard(header[j - 1]);
            }
          }
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
        if (acp->ac_type == ACT_FREQUENCYLEAD)
          free(acp->ac_expr2);
        free(acp->ac_u.acu_f2d.acuf_freqs);
        free(toprintcol);
        free(toprintrow);
      }
    }
  }
}

/**
 * Main loop for the dealer
 */
static int deal_main(struct globals *g) {
  // Quick test for bitops to make sure they are implement correctly for the
  // current cpu.
  assert(0 == popcount(char{0x00}));
  assert(1 == popcount(char{0x20}));
  assert(2 == popcount(char{0x30}));
  assert(8 == popcount(char{-1}));
  assert(0 == popcount(short{0x0000}));
  assert(1 == popcount(short{0x0100}));
  assert(2 == popcount(short{0x1001}));
  assert(16 == popcount(short{-1}));
  assert(0 == popcount(unsigned{0x0000}));
  assert(1 == popcount(unsigned{0x0100}));
  assert(2 == popcount(unsigned{0x1001}));
  assert(16 == popcount(unsigned{0xffff}));
  assert(0 == popcount(uint64_t{0x0000}));
  assert(1 == popcount(uint64_t{0x0100}));
  assert(2 == popcount(uint64_t{0x1001}));
  assert(16 == popcount(uint64_t{0xffff}));

  gp = g;

  initprogram ();

  setup_action ();

  auto shuffle = shuffle::factory(g);
  if (!shuffle) return EXIT_FAILURE;

  if (g->progressmeter)
    fprintf (stderr, "Calculating...  0%% complete\r");

  while (!shuffle->do_shuffle(&g->curboard, g)) {
    if (interesting (shuffle)) {
      action (shuffle);
      gp->nprod++;
      if (g->progressmeter) {
        if ((100 * gp->nprod / g->maxproduce) > 100 * (gp->nprod - 1) / g->maxproduce)
          fprintf (stderr, "Calculating... %2d%% complete\r",
              100 * gp->nprod / g->maxproduce);
      }
    }
  }

  if (g->progressmeter)
    fprintf (stderr, "                                      \r");

  cleanup_action(shuffle);

  return 0;
}

/**
 * Register the optimization alternative for the main loop
 */
auto dm_register = make_entry_register(::deal_main, deal_main);

} // namespace DEFUN()
