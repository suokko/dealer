#pragma once

#include "handpattern.h"
#include <ostream>

class patterntriplet {
	unsigned startend_, factor_;
	handpattern p0_, p1_, p2_;
public:
	patterntriplet(unsigned se, unsigned f,
			const handpattern &p1, const handpattern &p2,
			const handpattern &p3);

	const handpattern &pattern(unsigned idx) const;

	unsigned se() const;
	unsigned factor() const;
};

class GHP;
class SHP;

class GDX : public SidePrints {

	GHP *ghp_;
	SHP *shp_;
public:
	GDX();
	~GDX();

	unsigned size() const {return 393197;}

	patterntriplet operator[](unsigned idx) const;
};

std::ostream &operator<<(std::ostream &os, const patterntriplet &pp);
