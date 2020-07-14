
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

/**
 * Deal random hands with potentially fixed cards and shape restrictions
 */
template<typename rng_t>
struct predeal_bias : public predeal_cards<rng_t> {
    predeal_bias(globals* gp);
    int do_shuffle(board* d, globals* gp) override;
protected:

    decltype(std::begin(predeal_bias::pack_.c)) first_;

    struct bt {
        int total[4];
    };

    bt biastotal(const globals* gp) const {

        bt rv{{-1,-1,-1,-1}};

        // Find out how may cards bias affects in each suit
        for (const auto& pl: gp->biasdeal)
            for (unsigned i = 0; i < 4; ++i)
                if (pl[i] != -1)
                    rv.total[i] = std::max(rv.total[i], 0) + pl[i];
        return rv;
    }
};

static void setup_bias(globals* gp) {
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
                /* Remove predealt cards from bias length */
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
    predeal_cards<rng_t>(gp),
    first_()
{
    hand predealt = std::accumulate(std::begin(gp->predealt.hands), std::end(gp->predealt.hands),
            hand{0}, std::bit_or<hand>());

    setup_bias(gp);

    bt bias = biastotal(gp);

    auto end = std::end(this->pack_.c) - hand_count_cards(predealt);
    // Partition cards from biased suits to the middle
    first_ = std::partition(std::begin(this->pack_.c), end,
            [&bias](const card& c) {
                return bias.total[C_SUIT(c)] == -1;
            });

    // Sort middle based on suit
    std::sort(first_, end,
            [](const card& a, const card& b) {
                return a < b;
            });
}

template<typename rng_t>
int predeal_bias<rng_t>::do_shuffle(board* d, globals* gp)
{
    // This implementation isn't perfect. Unlikely cases may not generate any or
    // very few hands. Too bad better implementation would require shape based
    // predeal tables. Tables would allow more generic predeal requirements but
    // they are also harder to implement.
    hand predealt = std::accumulate(std::begin(gp->predealt.hands), std::end(gp->predealt.hands),
            hand{0}, std::bit_or<hand>());

    auto end = std::end(this->pack_.c) - hand_count_cards(predealt);

retry:
    board pd{gp->predealt};

    typename rng_t::copy_type rng = this->rng_;
    // shuffle biased cards for each player and suit
    auto first = first_;
    for (;first != end;) {
        unsigned s = C_SUIT(*first);
        // Find the end of suit in middle section
        auto send = std::find_if(first, end,
                [s](const card& c) { return (c & suit_masks[s]) == 0; });
        auto last = send - 1;

        for (unsigned p = 0; p < 4; ++p) {
            if (gp->biasdeal[p][s] == -1)
                continue;
            unsigned bc = gp->biasdeal[p][s];
            for (; bc-- > 0; --last) {
                fast_uniform_int_distribution<unsigned> dist(0, last - first);
                unsigned r = dist(rng);
                std::swap(*(first + r), *last);
                // Assign selected cards to predeal hands
                pd.hands[p] |= *last;
            }
        }
        first = send;
        assert(first == end || s != (unsigned)C_SUIT(*first));
    }
    // Shuffle remaining cards in suits which has bias limits
    bt bias = biastotal(gp);
    for (unsigned s = 0; s < 4; ++s) {
        // Nothing to do if no cards or all cards in suit have been handled
        if (bias.total[s] == -1 ||
                bias.total[s] == 13 - hand_count_cards(predealt & suit_masks[s]))
            continue;
        // Count free slot in each hand without limits
        unsigned free_slots[4] = {0};
        unsigned range = 0;
        for (unsigned p = 0; p < 4; ++p) {
            if (gp->biasdeal[p][s] == -1)
                range += free_slots[p] = 13 - hand_count_cards(pd.hands[p]);
        }

        unsigned left = 13 - hand_count_cards(predealt & suit_masks[s]) - bias.total[s];

        // If not enough slots left for remaining cards then retry. It might
        // mean requirements are unlikely to match or impossible.
        if (range < left) {
            if (++gp->ngen < gp->maxgenerate)
                goto retry;
            return 1;
        }


        const card lim = card{1} << suit_shifts[s];
        auto first = std::lower_bound(first_, end, lim);
        auto send = first + left;
        // Give remaining cards to rest of players
        std::for_each(first, send,
                [&pd, &range, &free_slots, &rng](const card& c) {
                    fast_uniform_int_distribution<unsigned> dist(0, range-1);
                    unsigned pos = dist(rng);
                    unsigned p = 0;
                    // Find player which matches the random card slot
                    for (; p < 4; pos -= free_slots[p], ++p)
                        if (pos < free_slots[p])
                            break;


                    // Give the card to selected player
                    pd.hands[p] |= c;
                    free_slots[p]--;
                    range--;
                });
    }

    unsigned cardsleft;
    unsigned sum = first_ - std::begin(this->pack_.c);
    for (int player = 0; player < 3; ++player) {
        cardsleft = 13 - hand_count_cards(pd.hands[player]);
        rng = this->shuffle_pack(rng, pd.hands[player],
                                 sum, cardsleft, false);
    }
    cardsleft = 13 - hand_count_cards(pd.hands[3]);
    rng = this->shuffle_pack(rng, pd.hands[3],
                             sum, cardsleft, true);
    this->rng_ = rng;

    *d = pd;
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
