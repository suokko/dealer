
#pragma once

#include <stdint.h>
#include <assert.h>

#include "bittwiddle.h"

#include <x86intrin.h>

/* Card is a 64 bit integer where each two bytes is one suit.
 * That makes a hand simple bitwise-or of all cards in the hand.
 * Board for computation contains 4*64=256 bits in 4 hand vectors.
 * Stored board is compressed board format only requiring 128bits
 * each board. Stored board and board can be quickly converted
 * from each other with a few bitwise operations.
 */

typedef uint64_t card;
typedef card hand;

/// Set of hands used for computation
union board {
	hand hands[4];
};

/* First hand tells if card is NS or EW, second hand tells which one from partnership */
struct stored_board {
	hand hands[2];
};

/* pack for shuffling */
union pack {
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

/// The number of bits each suit has
#define SUIT_WIDTH	16
/// The bit shift required to convert bit index to a suit
#define SUIT_WIDTH_SHIFT 4
/// The bitwise and mask required to convert bit index to a rank
#define SUIT_WIDTH_MOD	(16 - 1)
/// How many bits are used in each suit
#define CARDS_IN_SUIT	13

/// The lowest bit index for spades
#define spade_shift	SUIT_SPADE*SUIT_WIDTH
/// The lowest bit index for hearts
#define heart_shift	SUIT_HEART*SUIT_WIDTH
/// The lowest bit index for diamonds
#define diamond_shift	SUIT_DIAMOND*SUIT_WIDTH
/// The lowest bit index for clubs
#define club_shift	SUIT_CLUB*SUIT_WIDTH
/// A helper to lookup required shift if suit is from a variable
static const int suit_shifts[] = {
	club_shift,
	diamond_shift,
	heart_shift,
	spade_shift,
};

/// Create a mask with 13 lowest bits set
#define single_suit_mask	(card)(((1ULL << 13) - 1))
/// Shift the suit mask to the position of spade suit
#define spade_mask		(card)(single_suit_mask << spade_shift)
/// Shift the suit mask to the position of heart suit
#define heart_mask		(card)(single_suit_mask << heart_shift)
/// Shift the suit mask to the position of diamond suit
#define diamond_mask		(card)(single_suit_mask << diamond_shift)
/// Shift the suit mask to the position of club suit
#define club_mask		(card)(single_suit_mask << club_shift)
/// Create a combined mask which has bits for all possible cards
#define all_suits_mask		(card)(spade_mask | heart_mask | diamond_mask | club_mask)
// Helper to lookup a suit mask if suit is from a variable
static const card suit_masks[] = {
	club_mask,
	diamond_mask,
	heart_mask,
	spade_mask,
};

/**
 * Count number of cards in a hand
 */
static inline int hand_count_cards(const hand h)
{
	return popcount(h);
}

/**
 * Convert card or hand to a bit index
 *
 * @return The index of most significant set bit
 */
static inline int C_BITPOS(const card c)
{
	assert(hand_count_cards(c) >= 1);
	return std::numeric_limits<card>::digits - clz(c) - 1;
}

/**
 * Convert card to the suit
 */
static inline int C_SUIT(const card c)
{
	assert(hand_count_cards(c) == 1);
	return C_BITPOS(c) >> SUIT_WIDTH_SHIFT;
}

/**
 * Convert card to the rank
 */
static inline int C_RANK(const card c)
{
	assert(hand_count_cards(c) == 1);
	return C_BITPOS(c) & SUIT_WIDTH_MOD;
}

/**
 * Convert a suit and a rank to a card
 */
static inline card MAKECARD(int suit, int rank)
{
	assert(suit >= SUIT_CLUB && suit <= SUIT_SPADE);
	assert(rank >= 0 && rank < CARDS_IN_SUIT);

	return card{1} << (suit * SUIT_WIDTH + rank);
}

/**
 * Check if a hand has a card
 */
static inline card hand_has_card(const hand h, const card c)
{
	return h & c;
}

/**
 * Extract the most significant card from the hand
 */
static inline card hand_extract_card(const hand h)
{
	return card{1} << C_BITPOS(h);
}

/**
 * Remove a given card from the hand
 */
static inline hand hand_remove_card(const hand h, const card c)
{
	return h & ~c;
}

/**
 * Add a card to the hand
 */
static inline hand hand_add_card(const hand h, const card c)
{
	return h | c;
}

/**
 * Compress a board to the 128bit representation
 */
static inline void board_to_stored(struct stored_board *s, const union board *b)
{
	// First 64bits has set bit if north or south has the card
	s->hands[0] = b->hands[0] | b->hands[2];
	assert(hand_count_cards(s->hands[0]) == 26);
	// Second 64 bits has set bit if north or east has the card
	s->hands[1] = b->hands[0] | b->hands[1];
	assert(hand_count_cards(s->hands[1]) == 26);
}

/**
 * Decompress a board from the 128bit representation
 */
static inline void board_from_stored(union board *b, const struct stored_board *s)
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
