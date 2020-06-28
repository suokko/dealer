#if MVdefault || MVpopcnt

#include "bittwiddle.h"

// The optimization namespace e.g. avx2_
namespace DEFUN() {

    /**
     * A fallback function to count set bits. It is only used if compiling
     * a call to ::popcount without instruction set support. Fallback is
     * compiled twice to generate instruction optimized version and open coded
     * bit operation versions for runtime selections.
     *
     * @param uv Unsigned integer value to count set bits
     * @return number of set bits in uv
     */
    template<typename uT>
    static inline uT popcounti(uT uv)
    {
#if __POPCNT__ && __GNUC__
        return popcount(uv);
#endif
	uT mask = -1;
	unsigned depth = 1;
	const unsigned muldepth = std::numeric_limits<uT>::digits == 16 ? 16 : 8;

	/* Sum each bit with their pair.
	 * Cheat by subtract to avoid a mask operation.
	 */
	mask = (~uT{0})/((uT{1} << depth)+1u);
	uv = uv - ((uv >> depth) & mask);
	depth *= 2;

	/* Sum every two bits. */
	mask = (~uT{0})/((uT{1} << depth)+1u);
	uv = (uv & mask) + ((uv >> depth) & mask);
	depth *= 2;

	/* Calculate sum for each byte.
	 * No overflow allows masking after sum.
	 */
	while (depth < muldepth) {
		mask = (~uT{0})/((uT{1} << depth)+1u);
		uv = (uv  + (uv >> depth)) & mask;
		depth *= 2;
	}
        // Is the bit count complete after addition steps?
	if (depth == std::numeric_limits<uT>::digits)
		return uv;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~uT{0})/((uT{1} << depth)-1u);
	return uT(uv * mask) >> (std::numeric_limits<uT>::digits - depth);
    }

    /// Runtime detected 8bit set bit count fallback entry
    auto pc8_register  = make_entry_register(::popcounti<uint8_t>, popcounti<uint8_t>);
    /// Runtime detected 16bit set bit count fallback entry
    auto pc16_register = make_entry_register(::popcounti<uint16_t>, popcounti<uint16_t>);
    /// Runtime detected 32bit set bit count fallback entry
    auto pc32_register = make_entry_register(::popcounti<uint32_t>, popcounti<uint32_t>);
    /// Runtime detected 62bit set bit count fallback entry
    auto pc64_register = make_entry_register(::popcounti<uint64_t>, popcounti<uint64_t>);
    /// Runtime detected long long set bit count fallback entry
    auto pcll_register = make_entry_register(::popcounti<unsigned long long>, popcounti<unsigned long long>);
}

#endif
