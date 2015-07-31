
#include "ghp.h"
#include "handpattern.h"

static const char *filename = "GHP.BIN";

GHP::GHP() : Patterns(filename, size())
{
}

GHP::~GHP()
{
}

