
#pragma once

// C++ only header but can be included in mixed headers leading to C compiler
// including this too
#if __cplusplus

#include "detect.h"

/**
 * Entry is used to manage selected runtime function for C function entry points
 * to specialized optimization code.
 */
template<typename fn_t, fn_t* fn_ptr>
struct entry {
    constexpr entry() :
        current_{CPUDEFAULT}
    {}

    void register_alternative(unsigned required, fn_t fn)
    {
        if (current_ <= required &&
                cpu::detect::instance().supports(required)) {
            current_ = required;
            *fn_ptr = fn;
        }
    }
private:
    unsigned current_;
};

/**
 * class object to make sure static initialization setups function pointers
 * correctly.
 */
template<typename fn_t>
struct entry_register {
    template<typename entry_t>
    entry_register(unsigned required, entry_t &entry, fn_t fn) {
        entry.register_alternative(required, fn);
    }
};

/**
 * Helper function to register an alternative function
 */
template<typename fn_t, typename entry_t>
entry_register<fn_t> make_entry_register(unsigned required, entry_t &entry, fn_t fn)
{
    return {required, entry, fn};
}

#endif // __cplusplus
