#if MVdefault || MVpopcnt

#include "bittwiddle.h"


namespace DEFUN() {

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
	if (depth == std::numeric_limits<uT>::digits)
		return uv;

	/* Multiplication can sum all bytes because results fits to a byte. */
	mask = (~uT{0})/((uT{1} << depth)-1u);
	return uT(uv * mask) >> (std::numeric_limits<uT>::digits - depth);
    }

    auto pc8_register  = make_entry_register(::popcounti<uint8_t>, popcounti<uint8_t>);
    auto pc16_register = make_entry_register(::popcounti<uint16_t>, popcounti<uint16_t>);
    auto pc32_register = make_entry_register(::popcounti<uint32_t>, popcounti<uint32_t>);
    auto pc64_register = make_entry_register(::popcounti<uint64_t>, popcounti<uint64_t>);
    auto pcll_register = make_entry_register(::popcounti<unsigned long long>, popcounti<unsigned long long>);
}

#endif
