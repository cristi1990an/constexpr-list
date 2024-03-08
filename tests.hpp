#include <array>
#include <utility>
#include <optional>
#include <print>

#include "constexpr_list.hpp"

namespace testing{

	using namespace constexpr_list;

	struct tracker
	{
		std::size_t allocations = 0;
		std::size_t deallocations = 0;
		std::size_t constructions = 0;
		std::size_t destructions = 0;

		constexpr bool valid() noexcept
		{
			return allocations == deallocations
				&& allocations == constructions
				&& allocations == destructions;
		}
	};

	template <typename T>
	struct allocator_tracker : std::allocator<T>
	{
		template <typename U>
		friend struct allocator_tracker;

		constexpr allocator_tracker(tracker& tr)
			: tracker_ { &tr }
		{}
		allocator_tracker() = default;
		allocator_tracker(const allocator_tracker&) = default;

		using traits = std::allocator_traits<std::allocator<T>>;

		template <typename U>
		constexpr allocator_tracker(const allocator_tracker<U>& other)
			: tracker_ { other.tracker_ }
		{}

		constexpr T* allocate(std::size_t n)
		{
			if (tracker_)
			{
				tracker_->allocations++;
			}
			return underlying().allocate(n);
		}

		constexpr void deallocate(T* p, std::size_t n)
		{
			if (tracker_)
			{
				tracker_->deallocations++;
			}
			underlying().deallocate(p, n);
		}

		template <typename U, typename ... Args>
		constexpr void construct(U* p, Args&& ... args)
		{
			if (tracker_)
			{
				tracker_->constructions++;
			}
			traits::construct(underlying(), p, std::forward<Args>(args)...);
		}

		template <typename U>
		constexpr void destroy(U* p)
		{
			if (tracker_)
			{
				tracker_->destructions++;
			}
			traits::destroy(underlying(), p);
		}

		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;

		constexpr std::allocator<T>& underlying()
		{
			return static_cast<std::allocator<T>&>(*this);
		}
		
		tracker* tracker_ = nullptr;
	};

	template <typename T>
	using tracked_list = list<T, allocator_tracker<T>>;

	using opt_list = std::optional<tracked_list<int>>;

	template <std::size_t Index>
	constexpr void test(opt_list = {})
	{
		return;
	}

	template <>
	constexpr void test<0>(opt_list opt)
	{
		tracker tr;
		{
			tracked_list<int> l(tr);

			auto alloc = l.get_allocator();

			if (l.size() > 0)
			{
				throw "t0: size not zero";
			}

			for (int val : l)
			{
				throw "t0: iterator invalidated";
			}
		}

		if (tr.allocations != 0 || !tr.valid())
		{
			throw "t0: empty listed did allocation";
		}
	}

	template <>
	constexpr void test<1>(opt_list opt)
	{
		tracker tr;
		{
			tracked_list<int> l = opt.value_or(tracked_list<int>({ 1, 2, 3, 4 }, tr));

			if (l.size() != 4)
			{
				throw "t1: size not 4";
			}

			if (false == std::ranges::equal(l, std::array{ 1, 2, 3, 4 }))
			{
				throw "t1: range not valid";
			}

			if (false == std::ranges::equal(l | std::views::reverse, std::array{ 4, 3, 2, 1 }))
			{
				throw "t1: reversed iterators invalidated";
			}
		}
		if (!tr.valid())
		{
			throw "t1: allocator invalid state";
		}
	}

	template <>
	constexpr void test<2>(opt_list opt)
	{

		tracked_list<int> l = opt.value_or(tracked_list<int>{ 1, 2, 3, 4 });

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
	constexpr void test<3>(opt_list opt)
	{
		tracked_list<int> l = opt.value_or(tracked_list<int>{});
		l.assign({ 1, 2, 3, 4 });
		test<2>(l);
	}

	template <>
	constexpr void test<4>(opt_list opt)
	{
		test<3>(tracked_list<int>{ 1, 2, 4, 5, 6, 7 });
		test<3>(tracked_list<int>{ 1, 2 });
		test<3>(tracked_list<int>{ 4, 3, 2, 1 });
	}

	template<>
	constexpr void test<5>(opt_list opt)
	{
		tracked_list<int> l = opt.value_or(tracked_list<int>{});
		list tmp = { 1, 2, 3, 4 };
		l.assign(tmp.begin(), tmp.end());
		test<2>(l);
	}

	template <>
	constexpr void test<7>(opt_list opt)
	{
		test<5>(tracked_list<int>{ 1, 2, 4, 5, 6, 7 });
		test<5>(tracked_list<int>{ 1, 2 });
		test<5>(tracked_list<int>{ 4, 3, 2, 1 });
	}

	template <>
	constexpr void test<8>(opt_list opt)
	{
		tracked_list<int> l = { 4, 1, 3, 2 };
		l.sort();
		test<2>(l);
	}

	template <>
	constexpr void test<9>(opt_list opt)
	{
		tracked_list<int> l;
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
	constexpr void test<10>(opt_list opt)
	{
		tracked_list<int> l = { 1, 4 };

		l.insert(std::ranges::next(l.begin()), { 2, 3 });

		test<2>(l);
	}

	template <>
	constexpr void test<11>(opt_list opt)
	{
		tracked_list<int> l;

		l.insert(l.end(), {1, 2, 3, 4});

		test<2>(l);
	}

	template <>
	constexpr void test<12>(opt_list opt)
	{
		tracked_list<int> l = { 1, 2 };

		l.append_range(std::array{ 3, 4 });

		test<2>(l);
	}

	template <>
	constexpr void test<13>(opt_list opt)
	{
		tracked_list<int> l = { 3, 4};

		l.prepend_range(std::array{ 1, 2 });

		test<2>(l);
	}

	template <>
	constexpr void test<14>(opt_list opt)
	{
		tracked_list<int> l1 = { 1, 2, 3, 4 };
		tracked_list<int> l2 = { 4, 3, 2, 1 };
		std::ranges::swap(l1, l2);
		l1.reverse();

		test<2>(l1);
		test<2>(l2);
	}

	template <>
	constexpr void test<15>(opt_list opt)
	{
		tracked_list<int> l = { 1, 2, 2, 2, 3, 3, 4, 4, 4 };
		auto erased = l.unique();

		if (erased != 5)
		{
			throw "t15: erased counter not valid";
		}

		test<2>(l);
	}

	template <>
	constexpr void test<16>(opt_list opt)
	{
		tracker tr;

		{
			tracked_list<int> l1 ({ 1, 3 }, tr);
			tracked_list<int> l2 ({ 2, 4 }, tr);

			l1.merge(l2);
		}

		if (!tr.valid())
		{
			throw "t16: allocator invalid state";
		}
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
