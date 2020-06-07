#pragma once

#include "pcg-cpp/include/pcg_random.hpp"
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
    unsigned random32(unsigned max);

    // Factory function to construct shuffle object based on state
    static std::unique_ptr<shuffle> factory(globals* gp);

protected:
    /// Construct shuffle from global state
    // @TODO: should be const if globals wouldn't be modified in runtime
    shuffle(globals* gp);

    pcg32_fast rng_;
};

}

