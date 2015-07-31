#pragma once

#include "handpattern.h"
#include <ostream>

class patternpair {
	unsigned startend_, factor_;
	handpattern p0_, p1_;
public:
	patternpair(unsigned se, unsigned f, const handpattern &p1, const handpattern &p2);

	const handpattern &pattern(unsigned idx) const;

	unsigned se() const;
	unsigned factor() const;
};

class GHP;
class SHP;

class GSX : public SidePrints {

	GHP *ghp_;
	SHP *shp_;
public:
	GSX();
	~GSX();

	unsigned size() const {return 8048;}

	patternpair operator[](unsigned idx) const;
};

std::ostream &operator<<(std::ostream &os, const patternpair &pp);
