
#pragma once

union board;

/**
 * Print deal in the pbn format
 * @param board The board number used for vulnerability and dealer
 * @param b The deal to print
 * @return 0 on success
 */
int printpbn (int board, const union board *b);

