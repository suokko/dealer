#include "gsx.h"
#include "shp.h"
#include "ghp.h"

static const char *filename = "GSX.BIN";

GSX::GSX() : SidePrints(filename),
	ghp_(new GHP()),
	shp_(new SHP())
{}

GSX::~GSX()
{
	delete ghp_;
	delete shp_;
}

patternpair GSX::operator[](unsigned idx) const
{
	unsigned startend = shape_[idx] & 0x3,
		 factor = shape_[idx] >> 8 & 0x3f,
		 sidx = shape_[idx] >> 16 & 0x3ff,
		 gidx = shape_[idx] >> 26;

	patternpair r(startend, factor, (*ghp_)[gidx], (*shp_)[sidx]);
	return r;
}

patternpair::patternpair(unsigned se, unsigned f, const handpattern &p0, const handpattern &p1) :
	startend_(se),
	factor_(f),
	p0_(p0),
	p1_(p1)
{}

unsigned patternpair::se() const { return startend_; }
unsigned patternpair::factor() const { return factor_; }
const handpattern & patternpair::pattern(unsigned idx) const { return idx == 0 ? p0_ : p1_; }

#include <iomanip>

std::ostream &operator<<(std::ostream &os, const patternpair &pp)
{
	return os << std::hex << pp.se() << " "
		<< std::dec << pp.factor() << " "
		<< pp.pattern(0) << " " << pp.pattern(1);
}
