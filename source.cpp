#include <print>
#include <array>

#include "constexpr_list.hpp"

consteval bool test()
{
	using namespace std::literals;
	constexpr_list::list l{ "test1"s, "test2"s, "test3"s };
	constexpr_list::list cpy = l;
	l.emplace_back("multaaaaaaaaaaaaaaaaaaaaaaaaaaaae");
	l.begin();
	l.pop_back();
	l.pop_front();
	l.clear();
	l.swap(cpy);
	l.emplace_back("test4");
	l.swap(cpy);
	cpy.front();
	l <=> cpy;
	l = std::ranges::to<constexpr_list::list>(cpy);
	return l.back() == "test4";
}

consteval bool test_2()
{
	using namespace constexpr_list;

	list l = { 5, 2, 3, 5, 1, 2, 3 };
	l.sort();

	return std::ranges::is_sorted(l) && std::ranges::equal(l, std::array{1, 2, 2, 3, 3, 5, 5});
}


int main()
{
	static_assert(test());
	static_assert(test_2());
}