#pragma once

#include <cassert>
#include <array>
#include <cstdint>
#include <limits>

#if !__cpp_consteval
#define consteval constexpr
#endif

/** Interface to query limits to make random numbers uniform.
 *
 * @param T The type for random numbers returned from generator
 * @param max The highest range size supported by the table
 * @param bits The number of bits each invocation uses from the random numbers
 */
template<typename T, unsigned max,
	unsigned bits = std::numeric_limits<T>::digits>
struct prngtable {
	static_assert(!std::numeric_limits<T>::is_signed,
		"Random numbers must be unsigned");

	using value_type = T;

	/// Construct the table
	constexpr prngtable();

	/// Helper to lookup values;
	T operator[](T range) const
	{
		assert(range <= max && "Range must be smaller than max");
		assert(range != 0 && "Range must not be empty");
		return limits_[range-1];
	}

protected:

	using table_t = std::array<T, max>;

	/// Helper to construct the table
	static consteval table_t make_table();

	table_t limits_;
};

extern const prngtable<uint32_t, 52> prnglookup;

/** Interface to lookup compile time calculated n choose r permutation numbers
 *
 * @param T The type to use for result values
 * @param max The maximum n value supported by the table
 * @param min The minimum n value supported by the table
 */
template<typename T, unsigned max, unsigned min = 0>
struct ncr_lookup {
	/** Construct compile time lookup table
	 */
	constexpr ncr_lookup();

	/** Lookup n choose r number of combinations
	 *
	 * @param n The number of available items
	 * @param r The number of items to pick
	 * @return The number of unique ways selection can be made
	 */
	T operator()(unsigned n, unsigned r) const
	{
		assert(n >= min && "n must be at least minimum");
		assert(n <= max && "n must be at most maximum");
		assert(r <= n && "r must be at most n");
		return ncr_table_[index(n,r)];
	}

protected:
	/** Raw index calculation helper
	 */
	static constexpr unsigned index_raw(unsigned n, unsigned r)
	{
		return (((n-1)*n)>>1)+r;
	}

	/// Index offset to the smallest n value in the table
	static constexpr unsigned min_index_ = index_raw(min, 0);
	/// Index offset to the end of the largest n value
	static constexpr unsigned max_index_ = index_raw(max, max);
	/// Size of the table
	static constexpr unsigned size_ = max_index_ - min_index_ + 1;

	/// Helper to calculate lookup index to the table
	static constexpr unsigned index(unsigned n, unsigned r)
	{
		return index_raw(n, r) - min_index_;
	}

	using table_t = std::array<T, size_>;

	/// Helper to construct the lookup table
	static consteval auto make_table() -> table_t;

	/// The table holding the lookup table
	table_t ncr_table_;
};

/// Lookup table for small ncr values up to 26 cards available for selection
extern const ncr_lookup<uint32_t, 26> ncrtable;
/// Lookup table for large ncr values up to 52 cards available for selection
extern const ncr_lookup<uint64_t, 52, 27> ncrtablelarge;

