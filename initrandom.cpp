
#include "initrandom.h"

#include <random>


struct initrng {
    initrng() : dev_{} {};

    int operator()(int min, int max)
    {
        std::uniform_int_distribution<> dist(min, max);
        return dist(dev_);
    }

private:
    std::random_device dev_;
};

struct initrng* irng_open()
{
    return new initrng();
}

int irng_next(struct initrng *r, int min, int max)
{
    return (*r)(min, max);
}

void irng_close(struct initrng *r)
{
    delete r;
}
