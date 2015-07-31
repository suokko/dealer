#include "handpattern.h"
#include <exception>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

handpattern::handpattern(uint32_t pattern, uint64_t combinations) :
	pattern_(pattern),
	combinations_(combinations)
{}

uint32_t handpattern::pattern() const
{
	return pattern_;
}

uint64_t handpattern::combinations() const
{
	return combinations_;
}

#include <iomanip>

std::ostream &operator<<(std::ostream &os, const handpattern &hp)
{
	unsigned len1 = hp.pattern() & 0xff,
		 len2 = hp.pattern() >> 8 & 0xff,
		 len3 = hp.pattern() >> 16 & 0xff,
		 len4 = hp.pattern() >> 24 & 0xff;

	return os << std::hex << len1 << len2 << len3 << len4
		/* << ' ' << hp.combinations() */ << std::dec;
}

SidePrints::SidePrints(const char *filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		throw std::exception();

	struct stat buf;
	fstat(fd, &buf);
	size_ = buf.st_size;

	shape_ = (uint32_t*)mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (shape_ == MAP_FAILED)
		throw std::exception();
	close(fd);
}

SidePrints::~SidePrints()
{
	munmap(shape_, size_);
}

Patterns::Patterns(const char *filename, unsigned count) :
	SidePrints(filename)
{
	combinations_ = (uint64_t*)(shape_ + count);
}

Patterns::~Patterns()
{
}

handpattern Patterns::operator[](unsigned idx) const
{
	handpattern pattern(shape_[idx], combinations_[idx]);
	return pattern;
}
