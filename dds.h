
#pragma once

#include "card.h"

union board;

extern int (*solve)(const union board *d, int declarer, int contract);
extern void (*solveLead)(const union board *d, int declarer, int contract, card cards, char res[13]);
