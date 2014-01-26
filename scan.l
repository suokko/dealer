/* $Header: /home/henk/CVS/dealer/scan.l,v 1.8 1999/07/05 19:44:30 henk Exp $ */
%x COMMENT
%%
"&&"            return(AND2);
and             return(AND2);
"||"            return(OR2);
or              return(OR2);
"!"             return(NOT);
not             return(NOT);
"=="            return(CMPEQ);
"!="            return(CMPNE);
"<"             return(CMPLT);
"<="            return(CMPLE);
">"             return(CMPGT);
">="            return(CMPGE);
"+"             return(ARPLUS);
"-"             return(ARMINUS);
"*"             return(ARTIMES);
"/"             return(ARDIVIDE);
"%"             return(ARMOD);
"?"             return(QUERY);
":"             return(COLON);
"//".*\n        {lino++;}
"/*"            BEGIN(COMMENT);
<COMMENT>[^*\n<<EOF>>]*
<COMMENT>"*"+[^*/\n<<EOF>]*
<COMMENT>\n     lino++;
<COMMENT><<EOF>> yyerror("EOF found in comment");
<COMMENT>"*"+"/" BEGIN(INITIAL);
generate        return(GENERATE);
produce         return(PRODUCE);
dealer          return(DEALER);
vulnerable      return(VULNERABLE);
predeal         return(PREDEAL);
pointcount      return(POINTCOUNT);
altcount        return(ALTCOUNT);
condition       return(CONDITION);
action          return(ACTION);
print           return(PRINT);
printall        return(PRINTALL);
printpbn        return(PRINTPBN);
printew         return(PRINTEW);
printcompact    return(PRINTCOMPACT);
printoneline    return(PRINTONELINE);
printes         return(PRINTES);
evalcontract    return(EVALCONTRACT);
nv              { yylval.y_int = NON_VUL; return VULN; }
vul             { yylval.y_int = VUL; return VULN; }
ns              { yylval.y_int = VULNERABLE_NS; return VULNERABLE; }
NS              { yylval.y_int = VULNERABLE_NS; return VULNERABLE; }
ew              { yylval.y_int = VULNERABLE_EW; return VULNERABLE; }
EW              { yylval.y_int = VULNERABLE_EW; return VULNERABLE; }
all             { yylval.y_int = VULNERABLE_ALL; return VULNERABLE; }
none            { yylval.y_int = VULNERABLE_NONE; return VULNERABLE; }
average         return(AVERAGE);
frequency       return(FREQUENCY);
north           { yylval.y_int = COMPASS_NORTH; return COMPASS; }
east            { yylval.y_int = COMPASS_EAST; return COMPASS; }
south           { yylval.y_int = COMPASS_SOUTH; return COMPASS; }
west            { yylval.y_int = COMPASS_WEST; return COMPASS; }
clubs?          { yylval.y_int = SUIT_CLUB; return SUIT; }
diamonds?       { yylval.y_int = SUIT_DIAMOND; return SUIT; }
hearts?         { yylval.y_int = SUIT_HEART; return SUIT; }
spades?         { yylval.y_int = SUIT_SPADE; return SUIT; }
notrumps?   return NOTRUMPS;
hcps?       return(HCP);
pt0                 return(PT0);
tens?       return(PT0);
pt1                 return(PT1);
jacks?          return(PT1);
pt2                 return(PT2);
queens?         return(PT2);
pt3                 return(PT3);
kings?          return(PT3);
pt4                 return(PT4);
aces?           return(PT4);
pt5                 return(PT5);
top2            return(PT5);
pt6                 return(PT6);
top3            return(PT6);
pt7                 return(PT7);
top4            return(PT7);
pt8                 return(PT8);
top5            return(PT8);
pt9                 return(PT9);
c13                 return(PT9);
losers?         return(LOSER);
controls?   return(CONTROL);
quality         return(QUALITY);
cccc            return(CCCC);
tricks?     return(TRICKS);
score       return(SCORE);
imps?       return(IMPS);
hascard         return(HASCARD);
shape           return(SHAPE);
any                 return(ANY);
rnd         return(RND);
[CDHS]A?K?Q?J?T?9?8?7?6?5?4?3?2?        { yylval.y_str = mystrcpy(yytext); return HOLDING; }
[2-9TJQKA][CDHS]        { yylval.y_int = make_card(yytext[0], yytext[1]);
                          return(CARD); }
[x][1-7][CDHSN] { yylval.y_int = make_contract(yytext[2], yytext[1]);
                          return(CONTRACT);}
\"[^"]*\"       { yytext[yyleng-1] = 0; yylval.y_str = mystrcpy(yytext+1); return STRING; }
\\n             { yylval.y_str = mystrcpy("\n"); return STRING; }
[0-9x]{4}       { strncpy(yylval.y_distr, yytext, 4); return strchr(yytext, 'x') ? DISTR
 : DISTR_OR_NUMBER; }
[0-9]+          { yylval.y_int = atoi(yytext); return NUMBER; }

 /* AM990705: removed a production to fix unary-minus syntax glitch         */
 /* [-][0-9]+               { yylval.y_int = atoi(yytext); return NUMBER; } */

[a-zA-Z][a-zA-Z0-9_]*   { yylval.y_str = mystrcpy(yytext); return IDENT; }
[ \t]           ;
\r*\n           { lino++; }
^\#.*\n         { lino++; }
.               return(yytext[0]);

