
#pragma once

// C++ only header but can be included in mixed headers leading to C compiler
// including this too
#if __cplusplus

#include "detect.h"

#ifdef MVDEFAULT
#define CONCAT2(a,b) a ##_## b
#define CONCAT(a,b) CONCAT2(a,b)
#define DEFUN(x) CONCAT(MVDEFAULT, x)
#else
#define DEFUN(x) default_##x
#endif

/**
 * Entry is used to manage selected runtime function for C function entry points
 * to specialized optimization code.
 */
template<typename fn_t>
struct entry {
    constexpr entry() :
        current_{CPUDEFAULT}
    {}

    void register_alternative(unsigned required, fn_t fn)
    {
        if (current_ <= required &&
                cpu::detect::instance().supports(required)) {
            current_ = required;
            fn_ = fn;
        }
    }

    template <typename... Args>
    auto operator()(Args&&... arg) const
    {
        return fn_(std::forward<Args>(arg)...);
    }
private:
    fn_t fn_;
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

namespace DEFUN() {

/**
 * Helper function to register an alternative function
 */
template<typename fn_t, typename entry_t>
static inline entry_register<fn_t> make_entry_register(entry_t &entry, fn_t fn)
{
    return {cpu::detect::compiler_features(), entry, fn};
}

}

#endif // __cplusplus
