
#include "pregen.h"

#include <utility>

#if !__cpp_constinit
#define constinit constexpr
#endif

#if __cplusplus > 202002L
using std::to_array;
#else

template<typename T, size_t N, size_t... I>
constexpr std::array<T, N> to_array_expand(T (&arr)[N], std::index_sequence<I...>)
{
	return {{arr[I]...}};
}

template<typename T, size_t N>
constexpr std::array<T, N> to_array(T (&arr)[N])
{
	return to_array_expand(arr, std::make_index_sequence<N>{});
}

#endif


template<typename T, unsigned max, unsigned min>
consteval auto ncr_lookup<T, max, min>::make_table() -> table_t
{
	struct lookup_t {
		constexpr T& operator()(unsigned n, unsigned r) {
			if (n < min)
				return small[index_raw(n, r)];
			else
				return rv[index(n, r)];
		}

		T small[min_index_+1];
		T rv[size_];
	} lookup{{0}, {0}};

	for (unsigned n = 0; n < max + 1; n++) {
		lookup(n, 0) = 1;
		lookup(n, n) = 1;
		for (unsigned r = 1; r < n; r++)
			lookup(n, r) = lookup(n - 1, r - 1) + lookup(n - 1, r);
	}
	return to_array(lookup.rv);
}

template<typename T, unsigned max, unsigned min>
consteval ncr_lookup<T, max, min>::ncr_lookup() :
	ncr_table_{make_table()}
{}

constinit const ncr_lookup<uint32_t, 26> ncrtable{};
constinit const ncr_lookup<uint64_t, 52, 27> ncrtablelarge{};

template<typename T, unsigned max, unsigned bits>
consteval auto prngtable<T, max, bits>::make_table() -> table_t
{
	T rv[max] = {0};

	for (T cards = 1; cards <= max; ++cards) {

		// calculate upper limit value to accept for uniform result
		T limit = -cards % cards;

		rv[cards - 1] = limit;
	}

	return to_array(rv);
}


template<typename T, unsigned max, unsigned bits>
constexpr prngtable<T, max, bits>::prngtable() :
	limits_{make_table()}
{}

constinit const prngtable<uint32_t, 52> prnglookup{};

consteval auto distrbitmaps::make_table() -> table_t
{
	value_type rv[std::tuple_size<table_t>()] = {0};
	value_type shape = 0;

	for (value_type clubs = 0; clubs <= 13; clubs++) {
		value_type max = 14 - clubs;
		value_type min = 0;
		value_type toadd = ((max+min)*(max-min+1))/2;
		rv[clubs] = shape;
		shape += toadd;
	}

	return to_array(rv);
}

constexpr distrbitmaps::distrbitmaps() :
	distributions_{make_table()}
{}

constinit const distrbitmaps getshapenumber{};
