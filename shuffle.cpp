
#include "card.h"
#include "dealer.h"
#include "genlib.h"
#include "pregen.h"
#include "shuffle.h"
#include "uniform_int.h"
#include "tree.h"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <pcg_random.hpp>
#include <random>

#include <vector>

#include <iostream>
#include <iomanip>

#include <boost/multiprecision/cpp_int.hpp>

namespace mp = boost::multiprecision;

#if __cplusplus >= 202002L
#include <bit>
#else
#include <limits>
#endif

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



namespace DEFUN() {

shuffle::shuffle(globals *)
{}

shuffle::~shuffle()
{}

struct random_device : public std::random_device {
    random_device(std::seed_seq &&) :
        std::random_device{}
    {}

    random_device& operator=(const random_device&)
    { return *this; }

    using copy_type = random_device&;
};

struct pcg : public pcg32 {
    pcg(std::seed_seq &&seq) :
        pcg32{seq}
    {
    }

    using copy_type = pcg;
};

template<typename rng_t>
struct shuffle_rng : public shuffle {
    shuffle_rng(globals *gp);
    uint32_t random32(const fast_uniform_int_distribution<uint32_t, false> &dist) override;

    static std::unique_ptr<shuffle> factory(globals* gp);

protected:
    rng_t rng_;
};

template<typename rng_t>
shuffle_rng<rng_t>::shuffle_rng(globals* gp) :
    shuffle(gp),
    rng_{std::seed_seq(gp->seed.begin(), gp->seed.begin() + gp->seed_provided)}
{}

template<typename rng_t>
uint32_t shuffle_rng<rng_t>::random32(const fast_uniform_int_distribution<uint32_t, false> &dist)
{
    return dist(rng_);
}

/**
 * Use deals from library.dat instead of random hands.
 */
template<typename rng_t>
struct load_library : public shuffle_rng<rng_t> {

    load_library(globals *gp);
    int do_shuffle(board *d, globals *gp) override;

    struct library_not_found {};
private:
    std::ifstream lib_;
};

/**
 * Generate all possible deals if two hands are completely know
 */
template<typename rng_t>
struct exhaust_mode : public shuffle_rng<rng_t> {
    exhaust_mode(globals *gp);

    int do_shuffle(board *d, globals *gp) override;

private:
    /// Indexes to players we are shuffling
    std::array<unsigned,2> players_;
    /** Bit vector state which is used to generate hands
     * a bitvector is a binary *word* of at most 26 bits: there are at most
     * 26 cards to deal between two players.  Each card to deal will be
     * affected to a given position in the vector.  Example : we need to deal
     * the 26 minor cards between north and south (what a double fit !!!)
     * Then those cards will be represented in a binary vector as shown below:
     *      Card    : DA DK ... D2 CA CK ... C4 C3 C2
     *      Bit-Pos : 25 24     13 12 11      2  1  0
     * The two players will be respectively represented by the bit values 0 and
     * 1. Let's say north gets the 0, and south the 1. Then the vector
     * 11001100000001111111110000 represents the following hands :
     *      North: D QJ8765432 C 5432
     *      South: D AKT9      C AKQJT9876
     * A suitable exh_vectordeal is a vector deal with hamming weight equal to 13.
     * For computing those vectors, we use a straightforward
     * meet in the middle approach.
     */
    unsigned bitvector_;
#if __BMI2__
    /// Bit mask mapping card positions in hand representation (pdep required)
    hand exh_card_at_bit_[1];
#else
    /// Helper to map bit position to a card bit
    card exh_card_at_bit_[26];
#endif

    static std::array<unsigned, 2> map_players(const globals *gp);

    static unsigned bitpermutate(unsigned vector);
};

/**
 * Find two hands which don't have all cards defined.
 * @return lookup table to player positions with unknown cards
 */
template<typename rng_t>
std::array<unsigned, 2> exhaust_mode<rng_t>::map_players(const globals *gp)
{
    std::array<unsigned, 2> exh_player;
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
    return exh_player;
}

template<typename rng_t>
exhaust_mode<rng_t>::exhaust_mode(globals* gp) :
    shuffle_rng<rng_t>{gp},
    players_{map_players(gp)}
{
    int i;
    int bit_pos;
    hand predeal = 0;
    hand p0 = 0;
    hand p1 = 0;
    unsigned exh_vectordeal = 0;
    board *b = &gp->curboard;
#if __BMI2__
    hand allbits = 0;
#endif

    for (i = 0; i < 4; i++) {
        predeal |= gp->predealt.hands[i];
        b->hands[i] = gp->predealt.hands[i];
    }

    /* Fill in all cards not predealt */
    hand shuffle = ~predeal & all_suits_mask;
    for (bit_pos = 0; shuffle; ++bit_pos) {
        // remove lowest set bit
        hand next_shuffle = (shuffle - 1) & shuffle;
        // Isolate the changed card
        card onecard = next_shuffle ^ shuffle;
        shuffle = next_shuffle;
#if __BMI2__
        allbits |= onecard;
#else
        exh_card_at_bit_[bit_pos] = onecard;
#endif
    }

    int p1cnt = 13 - hand_count_cards(gp->predealt.hands[players_[1]]);

    /* Set N lower bits that will be moved to high bits */
    exh_vectordeal = (1u << p1cnt) - 1;

    // Map cards to hands
#if __BMI2__
    exh_card_at_bit_[0] = allbits;

    p1 = _pdep_u64(exh_vectordeal, allbits);
    unsigned mask = (1u << bit_pos) - 1;
    p0 = _pdep_u64(~exh_vectordeal & mask, allbits);
#else
    for (i = 0; i < p1cnt; i++)
        p1 |= exh_card_at_bit_[i];
    for (; i < bit_pos; i++)
        p0 |= exh_card_at_bit_[i];
#endif

    // Check that we have 13 cards in each hand
    assert(hand_count_cards(p0 | b->hands[players_[0]]) == 13);
    assert(hand_count_cards(p1 | b->hands[players_[1]]) == 13);
    // Add cards to hands
    b->hands[players_[0]] |= p0;
    b->hands[players_[1]] |= p1;

    bitvector_ = exh_vectordeal;
    // Setup limits to number of deals
    gp->maxgenerate = ncrtable(bit_pos, p1cnt);
    gp->maxproduce = ncrtable(bit_pos, p1cnt);
    gp->ngen = gp->nprod = 0;
}

template<typename rng_t>
unsigned exhaust_mode<rng_t>::bitpermutate(unsigned vector)
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
    moveback >>= ctz(vector) + 1;
    /* combine the result vector to get next permutation */
    return nextvector | moveback;
}

template<typename rng_t>
int exhaust_mode<rng_t>::do_shuffle(board *b, globals *gp)
{
    if (gp->ngen >= gp->maxgenerate)
        return 1;

    gp->ngen++;

    hand bitstoflip = 0;

    // Generate next bitpermutation
    unsigned exh_vectordeal = bitpermutate(bitvector_);
    // Find out bit positions which changed
    unsigned changed = exh_vectordeal ^ bitvector_;

    // Convert changed bit positions to card bit mask
#if __BMI2__
    bitstoflip = _pdep_u64(changed, exh_card_at_bit_[0]);
#else
    do {
        unsigned last = ctz(changed);
        bitstoflip |= exh_card_at_bit_[last];
        changed &= changed - 1;
    } while (changed);
#endif

    bitvector_ = exh_vectordeal;

    // Apply changes to hands with unknown cards
    b->hands[players_[0]] ^= bitstoflip;
    b->hands[players_[1]] ^= bitstoflip;

    return 0;
}

/**
 * Swap east-west-south cards to generate different hands from same cards. Used
 * for command line arguments -2 and -3.
 */
template<typename parent>
struct shuffle_swap : public parent {
    shuffle_swap(globals* gp);
    int do_shuffle(board* d, globals* gp) override;
private:
    int swapindex_;
    int swapbound_;
};

/**
 * Check production and generation limits.
 */
template<typename parent>
struct limit final : public parent {
    limit(globals* gp) : parent(gp)
    {
        // Zero loop counters. They are globals because other parts of code
        // depend on them.
        gp->ngen = gp->nprod = 0;
    }
    int do_shuffle(board* d, globals* gp) override
    {
        // Check if we need to generate more hands
        if (gp->ngen >= gp->maxgenerate ||
                gp->nprod >= gp->maxproduce)
            return 1;
        // Call the shuffle implementation
        int rv = parent::do_shuffle(d, gp);
        if (!rv)
            gp->ngen++;
        return rv;
    }
};

/**
 * Generate random hands with no constraints like fixed cards or defined
 * shapes.
 */
template<typename rng_t>
struct random_hand : public shuffle_rng<rng_t> {
    random_hand(globals *gp);
    int do_shuffle(board *d, globals *gp) override;
protected:
    using rng_copy_t = typename rng_t::copy_type;
    rng_copy_t shuffle_pack(rng_copy_t rng,
                            hand &hand,
                            unsigned &cardsleft,
                            unsigned cards_to_deal,
                            bool last);

    pack pack_;
};

template<typename rng_t>
random_hand<rng_t>::random_hand(globals* gp) :
    shuffle_rng<rng_t>(gp)
{
    newpack(&pack_, gp->initialpack);
}

template<typename rng_t>
int random_hand<rng_t>::do_shuffle(board *d, globals *)
{
    unsigned cardsleft = 52;
    unsigned cards_per_player = 13;
    board deal{{0}};

    hand temp{0};

    this->rng_ = shuffle_pack(this->rng_, temp, cardsleft, cardsleft, true);

    for (unsigned c = 0; c < cards_per_player; c++) {
        card *in = &pack_.c[c*4];
        deal.hands[0] = hand_add_card(deal.hands[0], in[0]);
        deal.hands[1] = hand_add_card(deal.hands[1], in[1]);
        deal.hands[2] = hand_add_card(deal.hands[2], in[2]);
        deal.hands[3] = hand_add_card(deal.hands[3], in[3]);
    }

    *d = deal;
    return 0;
}

template<typename rng_t>
auto random_hand<rng_t>::shuffle_pack(typename rng_t::copy_type rng,
                                      hand &hand,
                                      unsigned &cardsleft,
                                      unsigned cards_to_deal,
                                      bool last) -> rng_copy_t
{
    unsigned min = cardsleft - cards_to_deal;
    if (last)
        min++;

    do {
        fast_uniform_int_distribution<unsigned> dist(0, --cardsleft);
        const auto pos = dist(rng);
        hand |= pack_.c[pos];
        std::swap(pack_.c[pos], pack_.c[cardsleft]);
    } while (cardsleft > min);

    if (last)
        hand |= pack_.c[0];

    return rng;
}

/**
 * Deal random hands with fixed cards
 */
template<typename rng_t>
struct predeal_cards : public random_hand<rng_t> {
    predeal_cards(globals* gp);
    int do_shuffle(board* d, globals* gp) override;
protected:
    using cardsleft_t = std::array<unsigned, 4>;
    using player_map_t = std::array<unsigned, 4>;

    cardsleft_t cardsleft_;
    player_map_t player_map_;
    unsigned totalleft_;
    unsigned player_end_;
};

template<typename rng_t>
predeal_cards<rng_t>::predeal_cards(globals* gp) :
    random_hand<rng_t>(gp),
    cardsleft_{
        13u - hand_count_cards(gp->predealt.hands[0]),
        13u - hand_count_cards(gp->predealt.hands[1]),
        13u - hand_count_cards(gp->predealt.hands[2]),
        13u - hand_count_cards(gp->predealt.hands[3])
    },
    totalleft_{cardsleft_[0] + cardsleft_[1] + cardsleft_[2] + cardsleft_[3]}
{
    // Combine all predealt cards together
    hand predealt = std::accumulate(std::begin(gp->predealt.hands), std::end(gp->predealt.hands),
            hand{0}, std::bit_or<hand>());
    // Move all predealt cards to the end
    std::partition(std::begin(this->pack_.c), std::end(this->pack_.c),
            [predealt] (const card& c) { return !hand_has_card(predealt, c); });

    player_end_ = 0;
    for (unsigned player = 0; player < 4; player++)
        if (cardsleft_[player] > 0)
            player_map_[player_end_++] = player;
}

template<typename rng_t>
int predeal_cards<rng_t>::do_shuffle(board* d, globals* gp)
{
    if (!player_end_) {
        *d = gp->predealt;
        return 0;
    }

    unsigned player = 0;
    unsigned pos = 0;
    typename rng_t::copy_type rng = this->rng_;
    board deal{gp->predealt};
    unsigned sum = totalleft_;
    // Shuffle cards which aren't set to a specific hand
    for (; pos < player_end_ - 1; pos++) {
        player = player_map_[pos];
        rng = this->shuffle_pack(rng, deal.hands[player],
                                 sum, cardsleft_[player], false);
    }

    player = player_map_[pos];
    rng = this->shuffle_pack(rng, deal.hands[player],
                             sum, cardsleft_[player], true);

    this->rng_ = rng;
    // Store board to memory
    *d = deal;
    return 0;
}

using uint96_t = mp::number<mp::cpp_int_backend<96, 96, mp::unsigned_magnitude, mp::unchecked, void>>;

struct shape_probability {
    shape_probability() :
       combinations_{1},
       shapes_{0}
    {}

    shape_probability& operator*=(unsigned combinations)
    {
        combinations_ *= combinations;
        return *this;
    }

    shape_probability operator+(const shape_probability &o) const
    {
        shape_probability rv{o};
        rv.combinations_ += combinations_;
        return rv;
    }

    shape_probability& add_suit(unsigned player, unsigned suit, unsigned length)
    {
        assert(length <= 13 && "Suit length must be at most 13");
        assert(player < 4 && "There is at most 4 hands in a deal");
        assert(suit < 4 && "There is at most 4 suits in a hand");

        unsigned shift = (player * 4 + suit) * 4;
        shapes_ |= static_cast<uint64_t>(length) << shift;
        return *this;
    }

    struct depth_shape {
        depth_shape(uint64_t shape) : shape_{shape} {}
        uint64_t operator[](unsigned suit) const
        { return (shape_ >> (4*suit)) & 0xf; }

        operator bool() const
        { return shape_ & 0xffff; }
    private:
        uint64_t shape_;
    };

    depth_shape operator[](unsigned player)
    { return {shapes_ >> (4*4*player)}; }

    explicit operator uint96_t() const
    { return combinations_; }

    friend std::ostream& operator<<(std::ostream&, const shape_probability&);
private:
    uint96_t combinations_;
    uint64_t shapes_;
};

bool operator<(const shape_probability &a, const uint96_t &b)
{
    return static_cast<uint96_t>(a) < b;
}

bool operator<(const uint96_t &a, const shape_probability &b)
{
    return a < static_cast<uint96_t>(b);
}


std::ostream& operator<<(std::ostream& os, const shape_probability& sp)
{
    auto flags = os.flags(os.basefield);
    os << std::hex << std::setw(16) <<  sp.shapes_ << ":"
        << std::setw(24) << sp.combinations_;
    os.setf(flags, os.basefield);
    return os;
}

/**
 * Deal random hands with potentially fixed cards and shape restrictions
 */
template<typename rng_t>
struct predeal_bias : public predeal_cards<rng_t> {
    predeal_bias(globals *gp);
    int do_shuffle(board *d, globals *gp) override;
protected:
    using suit_map_t = std::array<std::array<unsigned, 13>, 4>;
    using suit_end_t = std::array<unsigned, 4>;
    using rng_copy_t = typename rng_t::copy_type;

    /**
     * Build shape selection table
     */
    void build_shape_table(globals *gp);

    inline
    rng_copy_t shuffle_player(rng_copy_t rng,
                              hand &cards,
                              shape_probability::depth_shape shape,
                              suit_map_t &suit_map,
                              suit_end_t &suit_end,
                              unsigned &destination);

    std::vector<shape_probability> probabilities_;
    unsigned predeal_end_;
    unsigned free_cards_;
    unsigned bits_required_;
};

struct shape_builder {
    int depth_;
    bool all_players_;
    struct hand_type {
        std::array<int, 4> lengths_;
        std::array<int, 4> predeal_;
        int compass_;
        int slotsleft_;
    };
    std::array<hand_type, 4> players_;
    std::array<int, 4> total_predeal_;

    shape_builder(globals *gp) :
        depth_{0},
        players_{},
        total_predeal_{}
    {
        hand predealt = std::accumulate(std::begin(gp->predealt.hands),
                                        std::end(gp->predealt.hands),
                                        hand{0}, std::bit_or<hand>());

        std::generate(std::begin(total_predeal_), std::end(total_predeal_),
                      [&predealt]() {
                          unsigned rv = hand_count_cards(predealt & club_mask);
                          predealt >>= SUIT_WIDTH;
                          return rv;
                      });

        for (int player = 0; player < 4; player++) {
            if (!std::all_of(std::begin(gp->biasdeal[player]),
                             std::end(gp->biasdeal[player]),
                             [](int v) {return v == -1;})) {

                players_[depth_++] = hand_type{
                    {
                        gp->biasdeal[player][0],
                        gp->biasdeal[player][1],
                        gp->biasdeal[player][2],
                        gp->biasdeal[player][3]
                    },
                    {
                        hand_count_cards(gp->predealt.hands[player] & club_mask),
                        hand_count_cards(gp->predealt.hands[player] & diamond_mask),
                        hand_count_cards(gp->predealt.hands[player] & heart_mask),
                        hand_count_cards(gp->predealt.hands[player] & spade_mask)
                    },
                    player,
                    13 - std::accumulate(std::begin(gp->biasdeal[player]),
                                         std::end(gp->biasdeal[player]),
                                         0, [](int a, int b) {return b == -1 ? a : a + b;}),
                };
            }
        }
        all_players_ = depth_ == 4;
    }

    void operator()(globals *gp,
                    std::vector<shape_probability> &list,
                    shape_probability element = shape_probability{})
    {
        std::array<int, 4> cardsleft = {13, 13, 13, 13};
        std::array<int, 4> reservedslots = {0, 0, 0, 0};
        for (int player = 0; player < depth_; player++)
            std::transform(std::begin(reservedslots),
                           std::end(reservedslots),
                           std::begin(players_[player].lengths_),
                           std::begin(reservedslots),
                           [](int a, int b) {return b == - 1? a : a + b;});
        (*this)(gp, list, element, cardsleft, reservedslots);
    }

    void operator()(globals *gp,
                    std::vector<shape_probability> &list,
                    shape_probability element,
                    std::array<int, 4> cardsleft,
                    std::array<int, 4> reservedslots)
    {
        auto lengths = players_[depth_-1].lengths_;
        if (depth_ == 1 && all_players_) {
            auto iter = std::begin(lengths);
            for (int left: cardsleft) {
                if (0)
                    std::cout << depth_ << ", "
                        << players_[depth_-1].compass_ << ": "
                        << *iter << " != "
                        << left << "\n";
                if (*iter != -1 && left != *iter)
                    return;
                iter++;
            }
        }
        int slotsleft = players_[--depth_].slotsleft_;

        auto iter = std::begin(lengths);
        int totalleft = std::accumulate(std::begin(cardsleft),
                                        std::end(cardsleft), 0,
                                        [&iter](int a, int b) {return *iter++ == -1 ? a + b : a;});

        iter = std::begin(lengths);
        totalleft = std::accumulate(std::begin(reservedslots),
                                    std::end(reservedslots), totalleft,
                                    [&iter](int a, int b) {return *iter++ == -1 ? a - b : a;});

        loop_lengths<0>(gp, list, element, cardsleft, reservedslots, totalleft,
                        slotsleft);
        depth_++;
    }

    template<int suit>
    void loop_lengths(globals *gp, std::vector<shape_probability> &list,
                      shape_probability element, std::array<int, 4> cardsleft,
                      std::array<int, 4> reservedslots, int totalleft,
                      int slotsleft) {
        if (0)
            std::cout << players_[depth_].compass_ << ", " << suit << ": "
                << element << ", " << std::setw(2) << totalleft << ", "
                << std::setw(2) << slotsleft << ": " << std::setw(2)
                << cardsleft[3] << "=" << std::setw(2) << cardsleft[2] << "="
                << std::setw(2) << cardsleft[1] << "=" << std::setw(2)
                << cardsleft[0] << " - " << std::setw(2) << reservedslots[3]
                << "=" << std::setw(2) << reservedslots[2] << "="
                << std::setw(2) << reservedslots[1] << "=" << std::setw(2)
                << reservedslots[0] << "\n";
        if (players_[depth_].lengths_[suit] != -1) {
            int length = !depth_ && all_players_ ? cardsleft[suit]
                : players_[depth_].lengths_[suit];
            reservedslots[suit] -= length;
            return calculate<suit>(gp, list, element, cardsleft, reservedslots,
                                   totalleft, slotsleft, length);
        }

        int suitcardsleft = cardsleft[suit] - reservedslots[suit];
        totalleft -= suitcardsleft;

        for (int length =
             std::max(players_[depth_].predeal_[suit], slotsleft - totalleft);
             length <= std::min(slotsleft, suitcardsleft); length++)
            calculate<suit>(gp, list, element, cardsleft, reservedslots, totalleft,
                            slotsleft - length, length);
    }

    template<int suit>
    void calculate(globals *gp,
                   std::vector<shape_probability> &list,
                   shape_probability element,
                   std::array<int, 4> cardsleft,
                   std::array<int, 4> reservedslots,
                   int totalleft,
                   int slotsleft,
                   int length)
    {
        if (0)
            std::cout << players_[depth_].compass_ << ": "
                << slotsleft << ": "
                << cardsleft[suit] << " L "
                << length << " P "
                << players_[depth_].predeal_[suit] << "\n";

        element.add_suit(players_[depth_].compass_, suit,
                         length - players_[depth_].predeal_[suit]);
        element *= ncrtable(cardsleft[suit] - total_predeal_[suit],
                            length - players_[depth_].predeal_[suit]);
        total_predeal_[suit] -= players_[depth_].predeal_[suit];
        cardsleft[suit] -= length;

        next_suit<suit>(gp, list, element,
                        cardsleft, reservedslots,
                        totalleft, slotsleft);
        total_predeal_[suit] += players_[depth_].predeal_[suit];
    }

    template<int suit>
    void next_suit(globals *gp,
                   std::vector<shape_probability> &list,
                   shape_probability element,
                   std::array<int, 4> cardsleft,
                   std::array<int, 4> reservedslots,
                   int totalleft,
                   int slotsleft)
    {
        loop_lengths<suit + 1>(gp, list, element, cardsleft, reservedslots,
                               totalleft, slotsleft);
    }

};

template<>
void shape_builder::next_suit<3>(globals *gp,
        std::vector<shape_probability> &list,
        shape_probability element,
        std::array<int, 4> cardsleft,
        std::array<int, 4> reservedslots,
        int,
        int slotsleft)
{
    int player = players_[depth_].compass_;
    if (0)
        std::cout << players_[depth_].compass_ << ": "
            << element << " - "
            << element[player][3] << "-"
            << element[player][2] << "-"
            << element[player][1] << "-"
            << element[player][0] << "\n";
    assert(element[player][0] + players_[depth_].predeal_[0] +
            element[player][1] + players_[depth_].predeal_[1] +
            element[player][2] + players_[depth_].predeal_[2] +
            element[player][3] + players_[depth_].predeal_[3] == 13);
    if (depth_) {
        (*this)(gp, list, element, cardsleft, reservedslots);
    } else {
        list.push_back(element);
    }
}

template<typename rng_t>
void predeal_bias<rng_t>::build_shape_table(globals *gp)
{
    shape_builder{gp}(gp, probabilities_);

    if (probabilities_.empty())
        error("No shapes are possible for the predeal shape limits.");

    std::partial_sum(std::begin(probabilities_),
                     std::end(probabilities_),
                     std::begin(probabilities_));

    bits_required_ = mp::msb(static_cast<uint96_t>(probabilities_.back())) + 1;

    // Create a lookup table where players with bias are before rest of players.
    unsigned pos = 0;
    unsigned pos_end = 3;
    for (unsigned player = 0; player < 4; ++player) {
        if (hand_count_cards(gp->predealt.hands[player]) == 13)
            continue;
        if (probabilities_.back()[player])
            this->player_map_[pos++] = player;
        else
            this->player_map_[pos_end--] = player;
    }

    this->predeal_end_ = pos;
    this->player_end_ = 4;
    if (pos < pos_end + 1) {
        std::copy(std::begin(this->player_map_) + pos_end + 1, std::end(this->player_map_),
                  std::begin(this->player_map_) + pos);
        this->player_end_ -= (pos_end + 1) - pos;
    }
}

static inline void validate_bias(globals* gp) {
    int p, s;
    char err[256];
    int biastotal = 0;
    int len[4] = {0}, lenset[4] = {0};
    for (p = 0; p < 4; p++) {
        int totset = 0;
        int tot = 0;
        for (s = 0; s < 4; s++) {
            const int predealcnt = hand_count_cards(gp->predealt.hands[p] & suit_masks[s]);
            if (gp->biasdeal[p][s] >= 0) {
                if (predealcnt > gp->biasdeal[p][s]) {
                    sprintf(err, "More predeal cards than bias allows for %s in %s\n", player_name[p], suit_name[s]);
                    error(err);
                }
                len[s] += gp->biasdeal[p][s];
                lenset[s]++;
                tot += gp->biasdeal[p][s];
                totset++;
                biastotal += gp->biasdeal[p][s] - predealcnt;
            } else {
                len[s] += predealcnt;
                tot += predealcnt;
            }
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
}

template<typename rng_t>
predeal_bias<rng_t>::predeal_bias(globals* gp) :
    predeal_cards<rng_t>(gp)
{
    validate_bias(gp);
    build_shape_table(gp);
}

template<typename rng_t>
auto predeal_bias<rng_t>::shuffle_player(rng_copy_t rng,
                                         hand &cards,
                                         shape_probability::depth_shape shape,
                                         suit_map_t &suit_map,
                                         suit_end_t &suit_end,
                                         unsigned &destination) -> rng_copy_t
{
    for (unsigned suit = 0; suit < 4; suit++) {
        unsigned length = shape[suit];

        for (; length > 0; --length, --suit_end[suit], --destination) {
            fast_uniform_int_distribution<unsigned> dist{0, suit_end[suit] - 1};
            unsigned suitsrc = dist(rng);
            unsigned src = suit_map[suit][suitsrc];
            unsigned dst = destination - 1;

            cards |= this->pack_.c[src];
            card dstcard = this->pack_.c[dst];
            std::swap(this->pack_.c[dst], this->pack_.c[src]);

            unsigned dstsuit = C_SUIT(dstcard);

            auto iter = std::find(std::rend(suit_map[dstsuit]) - suit_end[dstsuit],
                                  std::rend(suit_map[dstsuit]),
                                  dst);

            assert(iter != std::rend(suit_map[dstsuit]) &&
                   "Per suit map doesn't have expected index");

            *iter = src;
            suit_map[suit][suitsrc] = suit_map[suit][suit_end[suit]-1];
        }
    }
    return rng;
}

template<typename rng_t>
int predeal_bias<rng_t>::do_shuffle(board *d, globals *gp)
{
    using result_type = typename rng_t::result_type;
    constexpr size_t rng_bits = std::numeric_limits<result_type>::digits;
    // Select shape to use
    typename rng_t::copy_type rng = this->rng_;
    uint96_t shape_number;
    do {
        unsigned bits = 0;
        shape_number = 0;

        for (; bits + rng_bits < bits_required_; bits += rng_bits)
          shape_number |= uint96_t{rng()} << bits;

        const size_t mask = (size_t{1} << (bits_required_ - bits)) - 1;
        shape_number |= uint96_t{rng() & mask} << bits;
    } while (!(shape_number < probabilities_.back()));

    auto shape = std::upper_bound(std::begin(probabilities_),
                                  std::end(probabilities_),
                                  shape_number);

    assert(shape != std::end(probabilities_) &&
           "shape_number must be less than the maximum combinations number");

    board deal{gp->predealt};
    unsigned cards_end = this->totalleft_;
    suit_map_t suit_map;
    suit_end_t suit_end{0,0,0,0};
    unsigned pos;

    // Build lookup table for card locations
    for (pos = 0; pos < cards_end; pos++) {
        unsigned suit = C_SUIT(this->pack_.c[pos]);
        suit_map[suit][suit_end[suit]++] = pos;
    }

    for (pos = 0; pos < predeal_end_; pos++) {
        unsigned player = this->player_map_[pos];
        auto player_shape = (*shape)[player];
        rng = shuffle_player(rng,
                             deal.hands[player],
                             player_shape,
                             suit_map,
                             suit_end,
                             cards_end);
    }

    for (;pos < this->player_end_ - 1; pos++) {
        unsigned player = this->player_map_[pos];
        rng = this->shuffle_pack(rng,
                                 deal.hands[player],
                                 cards_end,
                                 this->cardsleft_[player],
                                 false);
    }

    if (pos < this->player_end_) {
        unsigned player = this->player_map_[pos];
        rng = this->shuffle_pack(rng,
                                 deal.hands[player],
                                 cards_end,
                                 this->cardsleft_[player],
                                 true);
    }

    this->rng_ = rng;
    *d = deal;
    return 0;
}

static constexpr long libdeal_size = 26;

template<typename rng_t>
load_library<rng_t>::load_library(globals* gp) :
    shuffle_rng<rng_t>(gp)
{
    // Find library.dat from predefined search paths
    static const std::string prefixes[] = { "", "./", "../", "../../", "c:/", "c:/data/",
        "d:/myprojects/dealer/", "d:/arch/games/gib/" };
    static const std::string filename{"library.dat"};
    for (const std::string& p : prefixes) {
        lib_.open(p + filename,
                std::ios_base::binary | std::ios_base::in);
        if (lib_.is_open()) {
            lib_.seekg(libdeal_size * gp->loadindex);
            return;
        }
    }
    throw library_not_found();
}

#if __cplusplus >= 202002L
using std::rotl;
#else
uint16_t rotl(uint16_t v, int s)
{
    return (v << s) | (v >> (std::numeric_limits<uint16_t>::digits - s));
}
#endif

template<typename rng_t>
int load_library<rng_t>::do_shuffle(board* d, globals* gp)
{
    tagLibdeal libdeal;
retry_read:
    // Read a deal from library
    lib_.read(reinterpret_cast<char*>(&libdeal), libdeal_size);
    // Check that read succeed
    if (!lib_.good())
        return -1;

    gp->loading = 1;
    *d = board{0,0,0,0};

    // Load tricks from network byte order and adjust players to correct bit
    // position.
    for (int contract = 0; contract < 4; ++contract)
        gp->libtricks[contract] = rotl(ntohs(libdeal.tricks[contract+1]), 4);
    gp->libtricks[4] = rotl(ntohs(libdeal.tricks[0]), 4);

    // Load each suit from network byte order
    for (int suit = 0; suit < 4; ++suit) {
        unsigned long cards = ntohl(libdeal.suits[suit]);
        // Assign each card to the player.
        for (int rank = 0; rank < 13; ++rank) {
            unsigned player = cards & 0x3;
            cards >>= 2;

            d->hands[player] = hand_add_card(d->hands[player], MAKECARD(suit, 12 - rank));
        }
    }

    // Check that loading produced a valid hand for all players
    for (const auto& h: d->hands) {
        if (hand_count_cards(h) != 13) {
            fprintf(stderr, "Deal %d is broken\n", gp->ngen);
            fprintcompact(stderr, d, 1, 0);
            gp->ngen++;

            goto retry_read;
        }
    }
    return 0;
}

template<typename parent>
shuffle_swap<parent>::shuffle_swap(globals* gp) :
    parent(gp),
    swapindex_(0),
    swapbound_(gp->swapping == 2 ? 2 : 6)
{
}

template<typename parent>
int shuffle_swap<parent>::do_shuffle(board* d, globals* gp)
{
    switch(swapindex_++) {
    case 0: /* -> NESW */
        return parent::do_shuffle(d, gp);
    case 1: /* swap EW -> NWSE */
        std::swap(d->hands[1], d->hands[3]);
        break;
    case 2: /* swap SW -> NWES */
        std::swap(d->hands[2], d->hands[3]);
        break;
    case 3: /* swap ES -> NEWS */
        std::swap(d->hands[1], d->hands[2]);
        break;
    case 4: /* swap EW -> NSWE */
        std::swap(d->hands[1], d->hands[3]);
        break;
    case 5: /* swap SW -> NSEW */
        std::swap(d->hands[2], d->hands[3]);
        break;
    }
    // Make sure tricks function doesn't try to use database values after swap
    gp->loading = 0;
    if (swapindex_ == swapbound_)
        swapindex_ = 0;
    return 0;
}
#if __cplusplus >= 201402L
using std::make_unique;
#else
template<typename T, class... Args >
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>{new T{std::forward<Args>(args)...}};
}
#endif

struct std::unique_ptr<shuffle> shuffle::factory(globals *gp)
{
    switch (gp->random_engine) {
    case 0:
    default:
        return shuffle_rng<pcg>::factory(gp);
    case 1:
        return shuffle_rng<random_device>::factory(gp);
    }
}

template<typename rng_t>
struct std::unique_ptr<shuffle> shuffle_rng<rng_t>::factory(globals *gp)
{
    try {
       // Are we using exhaust mode?
       if (gp->computing_mode == EXHAUST_MODE)
          return make_unique<exhaust_mode<rng_t>>(gp);

        // Are we loading from library.dat?
        if (gp->loading) {
            if (gp->swapping)
                return make_unique<limit<shuffle_swap<load_library<rng_t>>>>(gp);
            else
                return make_unique<limit<load_library<rng_t>>>(gp);
        }

        // Is there suit length biases?
        if (!std::all_of(std::begin(gp->biasdeal[0]), std::end(gp->biasdeal[3]),
                    [](const char& v) { return v == -1; })) {
            return make_unique<limit<predeal_bias<rng_t>>>(gp);
        }

        // Is there cards attached to a hand?
        if (std::any_of(std::begin(gp->predealt.hands), std::end(gp->predealt.hands),
                    [](const hand& h) { return h != 0; })) {
            if (gp->swapping)
                return make_unique<limit<shuffle_swap<predeal_cards<rng_t>>>>(gp);
            else
                return make_unique<limit<predeal_cards<rng_t>>>(gp);
        }

        // Completely random hands
        if (gp->swapping)
            return make_unique<limit<shuffle_swap<random_hand<rng_t>>>>(gp);
        else
            return make_unique<limit<random_hand<rng_t>>>(gp);
    } catch(...) {
        return {};
    }
}

} /* namespace DEFUN() */
