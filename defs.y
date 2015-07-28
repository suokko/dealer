%{
#include <stdio.h>
#include <string.h>

#ifdef WIN32

/* void * _alloca( size_t ) ; */
#define alloca _alloca

#endif /* WIN32 */

#include <stdlib.h>
#include "tree.h"
#include "dealer.h"

void  yyerror (char*);
void  setshapebit (struct shape *, int, int, int, int);
void  predeal (int, card);
card  make_card(char,char);
void  clearpointcount(void);
void  clearpointcount_alt(int);
void  pointcount(int,int);
int   make_contract (char, char, char);
extern int yylex (void);
static int d2n(char s[4]);

static int predeal_compass;     /* global variable for predeal communication */

static int pointcount_index;    /* global variable for pointcount communication */
extern struct globals *gp;

static struct treebase *var_lookup(char *s, int mustbethere) ;
static struct action *newaction(int type, struct treebase * p1, char * s1, int, struct treebase * ) ;
static struct treebase *newtree (int, struct treebase*, struct treebase*, int, int);
static struct treebase *newhascardtree (int, int, card);
static struct expr  *newexpr(struct treebase* tr1, char* ch1, struct expr* ex1);
static void bias_deal(int suit, int compass, int length) ;
static void predeal_holding(int compass, char *holding) ;
static inline struct shape makeshape(char s[4], int any);
static inline struct shape compineshape(struct shape *a, int op, struct shape *b);
static struct treebase *newshapetree(int compass, struct shape *list);
static void new_var(char *s, struct treebase *t) ;
%}


%union {
        int     y_int;
        card     y_card;
        char    *y_str;
        struct treebase *y_treebase;
        struct shape  y_shape;
        struct action *y_action;
        struct expr   *y_expr;
        char    y_distr[4];
};

%left QUERY COLON
%left OR2
%left AND2
%left CMPEQ CMPNE
%left CMPLT CMPLE CMPGT CMPGE
%left ARPLUS ARMINUS
%left ARTIMES ARDIVIDE ARMOD
%nonassoc NOT

%token GENERATE PRODUCE HCP SHAPE ANY EXCEPT CONDITION ACTION
%token PRINT PRINTALL PRINTEW PRINTPBN PRINTCOMPACT PRINTONELINE
%token AVERAGE HASCARD FREQUENCY PREDEAL POINTCOUNT ALTCOUNT
%token CONTROL LOSER DEALER
%token QUALITY CCCC
%token TRICKS LEADTRICKS NOTRUMPS NORTHSOUTH EASTWEST
%token EVALCONTRACT ALL NONE SCORE IMPS RND AVG ABS
%token PT0 PT1 PT2 PT3 PT4 PT5 PT6 PT7 PT8 PT9 PRINTES

%token <y_int> NUMBER
%token <y_str> HOLDING
%token <y_str> STRING
%token <y_str> IDENT
%token <y_int> COMPASS
%token <y_int> VULNERABLE
%token <y_int> VULN
%token <y_int> SUIT
%token <y_card> CARD
%token <y_int> CONTRACT
%token <y_distr> DISTR DISTR_OR_NUMBER

%type <y_treebase>  expr
%type <y_int> number compass printlist shlprefix any vulnerable
%type <y_distr> shape
%type <y_action> actionlist action
%type <y_expr> exprlist
%type <y_str> optstring
%type <y_shape> shapelistel shapelist

%start defs

%%

defs
        : /* empty */
        | defs def
        ;

def
        : GENERATE number
                { if(!gp->maxgenerate) gp->maxgenerate = $2; }
        | PRODUCE number
                { if(!gp->maxproduce) gp->maxproduce = $2; }
        | DEALER  compass
                { gp->maxdealer = $2; }
        | VULNERABLE vulnerable
                { gp->maxvuln = $2; }
        | PREDEAL predealargs
        | POINTCOUNT { clearpointcount(); pointcount_index=12;} pointcountargs
        | ALTCOUNT number
                { clearpointcount_alt($2); pointcount_index=12;} pointcountargs
        | CONDITION expr
                { gp->decisiontree = $2; }
        | expr
                { gp->decisiontree = $1; }
        | IDENT '=' expr
                { new_var($1, $3); }
        | ACTION actionlist
                { gp->actionlist = $2; }
        ;

predealargs
        : predealarg
        | predealargs predealarg
        ;

predealarg
        :  COMPASS { predeal_compass = $1;} holdings
        |  SUIT '(' COMPASS ')' CMPEQ NUMBER {bias_deal($1,$3,$6);}
        ;

holdings
        : HOLDING
                { predeal_holding(predeal_compass, $1); }
        | holdings ',' HOLDING
                { predeal_holding(predeal_compass, $3); }
        ;

pointcountargs
        : /* empty */
        | number
                { pointcount(pointcount_index, $1);
                  pointcount_index--;
                }
          pointcountargs
        ;

compass
        : COMPASS
                { gp->use_compass |= 1 << $1; $$= $1; }
        ;

vulnerable
        : VULNERABLE
                { $$= $1; }
        ;

shlprefix
        : ','
                { $$ = 0; }
        | ARPLUS
                { $$ = 0; }
        | /* empty */
                { $$ = 0; }
        | ARMINUS
                { $$ = 1; }
        ;

any
        : /* empty */
                { $$ = 0; }
        | ANY
                { $$ = 1; }
        ;

/* AM990705: extra production to fix unary-minus syntax glitch */
number
        : NUMBER
        | ARMINUS NUMBER
                { $$ = - $2; }
        | DISTR_OR_NUMBER
                { $$ = d2n($1); }
        | ARMINUS DISTR_OR_NUMBER
                { $$ = - d2n($2); }
        ;

shape
        : DISTR
        | DISTR_OR_NUMBER
        ;
shapelistel
        : any shape
                { $$ = makeshape($2, $1); }
        ;

shapelist
        : shapelistel shlprefix shapelistel
                { $$ = compineshape(&$1, $2, &$3); }
        | shapelist shlprefix shapelistel
                { $$ = compineshape(&$1, $2, &$3); }
        | shapelistel
                { $$ = $1; }
        ;

expr
        : number
                { $$ = newtree(TRT_NUMBER, NIL, NIL, $1, 0); }
        | IDENT
                { $$ = var_lookup($1, 1); }
        | SUIT '(' compass ')'
                { $$ = newtree(TRT_LENGTH, NIL, NIL, $1, $3); }
        | HCP '(' compass ')'
                { $$ = newtree(TRT_HCPTOTAL, NIL, NIL, $3, 0); }
        | HCP '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_HCP, NIL, NIL, $3, $5); }
        | PT0 '(' compass ')'
                { $$ = newtree(TRT_PT0TOTAL, NIL, NIL, $3, 0); }
        | PT0 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT0, NIL, NIL, $3, $5); }
        | PT1 '(' compass ')'
                { $$ = newtree(TRT_PT1TOTAL, NIL, NIL, $3, 0); }
        | PT1 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT1, NIL, NIL, $3, $5); }
        | PT2 '(' compass ')'
                { $$ = newtree(TRT_PT2TOTAL, NIL, NIL, $3, 0); }
        | PT2 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT2, NIL, NIL, $3, $5); }
        | PT3 '(' compass ')'
                { $$ = newtree(TRT_PT3TOTAL, NIL, NIL, $3, 0); }
        | PT3 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT3, NIL, NIL, $3, $5); }
        | PT4 '(' compass ')'
                { $$ = newtree(TRT_PT4TOTAL, NIL, NIL, $3, 0); }
        | PT4 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT4, NIL, NIL, $3, $5); }
        | PT5 '(' compass ')'
                { $$ = newtree(TRT_PT5TOTAL, NIL, NIL, $3, 0); }
        | PT5 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT5, NIL, NIL, $3, $5); }
        | PT6 '(' compass ')'
                { $$ = newtree(TRT_PT6TOTAL, NIL, NIL, $3, 0); }
        | PT6 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT6, NIL, NIL, $3, $5); }
        | PT7 '(' compass ')'
                { $$ = newtree(TRT_PT7TOTAL, NIL, NIL, $3, 0); }
        | PT7 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT7, NIL, NIL, $3, $5); }
        | PT8 '(' compass ')'
                { $$ = newtree(TRT_PT8TOTAL, NIL, NIL, $3, 0); }
        | PT8 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT8, NIL, NIL, $3, $5); }
        | PT9 '(' compass ')'
                { $$ = newtree(TRT_PT9TOTAL, NIL, NIL, $3, 0); }
        | PT9 '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_PT9, NIL, NIL, $3, $5); }
        | LOSER '(' compass ')'
                { $$ = newtree(TRT_LOSERTOTAL, NIL, NIL, $3, 0); }
        | LOSER '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_LOSER, NIL, NIL, $3, $5); }
        | CONTROL '(' compass ')'
                { $$ = newtree(TRT_CONTROLTOTAL, NIL, NIL, $3, 0); }
        | CONTROL '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_CONTROL, NIL, NIL, $3, $5); }
        | CCCC '(' compass ')'
                { $$ = newtree(TRT_CCCC, NIL, NIL, $3, 0); }
        | QUALITY '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_QUALITY, NIL, NIL, $3, $5); }
        | SHAPE '(' compass ',' shapelist ')'
                {
		  $$ = newshapetree($3, &$5);
		}
        | HASCARD '(' COMPASS ',' CARD ')'
                { $$ = newhascardtree(TRT_HASCARD, $3, $5); }
    | TRICKS '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_TRICKS, NIL, NIL, $3, $5); }
    | TRICKS '(' compass ',' NOTRUMPS ')'
                { $$ = newtree(TRT_TRICKS, NIL, NIL, $3, 4); }
    | LEADTRICKS '(' compass ',' SUIT ')'
                { $$ = newtree(TRT_LEADTRICKS, NIL, NIL, $3, $5); }
    | LEADTRICKS '(' compass ',' NOTRUMPS ')'
                { $$ = newtree(TRT_LEADTRICKS, NIL, NIL, $3, 4); }
    | SCORE '(' VULN ',' CONTRACT ',' expr ')'
                { $$ = newtree(TRT_SCORE, $7, NIL, $3, $5); }
    | IMPS '(' expr ')'
                { $$ = newtree(TRT_IMPS, $3, NIL, 0, 0); }
        | '(' expr ')'
                { $$ = $2; }
        | expr CMPEQ expr
                { $$ = newtree(TRT_CMPEQ, $1, $3, 0, 0); }
        | expr CMPNE expr
                { $$ = newtree(TRT_CMPNE, $1, $3, 0, 0); }
        | expr CMPLT expr
                { $$ = newtree(TRT_CMPLT, $1, $3, 0, 0); }
        | expr CMPLE expr
                { $$ = newtree(TRT_CMPLE, $1, $3, 0, 0); }
        | expr CMPGT expr
                { $$ = newtree(TRT_CMPGT, $1, $3, 0, 0); }
        | expr CMPGE expr
                { $$ = newtree(TRT_CMPGE, $1, $3, 0, 0); }
        | expr AND2 expr
                { $$ = newtree(TRT_AND2, $1, $3, 0, 0); }
        | expr OR2 expr
                { $$ = newtree(TRT_OR2, $1, $3, 0, 0); }
        | expr ARPLUS expr
                { $$ = newtree(TRT_ARPLUS, $1, $3, 0, 0); }
        | expr ARMINUS expr
                { $$ = newtree(TRT_ARMINUS, $1, $3, 0, 0); }
        | expr ARTIMES expr
                { $$ = newtree(TRT_ARTIMES, $1, $3, 0, 0); }
        | expr ARDIVIDE expr
                { $$ = newtree(TRT_ARDIVIDE, $1, $3, 0, 0); }
        | expr ARMOD expr
                { $$ = newtree(TRT_ARMOD, $1, $3, 0, 0); }
        | expr QUERY expr COLON expr
                { $$ = newtree(TRT_IF, $1, newtree(TRT_THENELSE, $3, $5, 0, 0), 0, 0); }
        | NOT expr
                { $$ = newtree(TRT_NOT, $2, NIL, 0, 0); }
        | RND '(' expr ')'
                { $$ = newtree(TRT_RND, $3, NIL, 0, 0); }
        | AVG
                { $$ = newtree(TRT_AVG, NIL, NIL, 0, 0); }
        | ABS '(' expr ')'
                { $$ = newtree(TRT_ABS, $3, NIL, 0, 0); }
        ;

exprlist
        : expr
                { $$ = newexpr($1, 0, 0); }
        | STRING
                { $$ = newexpr(0, $1, 0); }
        | exprlist ',' expr
                { $$ = newexpr($3, 0, $1); }
        | exprlist ',' STRING
                { $$ = newexpr(0, $3, $1); }
        ;

actionlist
        : action
                { $$ = $1; }
        | action ',' actionlist
                { $$ = $1; $$->ac_next = $3; }
        | /* empty */
                { $$ = 0; }
        ;
action
        : PRINTALL
                { gp->will_print = 1;
                  $$ = newaction(ACT_PRINTALL, NIL, (char *) 0, 0, NIL);
                }
        | PRINTEW
                { gp->will_print = 1;
                  $$ = newaction(ACT_PRINTEW, NIL, (char *) 0, 0, NIL);
                }
        | PRINT '(' printlist ')'
                { gp->will_print = 1;
                  $$ = newaction(ACT_PRINT, NIL, (char *) 0, $3, NIL);
                }
        | PRINTCOMPACT
                { gp->will_print = 1;
                  $$=newaction(ACT_PRINTCOMPACT,NIL,0,0, NIL);}
        | PRINTONELINE
                { gp->will_print = 1;
                  $$ = newaction(ACT_PRINTONELINE, NIL, 0, 0, NIL);}
        | PRINTPBN
                { gp->will_print = 1;
                  $$=newaction(ACT_PRINTPBN,NIL,0,0, NIL);}
        | PRINTES '(' exprlist ')'
                { gp->will_print = 1;
                  $$=newaction(ACT_PRINTES,(struct treebase*)$3,0,0, NIL); }
        | EVALCONTRACT  /* should allow user to specify vuln, suit, decl */
                { gp->will_print = 1;
                  $$=newaction(ACT_EVALCONTRACT,0,0,0, NIL);}
        | PRINTCOMPACT '(' expr ')'
                { gp->will_print = 1;
                  $$=newaction(ACT_PRINTCOMPACT,$3,0,0, NIL);}
        | PRINTONELINE '(' expr ')'
                { gp->will_print = 1;
                  $$=newaction(ACT_PRINTONELINE,$3,0,0, NIL);}
        | AVERAGE optstring expr
                { $$ = newaction(ACT_AVERAGE, $3, $2, 0, NIL); }
        | AVERAGE optstring '(' expr ',' expr ')'
                { $$ = newaction(ACT_AVERAGE, $4, $2, 0, $6); }
        | FREQUENCY optstring '(' expr ',' number ',' number ')'
                { $$ = newaction(ACT_FREQUENCY, $4, $2, 0, NIL);
                  $$->ac_u.acu_f.acuf_lowbnd = $6;
                  $$->ac_u.acu_f.acuf_highbnd = $8;}
        | FREQUENCY optstring
           '(' expr ',' number ',' number ',' expr ',' number ',' number ')' {
             $$ = newaction(ACT_FREQUENCY2D, $4, $2, 0, $10);
             $$->ac_u.acu_f2d.acuf_lowbnd_expr1 = $6;
             $$->ac_u.acu_f2d.acuf_highbnd_expr1 = $8;
             $$->ac_u.acu_f2d.acuf_lowbnd_expr2 = $12;
             $$->ac_u.acu_f2d.acuf_highbnd_expr2 = $14;
                }
        ;
optstring
        : /* empty */
                { $$ = (char *) 0; }
        | STRING
                { $$ = $1; }
        ;
printlist
        : COMPASS
                { $$ = (1<<$1); }
        | printlist ',' COMPASS
                { $$ = $1|(1<<$3); }
        ;
%%

static struct var {
        struct var *v_next;
        char *v_ident;
        struct treebase *v_tree;
} *vars=0;

struct treebase *var_lookup(char *s, int mustbethere)
{
        struct var *v;

        for(v=vars; v!=0; v = v->v_next)
                if (strcmp(s, v->v_ident)==0)
                        return v->v_tree;
        if (mustbethere) {
                char buffer[40 + strlen(s)];
                sprintf(buffer, "unknown variable: %s", s);
                yyerror(buffer);
        }
        return 0;
}

void new_var(char *s, struct treebase *t)
{
        struct var *v;
        struct treebase *var;
        /* char *mycalloc(); */

        if (var_lookup(s, 0)!=0)
                yyerror("redefined variable");
        v = (struct var *) mycalloc(1, sizeof(*v));
        var = newtree(TRT_VAR, t, NULL, -1, 0);
        v->v_next = vars;
        v->v_ident = s;
        v->v_tree = var;
        vars = v;
}

int lino=1;

void yyerror( char *s) {
        fprintf(stderr, "line %d: %s\n", lino, s);
        exit(-1);
}

int perm[24][4] = {
        {       0,      1,      2,      3       },
        {       0,      1,      3,      2       },
        {       0,      2,      1,      3       },
        {       0,      2,      3,      1       },
        {       0,      3,      1,      2       },
        {       0,      3,      2,      1       },
        {       1,      0,      2,      3       },
        {       1,      0,      3,      2       },
        {       1,      2,      0,      3       },
        {       1,      2,      3,      0       },
        {       1,      3,      0,      2       },
        {       1,      3,      2,      0       },
        {       2,      0,      1,      3       },
        {       2,      0,      3,      1       },
        {       2,      1,      0,      3       },
        {       2,      1,      3,      0       },
        {       2,      3,      0,      1       },
        {       2,      3,      1,      0       },
        {       3,      0,      1,      2       },
        {       3,      0,      2,      1       },
        {       3,      1,      0,      2       },
        {       3,      1,      2,      0       },
        {       3,      2,      0,      1       },
        {       3,      2,      1,      0       },
};

struct shape compineshape(struct shape *a, int op, struct shape *b)
{
        struct shape r;
        int i, max;
        max = sizeof(r.bits) / sizeof(r.bits[0]);
        for (i = 0; i < max; i++) {
                r.bits[i] = op == 0 ? a->bits[i] | b->bits[i] : a->bits[i] & (~b->bits[i]);
        }
        return r;
}

struct treebase *newshapetree(int compass, struct shape *list)
{
        struct treeshape *t;

        t = mycalloc(1, sizeof *t);
        t->base.tr_type = TRT_SHAPE;
        t->compass = compass;
        t->shape = *list;

        return &t->base;
}

struct shape makeshape(char s[4], int any)
{
        int i,j,p;
        int xcount=0, ccount=0;
        char copy_s[4];
        struct shape r = {{0}};

        for (i=0;i<4;i++) {
		if (s[i]=='x')
                        xcount++;
                else
                        ccount += s[i]-'0';
        }
        switch(xcount) {
        case 0:
                if (ccount!=13)
                        yyerror("wrong number of cards in shape");
                for (p=0; p<(any? 24 : 1); p++)
                        setshapebit(&r, s[perm[p][3]]-'0', s[perm[p][2]]-'0',
                                s[perm[p][1]]-'0', s[perm[p][0]]-'0');
                break;
        default:
                if (ccount>13)
                        yyerror("too many cards in ambiguous shape");
                bcopy(s, copy_s, 4);
                for(i=0; copy_s[i] != 'x'; i++)
                        ;
                if (xcount==1) {
                        copy_s[i] = 13-ccount+'0';      /* could go above '9' */
                        r = makeshape(copy_s, any);
                } else {
                        for (j=0; j<=13-ccount; j++) {
                                struct shape temp;
                                copy_s[i] = j+'0';
                                temp = makeshape(copy_s, any);
                                r = compineshape(&r, 0, &temp);
                        }
                }
                break;
        }
        return r;
}

int d2n(char s[4]) {
        static char copys[5];

        strncpy(copys, s, 4);
        return atoi(copys);
}

struct treebase *newtree(type, p1, p2, i1, i2)
int type;
struct treebase *p1, *p2;
int i1,i2;
{
        /* char *mycalloc(); */
        struct tree *p;

        p = (struct tree *) mycalloc(1, sizeof(*p));
        p->base.tr_type = type;
        p->tr_leaf1 = p1;
        p->tr_leaf2 = p2;
        p->tr_int1 = i1;
        p->tr_int2 = i2;
        return &p->base;
}

struct treebase *newhascardtree(int type, int compass, card c)
{
        struct treehascard *p;

        p = mycalloc(1, sizeof(*p));
        p->base.tr_type = type;
        p->compass = compass;
        p->c = c;
        return &p->base;
}

struct action *newaction(type, p1, s1, i1, p2)
int type;
struct treebase *p1;
char *s1;
int i1;
struct treebase *p2;
{
        /* char *mycalloc(); */
        struct action *a;

        a = (struct action *) mycalloc(1, sizeof(*a));
        a->ac_type = type;
        a->ac_expr1 = p1;
        a->ac_str1 = s1;
        a->ac_int1 = i1;
        a->ac_expr2 = p2;
        return a;
}

struct expr *newexpr(struct treebase* tr1, char* ch1, struct expr* ex1)
{
    struct expr* e;
    e=(struct expr*) mycalloc(1, sizeof(*e));
    e->ex_tr = tr1;
    e->ex_ch = ch1;
    e->next  = 0;
    if(ex1) {
        struct expr* exau = ex1;
            /* AM990705: the while's body had mysteriously disappeared, reinserted it */
            while(exau->next)
              exau = exau->next;
            exau->next = e;
            return ex1;
    } else {
        return e;
    }
}

char *mystrcpy(s)
char *s;
{
        char *cs;
        /* char *mycalloc(); */

        cs = mycalloc(strlen(s)+1, sizeof(char));
        strcpy(cs, s);
        return cs;
}

void predeal_holding(compass, holding)
int compass;
char *holding;
{
        char suit;

        suit = *holding++;
        while (*holding) {
                predeal(compass, make_card(*holding, suit));
                holding++;
        }
}


#define TRUNCZ(x) ((x)<0?0:(x))

extern int biasdeal[4][4];
static char *suit_name[] = {"Club", "Diamond", "Heart", "Spade"};


int bias_len(int compass){
  return
    TRUNCZ(gp->biasdeal[compass][0])+
    TRUNCZ(gp->biasdeal[compass][1])+
    TRUNCZ(gp->biasdeal[compass][2])+
    TRUNCZ(gp->biasdeal[compass][3]);
}

int bias_totsuit(int suit){
  return
    TRUNCZ(gp->biasdeal[0][suit])+
    TRUNCZ(gp->biasdeal[1][suit])+
    TRUNCZ(gp->biasdeal[2][suit])+
    TRUNCZ(gp->biasdeal[3][suit]);
}

void bias_deal(int suit, int compass, int length){
  if(gp->biasdeal[compass][suit]!=-1){
    char s[256];
    sprintf(s,"%s's %s suit has length already set to %d",
      player_name[compass],suit_name[suit],
      gp->biasdeal[compass][suit]);
    yyerror(s);
  }
  gp->biasdeal[compass][suit]=length;
  if(bias_len(compass)>13){
      char s[256];
    sprintf(s,"Suit lengths too long for %s",
      player_name[compass]);
    yyerror(s);
  }
  if(bias_totsuit(suit)>13){
    char s[256];
    sprintf(s,"Too many %ss",suit_name[suit]);
    yyerror(s);
  }
}


#define YY_USE_PROTOS

#ifdef WIN32
#pragma warning( disable : 4127 )
#endif

#include "scan.c"

#ifdef WIN32
#pragma warning( default : 4127 )
#endif


