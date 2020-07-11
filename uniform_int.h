#pragma once

#include "pregen.h"

#include <limits>
#include <cstdint>

template<typename T>
struct promote;

template<> struct promote<uint32_t> { using type = uint64_t; };



/**
 * Implement uniform int distribution for ranges between one and 52. It uses
 * compile time generate lookup table to filter values which result to uneven
 * distribution
 */
template<typename IntType, bool use_table = true>
struct fast_uniform_int_distribution {

    using result_type = IntType;

    struct param_type {
        param_type(IntType a, IntType b) :
            min_{a},
            range_{b - a + 1}
        {}

        bool operator()(IntType modulo) const
        {
            if (use_table)
                return modulo < prnglookup[range_];
            if (modulo >= range_)
                return false;
            const IntType limit = -range_ % range_;
            return modulo < limit;
        }

        IntType min_;
        IntType range_;
    };

    fast_uniform_int_distribution(IntType a,
            IntType b = std::numeric_limits<IntType>::max()) :
        params_{a, b}
    {}

    template<typename Generator>
    result_type operator()(Generator &g) const
    {
        typename Generator::result_type n;
        const result_type range = params_.range_;
        result_type low;
        do {
            // Scale the result from 0 to range - 1 in the high part of
            // multiplication result.
            n = mulhilo(g(), range, low);
            // Make sure we have uniform distribution
        } while(params_(low));

        // Multiply value by range to get uniform values from 0 to range-1
        return n + params_.min_;
    }
private:


    result_type mulhilo(result_type a, result_type b, result_type& lo) const
    {
        typename promote<result_type>::type res = a;
        res *= b;
        lo = res;
        return res >> std::numeric_limits<result_type>::digits;
    }

    param_type params_;
};

