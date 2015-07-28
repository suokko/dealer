
#ifdef __cplusplus
extern "C" {
#endif

#include "card.h"

struct board;

extern int (*solve)(const struct board *d, int declarer, int contract);
extern void (*solveLead)(const struct board *d, int declarer, int contract, card cards, char res[13]);

#ifdef __cplusplus
}
#endif
