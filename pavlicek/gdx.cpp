#include "gdx.h"
#include "shp.h"
#include "ghp.h"

static const char *filename = "GDX.BIN";

GDX::GDX() : SidePrints(filename),
	ghp_(new GHP()),
	shp_(new SHP())
{}

GDX::~GDX()
{
	delete ghp_;
	delete shp_;
}

patterntriplet GDX::operator[](unsigned idx) const
{
	unsigned startend = shape_[idx] & 0x3,
		 factor = shape_[idx] >> 2 & 0x7,
		 sidx = shape_[idx] >> 6 & 0x3ff,
		 sidx2 = shape_[idx] >> 16 & 0x3ff,
		 gidx = shape_[idx] >> 26;

	patterntriplet r(startend, factor, (*ghp_)[gidx], (*shp_)[sidx], (*shp_)[sidx2]);
	return r;
}

patterntriplet::patterntriplet(unsigned se, unsigned f,
		const handpattern &p0, const handpattern &p1,
		const handpattern &p2) :
	startend_(se),
	factor_(f*8),
	p0_(p0),
	p1_(p1),
	p2_(p2)
{}

unsigned patterntriplet::se() const { return startend_; }
unsigned patterntriplet::factor() const { return factor_; }
const handpattern & patterntriplet::pattern(unsigned idx) const { return idx == 0 ? p0_ : (idx == 1 ? p1_ : p2_); }

#include <iomanip>

std::ostream &operator<<(std::ostream &os, const patterntriplet &pp)
{
	return os << std::hex << pp.se() << " "
		<< std::dec << pp.factor() << " "
		<< pp.pattern(0) << " " << pp.pattern(1) << " " << pp.pattern(2);
}
