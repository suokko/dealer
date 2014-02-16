
#pragma once

#include <stdint.h>
#include <assert.h>

/* Card is a 64 bit integer where each two bytes is one suit.
 * That makes a hand simple bitwise-or of all cards in the hand.
 * Board for compuation contains 4*64=256 bits in 4 hand vectors.
 * Stored board is compressed board format only requiring 128bits
 * each board. Stored board and board can be quickly converted
 * from each other with a few bitwise operations.
 */

typedef uint64_t card;
typedef card hand;

struct board {
	hand hands[4];
} __attribute__  ((aligned(16)));

/* First hand tells if card is NS or EW, second hand tells which one from partnership */
struct stored_board {
	hand hands[2];
} __attribute__  ((aligned(16)));

/* pack for shuffling */
struct pack {
	card c[52];
};

/* Suit numbering */
enum suits {
	SUIT_CLUB = 0,
	SUIT_DIAMOND = 1,
	SUIT_HEART = 2,
	SUIT_SPADE = 3,
	SUIT_NT = 4,
	NSUITS = 4,
};

#define SUIT_WIDTH	16
#define SUIT_WIDTH_SHIFT 4
#define SUIT_WIDTH_MOD	(16 - 1)
#define CARDS_IN_SUIT	13

/* Some helper bit positions */
#define spade_shift	SUIT_SPADE*SUIT_WIDTH
#define heart_shift	SUIT_HEART*SUIT_WIDTH
#define diamond_shift	SUIT_DIAMOND*SUIT_WIDTH
#define club_shift	SUIT_CLUB*SUIT_WIDTH
static const int suit_shifts[] = {
	club_shift,
	diamond_shift,
	heart_shift,
	spade_shift,
};

/* Some helper masks */
#define single_suit_mask	(card)(((1ULL << 13) - 1))
#define spade_mask		(card)(single_suit_mask << spade_shift)
#define heart_mask		(card)(single_suit_mask << heart_shift)
#define diamond_mask		(card)(single_suit_mask << diamond_shift)
#define club_mask		(card)(single_suit_mask << club_shift)
#define all_suits_mask		(card)(spade_mask | heart_mask | diamond_mask | club_mask)

static const card suit_masks[] = {
	club_mask,
	diamond_mask,
	heart_mask,
	spade_mask,
};

/* Some helper functions for hands */
static inline int hand_count_cards(const hand h)
{
	return __builtin_popcountll(h);
}

/* Return index of most significant card bit set */
static inline int C_BITPOS(const card c)
{
	assert(hand_count_cards(c) >= 1);
	return 63 - __builtin_clzll(c);
}

static inline int C_SUIT(const card c)
{
	assert(hand_count_cards(c) == 1);
	return C_BITPOS(c) >> SUIT_WIDTH_SHIFT;
}

static inline int C_RANK(const card c)
{
	assert(hand_count_cards(c) == 1);
	return C_BITPOS(c) & SUIT_WIDTH_MOD;
}

static inline card MAKECARD(int suit, int rank)
{
	assert(suit >= SUIT_CLUB && suit <= SUIT_SPADE);
	assert(rank >= 0 && rank < CARDS_IN_SUIT);

	return 1ULL << (suit * SUIT_WIDTH + rank);
}

static inline card hand_has_card(const hand h, const card c)
{
	return h & c;
}

static inline card hand_extract_card(const hand h)
{
	return 1ULL << C_BITPOS(h);
}

static inline hand hand_remove_card(const hand h, const card c)
{
	return h & ~c;
}

static inline hand hand_add_card(const hand h, const card c)
{
	return h | c;
}

/* Conversion to and from stored board */

static inline void board_to_stored(struct stored_board *s, const struct board *b)
{
	s->hands[0] = b->hands[0] | b->hands[2];
	assert(hand_count_cards(s->hands[0]) == 26);
	s->hands[1] = b->hands[0] | b->hands[1];
	assert(hand_count_cards(s->hands[1]) == 26);
}

static inline void board_from_stored(struct board *b, const struct stored_board *s)
{
	hand ns = s->hands[0];
	b->hands[0] = s->hands[1] & ns;
	assert(hand_count_cards(b->hands[0]) == 13);
	b->hands[1] = s->hands[1] & ~ns;
	assert(hand_count_cards(b->hands[1]) == 13);
	b->hands[2] = ~s->hands[1] & ns;
	assert(hand_count_cards(b->hands[2]) == 13);
	b->hands[3] = (~s->hands[1] & ~ns) & all_suits_mask;
	assert(hand_count_cards(b->hands[3]) == 13);
}
