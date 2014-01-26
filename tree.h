#ifndef TREE_H
#define TREE_H
/*
 * decision tree and other expression stuff
 */

struct tree {
        int             tr_type;
        struct tree     *tr_leaf1, *tr_leaf2;
        int             tr_int1;
        int             tr_int2;
        int             tr_int3;
};

struct expr {
        struct tree*    ex_tr;
        char*               ex_ch;
        struct expr*    next;
};

#define NIL     ((struct tree *) 0)

#define SUIT_CLUB       0
#define SUIT_DIAMOND    1
#define SUIT_HEART      2
#define SUIT_SPADE      3
#define SUIT_NT         4
#define NSUITS          4

#define MAKECARD(suit, rank)    ((card)(((suit)<<6)|(rank)))

#define MAKECONTRACT(suit, tricks) (tricks*5+suit)
#define C_SUIT(c)               ((c)>>6)
#define C_RANK(c)               ((c)&0x3F)
#define NO_CARD                 0xFF

#define COMPASS_NORTH   0
#define COMPASS_EAST    1
#define COMPASS_SOUTH   2
#define COMPASS_WEST    3

#define VULNERABLE_NONE 0
#define VULNERABLE_NS   1
#define VULNERABLE_EW   2
#define VULNERABLE_ALL  3

#define NON_VUL 0
#define VUL     1

#define TRT_NUMBER      0
#define TRT_AND2        1
#define TRT_OR2         2
#define TRT_CMPEQ       3
#define TRT_CMPNE       4
#define TRT_CMPLT       5
#define TRT_CMPLE       6
#define TRT_CMPGT       7
#define TRT_CMPGE       8
#define TRT_LENGTH      9
#define TRT_ARPLUS      10
#define TRT_ARMINUS     11
#define TRT_ARTIMES     12
#define TRT_ARDIVIDE    13
#define TRT_ARMOD       14
#define TRT_HCPTOTAL    15
#define TRT_HCP         16
#define TRT_SHAPE       17
#define TRT_NOT         18
#define TRT_HASCARD     19
#define TRT_IF          20
#define TRT_THENELSE    21
#define TRT_LOSERTOTAL  22
#define TRT_LOSER       23
#define TRT_TRICKS      24
#define TRT_RND          25
#define TRT_CONTROL     26
#define TRT_CONTROLTOTAL        27
#define TRT_SCORE       28
#define TRT_IMPS        29
#define TRT_CCCC        30
#define TRT_QUALITY     31
#define TRT_PT0TOTAL    32
#define TRT_PT0         33
#define TRT_PT1TOTAL    34
#define TRT_PT1         35
#define TRT_PT2TOTAL    36
#define TRT_PT2         37
#define TRT_PT3TOTAL    38
#define TRT_PT3         39
#define TRT_PT4TOTAL    40
#define TRT_PT4         41
#define TRT_PT5TOTAL    42
#define TRT_PT5         43
#define TRT_PT6TOTAL    44
#define TRT_PT6         45
#define TRT_PT7TOTAL    46
#define TRT_PT7         47
#define TRT_PT8TOTAL    48
#define TRT_PT8         49
#define TRT_PT9TOTAL    50
#define TRT_PT9         51

/*
 * Actions to be taken
 */
struct acuft{
  long acuf_lowbnd;
  long acuf_highbnd;
  long acuf_uflow;
  long acuf_oflow;
  long*acuf_freqs;
};

struct acuft2d{
  long acuf_lowbnd_expr1;
  long acuf_highbnd_expr1;
  long acuf_lowbnd_expr2;
  long acuf_highbnd_expr2;
  long*acuf_freqs;
};




struct action {
        struct action   *ac_next;
        int             ac_type;
        struct tree     *ac_expr1;
        struct tree *ac_expr2;
        int             ac_int1;
        char            *ac_str1;
        union {
                struct acuft acu_f;
        struct acuft2d acu_f2d;
        } ac_u;
};


#define ACT_PRINTALL    0
#define ACT_PRINT       1
#define ACT_AVERAGE     2
#define ACT_FREQUENCY   3
#define ACT_PRINTCOMPACT 4
#define ACT_EVALCONTRACT 5
#define ACT_PRINTPBN 6
#define ACT_PRINTEW 7
#define ACT_FREQUENCY2D 8
#define ACT_PRINTONELINE 9
#define ACT_PRINTES 10

/* Constants for CCCC and Quality */

#define RK_ACE          12
#define RK_KING         11
#define RK_QUEEN        10
#define RK_JACK          9
#define RK_TEN           8
#define RK_NINE          7
#define RK_EIGHT     6

#ifdef WIN32
#define bcopy(s,t,size) memcpy(t,s,size)
#endif

#endif /* TREE_H */
