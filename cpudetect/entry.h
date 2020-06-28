
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
 * Entry is used to manage the best fit selection for runtime optimization.
 * Entry provides call able function pointer like object which has pointer to
 * the function with the best fit optimization.
 */
template<typename fn_t>
struct entry {
    /**
     * Construct entry point which looks like a function pointer pointing to
     * null.
     */
    constexpr entry() :
        current_{CPUDEFAULT}
    {}

    /**
     * Register optimization alternative. If the alternative is better than
     * current best match for the cpu then the pointer is updated with the new
     * function.
     */
    void register_alternative(unsigned required, fn_t fn)
    {
        if (current_ <= required &&
                cpu::detect::instance().supports(required)) {
            current_ = required;
            fn_ = fn;
        }
    }

    /**
     * Call the underlying function pointer with same syntax as dump function
     * pointer would be called.
     *
     * This must not be called from any static initialization function. Static
     * initialization order is undefined making it impossible to know if
     * function pointer is a correct one at time of call.
     */
    template <typename... Args>
    auto operator()(Args&&... arg) const
    {
        return fn_(std::forward<Args>(arg)...);
    }
private:
    /// The best fit optimization for the cpu
    fn_t fn_;
    /// The feature set supported by the selected function pointer
    unsigned current_;
};

/**
 * A helper to register function pointer alternatives in static initialization
 * phase. Constructing a static object makes sure runtime detection is handled
 * for each function pointer correctly.
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
 * Helper function to register an alternative function with automated compiler
 * feature detection. This must be used in context which results to returned
 * object to initialize in static initialization phase.
 *
 * @param entry The function pointer which has fn as a potential alternative
 * @param entry The function pointer to register with entry
 * @return A object which registers the alternative to the entry
 */
template<typename fn_t, typename entry_t>
static inline entry_register<fn_t> make_entry_register(entry_t &entry, fn_t fn)
{
    return {cpu::detect::compiler_features(), entry, fn};
}

}

#endif // __cplusplus
