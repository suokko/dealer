#pragma once
#include <stdint.h>

/**
 * library.dat format is a binary table with tagLibdeal elements
 */
struct tagLibdeal {
  uint32_t suits[4];
  uint16_t tricks[5];
};

