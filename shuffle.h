#pragma once

#include "dealer.h"
#include "uniform_int.h"

#include <memory>

namespace DEFUN() {

/// Interface to shuffling code with varying implementations
struct shuffle {
    /// Virtual class must have virtual destructor
    virtual ~shuffle();
    /// Generate a hand based on user input
    virtual int do_shuffle(board* d, globals* gp) = 0;

    /// Generate a 32bit random number
    // Used for rnd() function in scripts
    virtual uint32_t random32(const fast_uniform_int_distribution<uint32_t, false> &dist) = 0;

    // Factory function to construct shuffle object based on state
    static std::unique_ptr<shuffle> factory(globals* gp);

protected:
    /// Construct shuffle from global state
    // @TODO: should be const if globals wouldn't be modified in runtime
    shuffle(globals* gp);
};

}

