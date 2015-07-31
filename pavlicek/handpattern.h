#pragma once

#include <stdint.h>
#include <ostream>
#include <utility>

class handpattern {
	uint32_t pattern_;
	uint64_t combinations_;

public:
	handpattern(uint32_t pattern, uint64_t combinations);

	uint32_t pattern() const;
	uint64_t combinations() const;
};

class SidePrints {
protected:
	uint32_t *shape_;
	unsigned size_;

	SidePrints(const char* filename);
	~SidePrints();
public:
};

class Patterns : public SidePrints {
	uint64_t *combinations_;
protected:
	Patterns(const char *filename, unsigned count);
	~Patterns();

public:
	handpattern operator[](unsigned idx) const;
};

std::ostream &operator<<(std::ostream &os, const handpattern &hp);
