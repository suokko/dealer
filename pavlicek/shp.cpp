
#include "shp.h"
#include "handpattern.h"

static const char *filename = "SHP.BIN";

SHP::SHP() : Patterns(filename, size())
{
}

SHP::~SHP()
{
}

