#pragma once
#include <stdint.h>

struct tagLibdeal {
  uint32_t suits[4];
  uint16_t tricks[5];
  int valid;
};

