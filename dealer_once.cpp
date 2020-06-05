#include "dealer.h"

/**
 * Some declrations for dealer.cpp which must be compiled only once
 */

int (*deal_main)(struct globals *g) = nullptr;
entry<decltype(deal_main), &deal_main> deal_main_entry;
