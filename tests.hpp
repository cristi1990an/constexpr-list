#include <array>
#include <utility>
#include <optional>

#include "list"

namespace testing{

	using namespace constexpr_list;

	template <std::size_t Index>
	constexpr void test(std::optional<list<int>> = {})
	{
		return;
	}

	template <>
	constexpr void test<0>(std::optional<list<int>> opt)
	{
		list<int> l;
		
		if (l.size() > 0)
		{
			throw "t0: size not zero";
		}

		for (int val : l)
		{
			throw "t0: iterator invalidated";
		}
	}

	template <>
	constexpr void test<1>(std::optional<list<int>> opt)
	{
		list<int> l = opt.value_or(list{ 1, 2, 3, 4 });

		if (l.size() != 4)
		{
			throw "t1: size not 4";
		}

		if (false == std::ranges::equal(l, std::array{ 1, 2, 3, 4 }))
		{
			throw "t1: range not valid";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{4, 3, 2, 1}))
		{
			throw "t1: reversed iterators invalidated";
		}
	}

	template <>
	constexpr void test<2>(std::optional<list<int>> opt)
	{

		list<int> l = opt.value_or(list{ 1, 2, 3, 4 });

		test<1>(l);

		l.push_back(5);
		l.push_front(0);

		if (l.front() != 0)
		{
			throw "t2: front element invalid";
		}

		if (l.back() != 5)
		{
			throw "t2: back element invalid";
		}

		if (l.size() != 6)
		{
			throw "t2: size not 6";
		}

		if (false == std::ranges::equal(l, std::array{ 0, 1, 2, 3, 4, 5}))
		{
			throw "t2: range not valid";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 5, 4, 3, 2, 1, 0 }))
		{
			throw "t2: reversed iterators invalidated";
		}

		l.reverse();

		if (false == std::ranges::equal(l, std::array{ 5, 4, 3, 2, 1, 0 }))
		{
			throw "t2: range not valid after reverse";
		}

		l.reverse();

		if (false == std::ranges::equal(l, std::array{ 0, 1, 2, 3, 4, 5 }))
		{
			throw "t2: range not valid after reverse";
		}

		l.erase(std::ranges::next(l.begin(), 3));

		if (l.size() != 5)
		{
			throw "t2: size not 5 after erase";
		}

		if (false == std::ranges::equal(l, std::array{ 0, 1, 2, 4, 5 }))
		{
			throw "t2: range not valid after erase";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 5, 4, 2, 1, 0 }))
		{
			throw "t2: reversed iterators invalidated after erase";
		}

		l.erase(std::ranges::next(l.begin(), 1), std::ranges::next(l.begin(), 3));

		if (l.size() != 3)
		{
			throw "t2: size not 3 after erase";
		}

		if (false == std::ranges::equal(l, std::array{ 0, 4, 5 }))
		{
			throw "t2: range not valid after erase";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 5, 4, 0 }))
		{
			throw "t2: reversed iterators invalidated after erase";
		}

		l.pop_back();

		if (l.size() != 2)
		{
			throw "t2: size not 2 after pop_back";
		}

		if (false == std::ranges::equal(l, std::array{ 0, 4 }))
		{
			throw "t2: range not valid after pop_back";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 4, 0 }))
		{
			throw "t2: reversed iterators invalidated after pop_back";
		}

		l.pop_front();

		if (l.size() != 1)
		{
			throw "t2: size not 1 after pop_back";
		}

		if (false == std::ranges::equal(l, std::array{ 4 }))
		{
			throw "t2: range not valid after pop_front";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 4 }))
		{
			throw "t2: reversed iterators invalidated after pop_front";
		}

		l.resize(5);

		if (l.size() != 5)
		{
			throw "t2: size not 5 after resize";
		}

		if (false == std::ranges::equal(l, std::array{ 4, 0, 0, 0, 0}))
		{
			throw "t2: range not valid after resize";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 0, 0, 0, 0, 4 }))
		{
			throw "t2: reversed iterators invalidated after resize";
		}

		l.resize(2);

		if (l.size() != 2)
		{
			throw "t2: size not 2 after resize";
		}

		if (false == std::ranges::equal(l, std::array{ 4, 0 }))
		{
			throw "t2: range not valid after resize";
		}

		if (false == std::ranges::equal(l | std::views::reverse, std::array{ 0, 4 }))
		{
			throw "t2: reversed iterators invalidated after resize";
		}

		l.clear();

		for (int val : l)
		{
			throw "t2: range invalidated after clear";
		}

		if (!l.empty())
		{
			throw "t2: range not empty after clear";
		}
	}

	template <>
	constexpr void test<3>(std::optional<list<int>> opt)
	{
		list<int> l = opt.value_or(list<int>{});
		l.assign({ 1, 2, 3, 4 });
		test<2>(l);
	}

	template <>
	constexpr void test<4>(std::optional<list<int>> opt)
	{
		test<3>(list{ 1, 2, 4, 5, 6, 7 });
		test<3>(list{ 1, 2 });
		test<3>(list{ 4, 3, 2, 1 });
	}

	template<>
	constexpr void test<5>(std::optional<list<int>> opt)
	{
		list<int> l = opt.value_or(list<int>{});
		list tmp = { 1, 2, 3, 4 };
		l.assign(tmp.begin(), tmp.end());
		test<2>(l);
	}

	template <>
	constexpr void test<7>(std::optional<list<int>> opt)
	{
		test<5>(list{ 1, 2, 4, 5, 6, 7 });
		test<5>(list{ 1, 2 });
		test<5>(list{ 4, 3, 2, 1 });
	}

	template <>
	constexpr void test<8>(std::optional<list<int>> opt)
	{
		list l = { 4, 1, 3, 2 };
		l.sort();
		test<2>(l);
	}

	template <>
	constexpr void test<9>(std::optional<list<int>> opt)
	{
		list<int> l;
		l.sort();

		for (int val : l)
		{
			throw "t9: empty range invalidated after sort";
		}

		if (!l.empty())
		{
			throw "t9: range not empty after sort";
		}
	}

	template <>
	constexpr void test<10>(std::optional<list<int>> opt)
	{
		list l = { 1, 4 };

		l.insert(std::ranges::next(l.begin()), { 2, 3 });

		test<2>(l);
	}

	template <>
	constexpr void test<11>(std::optional<list<int>> opt)
	{
		list<int> l;

		l.insert(l.end(), {1, 2, 3, 4});

		test<2>(l);
	}

	template <>
	constexpr void test<12>(std::optional<list<int>> opt)
	{
		list l = { 1, 2 };

		l.append_range(std::array{ 3, 4 });

		test<2>(l);
	}

	template <>
	constexpr void test<13>(std::optional<list<int>> opt)
	{
		list l = { 3, 4};

		l.prepend_range(std::array{ 1, 2 });

		test<2>(l);
	}

	template <>
	constexpr void test<14>(std::optional<list<int>> opt)
	{
		list l1 = { 1, 2, 3, 4 };
		list l2 = { 4, 3, 2, 1 };
		std::ranges::swap(l1, l2);
		l1.reverse();

		test<2>(l1);
		test<2>(l2);
	}

	constexpr bool all_tests_passed()
	{
		[]<std::size_t ... I>(std::index_sequence<I...>)
		{
			(test<I>(), ...);
		}(std::make_index_sequence<100>());
		return true;
	}
}

static_assert(testing::all_tests_passed());
