#ifndef CONSTEXPR_LIST
#define CONSTEXPR_LIST

#include <algorithm>
#include <ranges>
#include <concepts>
#include <type_traits>
#include <memory>

namespace constexpr_list
{
	namespace detail
	{
		template <typename R, typename T>
		concept container_compatible_range =
			std::ranges::input_range<R> &&
			std::convertible_to<std::ranges::range_reference_t<R>, T>;

		template <typename B>
		concept boolean_testable =
			std::convertible_to<B, bool>&&
			requires (B&& b)
		{
			{ !std::forward<B>(b) } -> std::convertible_to<bool>;
		};

		static constexpr struct synth_three_way_fn
		{
			template <typename T, typename U>
				requires requires (const T& t, const U& u)
			{
				{ t < u } -> boolean_testable;
				{ u < t } -> boolean_testable;
			}
			constexpr auto operator()(const T& t, const U& u) const
			{
				if constexpr (std::three_way_comparable_with<T, U>)
				{
					return t <=> u;
				}
				else
				{
					if (t < u)
					{
						return std::weak_ordering::less;
					}
					if (u < t)
					{
						return std::weak_ordering::greater;
					}
					return std::weak_ordering::equivalent;
				}
			}

			using is_transparent = int;

		} synth_three_way;

		template <typename T, typename U = T>
		using synth_three_way_result =
			decltype(synth_three_way(std::declval<T&>(), std::declval<U&>()));
	}

	template<
		typename T,
		typename Allocator = std::allocator<T>
	>
	class list
	{
		template <bool Const>
		struct iterator_base;

		static_assert(std::copy_constructible<T>, "T is required to be copy-constructible");
		static_assert(!std::is_reference_v<T>, "T cannot be a reference type");
		static_assert(!std::is_void_v<T>, "T cannot be void");
		static_assert(std::is_destructible_v<T>, "T must be destructible");

	public:
		using value_type = T;
		using allocator_type = Allocator;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using iterator = iterator_base<false>;
		using const_iterator = iterator_base<true>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		constexpr list() noexcept = default;

		constexpr list(const Allocator& alloc)
			: alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(alloc))
		{}

		constexpr list(size_type count,
			const allocator_type& alloc = allocator_type()) requires (std::is_default_constructible_v<T>)
			: list(alloc)
		{
			while (count)
			{
				this->emplace_back();
				--count;
			}
		}

		constexpr list(std::initializer_list<T> il,
			const allocator_type& alloc = allocator_type())
			: list(il.begin(), il.end(), alloc)
		{
		}

		template <typename U> requires std::constructible_from<T, const U&> && (!std::same_as<T, U>)
		constexpr list(std::initializer_list<U> il,
			const allocator_type& alloc = allocator_type())
			: list(il.begin(), il.end(), alloc)
		{
		}

		template <std::input_iterator InputIt, std::sentinel_for<InputIt> S>
			requires std::constructible_from<T, std::iter_reference_t<InputIt>>
		constexpr list(InputIt first, S last,
			const Allocator& alloc = Allocator())
			: alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(alloc))
		{
			this->insert(this->end(), std::move(first), std::move(last));
		}

		constexpr list(const list& other)
			: list(other.begin(), other.end())
		{

		}

		constexpr list(list&& other)
			noexcept(std::allocator_traits<node_allocator>::is_always_equal::value)
			: ptrs_{ other.ptrs_.next_, other.ptrs_.prev_ }
			, size_{ other.size_ }
			, alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.get_allocator()))
		{
			if (size_ == 0) [[unlikely]]
			{
				ptrs_ = links_{ &ptrs_, &ptrs_ };
			}

			other.ptrs_.next_->prev_ = &ptrs_;
			other.ptrs_.prev_->next_ = &ptrs_;
			other.size_ = 0;
			other.ptrs_ = links_{ &other.ptrs_, &other.ptrs_ };
		}

		constexpr list(size_type count,
			const T& value = T(),
			const allocator_type& alloc = allocator_type())
			: alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(alloc))
		{
			while (count)
			{
				this->push_back(value);
				--count;
			}
		}

		template <detail::container_compatible_range<T> R>
		constexpr list(std::from_range_t, R&& rg,
			const allocator_type& alloc = allocator_type())
			: alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(alloc))
		{
			auto begin = std::ranges::begin(rg);
			auto end = std::ranges::end(rg);

			for (; begin != end; ++begin)
			{
				this->push_back(*begin);
			}
		}

		constexpr list& operator=(const list& other)
		{
			if constexpr (std::allocator_traits<node_allocator>::propagate_on_container_copy_assignment::value)
			{
				if (alloc_ != other.alloc_)
				{
					this->reset();
				}

				alloc_ = std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.alloc_);
			}

			const_iterator mid_point = std::ranges::next(other.begin(), this->size(), other.end());
			const auto copyable = std::ranges::subrange(other.begin(), mid_point);
			const auto new_ones = std::ranges::subrange(mid_point, other.end());

			std::ranges::copy(copyable, this->begin());
			for (const auto& value : new_ones)
			{
				this->push_back(value);
			}

			return *this;
		}

		template <typename U> requires std::assignable_from<T&, const U&>&& std::constructible_from<T, const U&>
		constexpr list& operator=(std::initializer_list<U> ilist)
		{
			auto it = std::ranges::next(ilist.begin(),
				static_cast<difference_type>(this->size()), ilist.end());
			auto copyable = std::ranges::subrange{ ilist.begin(), it };
			auto new_ones = std::ranges::subrange{ it, ilist.end() };

			std::ranges::copy(copyable, this->begin());
			for (const auto& value : new_ones)
			{
				this->push_back(value);
			}

			return *this;
		}

		constexpr list& operator=(list&& other)
			noexcept(std::allocator_traits<node_allocator>::is_always_equal::value)
		{
			if constexpr (std::allocator_traits<node_allocator>::propagate_on_container_move_assignment::value)
			{
				if (alloc_ != other.alloc_)
				{
					this->clear();
					alloc_ = std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.alloc_);
					for (auto& value : other)
					{
						this->push_back(std::move(value));
					}
				}
				else
				{
					auto it = std::ranges::next(other.begin(),
						static_cast<difference_type>(this->size()), other.end());
					auto movable = std::ranges::subrange{ other.begin(), it };
					auto new_ones = std::ranges::subrange{ it, other.end() };

					std::ranges::move(movable, this->begin());
					for (auto& value : new_ones)
					{
						this->push_back(std::move(value));
					}
				}
			}
			else
			{
				ptrs_ = std::exchange(other.ptrs_, links_{});
				size_ = std::exchange(other.size_, 0);
			}

			return *this;
		}

		constexpr void assign(size_type count, const T& value)
		{
			std::ranges::fill_n(this->begin(), std::min(this->size(), count), value);

			while (this->size() < count)
			{
				this->push_back(value);
			}

			while (this->size() > count)
			{
				this->pop_back();
			}
		}

		template <std::input_iterator InputIt, std::sentinel_for<InputIt> S>
			requires (std::is_constructible_v<T, std::iter_reference_t<InputIt>> && std::is_assignable_v<T&, std::iter_reference_t<InputIt>>)
		constexpr void assign(InputIt first, S last)
		{
			if constexpr (std::sized_sentinel_for<S, InputIt>)
			{
				this->assign_range(std::ranges::subrange(std::move(first), std::move(last)));
			}
			else
			{
				auto it = this->begin();
				size_type counter = 0;
				while (true)
				{
					if (it == this->end())
					{
						while (first != last)
						{
							this->push_back(*first);
							++first;
						}
						return;
					}
					if (first == last)
					{
						while (this->size() > counter)
						{
							this->pop_back();
						}
						return;
					}

					*it = *first;
					++it;
					++first;
					++counter;
				}
			}
		}
		
		template <typename U> requires std::assignable_from<T&, const U&> && std::constructible_from<T, const U&>
		constexpr void assign(std::initializer_list<U> ilist)
		{
			this->assign_range(ilist);
		}

		template <detail::container_compatible_range<T> R>
		constexpr void assign_range(R&& rg)
		{
			if constexpr (std::ranges::sized_range<R>)
			{
				auto it = std::ranges::copy_n(std::ranges::begin(rg), std::min(this->size(), std::ranges::size(rg)), this->begin()).in;
				auto end = std::ranges::end(rg);

				while (it != end)
				{
					this->push_back(*it);
					++it;
				}

				while (this->size() > std::ranges::size(rg))
				{
					this->pop_back();
				}
			}
			else
			{
				this->assign(std::ranges::begin(rg), std::ranges::end(rg));
			}
		}

		template <detail::container_compatible_range<T> R>
		constexpr void append_range(R&& rg)
		{
			auto end = std::ranges::end(rg);
			for (auto it = std::ranges::begin(rg); it != end; ++it)
			{
				this->emplace_back(*it);
			}
		}

		template <detail::container_compatible_range<T> R>
		constexpr void prepend_range(R&& rg)
		{
			this->insert_range(this->begin(), std::forward<R>(rg));
		}

		template <detail::container_compatible_range<T> R>
		constexpr iterator insert_range(const_iterator pos, R&& rg)
		{
			auto begin = std::ranges::begin(rg);
			auto end = std::ranges::end(rg);

			if (begin == end)
			{
				return iterator{ const_cast<links_*>(pos.ptrs_) };
			}
			
			auto first = this->emplace(pos, *begin);
			++begin;

			while (begin != end)
			{
				this->emplace(pos, *begin);
				++begin;
			}

			return first;
		}

		constexpr iterator insert(const_iterator pos, const T& value)
		{
			return this->emplace(pos, value);
		}

		template <typename ... Args> requires std::constructible_from<T, Args...>
		constexpr iterator emplace(const_iterator pos, Args&& ... args)
		{
			node_* new_node = traits::allocate(alloc_, 1);
			links_* prev = const_cast<links_*>(pos.ptrs_)->prev_;
			traits::construct(alloc_, new_node, std::in_place, std::forward<Args>(args)...);
			prev->next_ = static_cast<links_*>(new_node);
			new_node->prev_ = prev;
			new_node->next_ = const_cast<links_*>(pos.ptrs_);
			const_cast<links_*>(pos.ptrs_)->prev_ = static_cast<links_*>(new_node);
			++size_;
			return iterator{ static_cast<links_*>(new_node) };
		}

		constexpr iterator insert(const_iterator pos, T&& value)
		{
			return this->emplace(pos, std::move(value));
		}

		constexpr iterator insert(const_iterator pos, size_type count, const T& value)
		{
			if (count == 0)
			{
				return pos;
			}

			auto it = this->insert(pos, value);
			--count;

			while (count)
			{
				this->insert(pos, value);
				--count;
			}

			return it;
		}

		template <std::input_iterator I, std::sentinel_for<I> S>
		constexpr iterator insert(const_iterator pos, I first, S last)
		{
			if (first == last)
			{
				return iterator{ const_cast<links_*>(pos.ptrs_) };
			}

			auto it = this->insert(pos, *first);
			++first;

			while (first != last)
			{
				this->insert(pos, *first);
				++first;
			}

			return it;
		}

		template <typename U> requires std::constructible_from<T, const U&>
		constexpr iterator insert(std::initializer_list<U> ilist)
		{
			return this->insert_range(ilist);
		}

		template <typename U> requires std::constructible_from<T, const U&>
		constexpr iterator insert(const_iterator pos, std::initializer_list<U> ilist)
		{
			return this->insert_range(pos, ilist);
		}

		constexpr void push_back(const T& value)
		{
			this->insert(this->end(), value);
		}

		constexpr void push_back(T&& value)
		{
			this->insert(this->end(), std::move(value));
		}

		template<typename ... Args>
		constexpr reference emplace_back(Args&& ... args)
		{
			return *this->emplace(this->end(), std::forward<Args>(args)...);
		}

		constexpr void pop_back()
		{
			node_* removed = static_cast<node_*>(ptrs_.prev_);
			removed->prev_->next_ = &ptrs_;
			ptrs_.prev_ = removed->prev_;

			std::destroy_at(std::addressof(static_cast<node_*>(removed)->storage_.value_));
			traits::destroy(alloc_, static_cast<node_*>(removed));
			traits::deallocate(alloc_, static_cast<node_*>(removed), 1);
			--size_;
		}

		constexpr void push_front(const T& value)
		{
			this->insert(this->begin(), value);
		}

		constexpr void push_front(T&& value)
		{
			this->insert(this->begin(), std::move(value));
		}

		template<typename ... Args>
		constexpr reference emplace_front(Args&& ... args)
		{
			this->emplace(this->begin(), std::forward<Args>(args)...);
		}

		constexpr void pop_front()
		{
			node_* removed = static_cast<node_*>(ptrs_.next_);
			ptrs_.next_ = removed->next_;
			removed->next_->prev_ = &ptrs_;

			std::destroy_at(std::addressof(static_cast<node_*>(removed)->storage_.value_));
			traits::destroy(alloc_, static_cast<node_*>(removed));
			traits::deallocate(alloc_, static_cast<node_*>(removed), 1);
			--size_;
		}

		constexpr iterator erase(const_iterator pos)
		{
			if (pos == this->end())
			{
				return this->end();
			}

			node_* removed = static_cast<node_*>(const_cast<links_*>(pos.ptrs_));
			links_* prev = removed->prev_;
			prev->next_ = removed->next_;
			removed->next_->prev_ = prev;

			std::destroy_at(std::addressof(static_cast<node_*>(removed)->storage_.value_));
			traits::destroy(alloc_, static_cast<node_*>(removed));
			traits::deallocate(alloc_, static_cast<node_*>(removed), 1);
			--size_;

			return iterator{ prev->next_ };
		}

		constexpr iterator erase(const_iterator first, const_iterator last)
		{
			if (first != last)
			{
				do
				{
					first = this->erase(first);
				} while (first != last);
			}

			return iterator(const_cast<links_*>(last.ptrs_));
		}

		template <typename U> requires std::equality_comparable_with<const T&, const U&>
		constexpr size_type remove(const U& value)
		{
			auto it = this->cbegin();
			const size_type old_size = this->size();

			while (it != this->cend())
			{
				if (*it == value)
				{
					it = this->erase(it);
				}
				else
				{
					++it;
				}
			}

			return old_size - this->size();
		}

		template <typename UnaryPredicate>
		constexpr size_type remove_if(UnaryPredicate p)
		{
			auto it = this->cbegin();
			const size_type old_size = this->size();

			while (it != this->cend())
			{
				if (static_cast<bool>(std::invoke(p, *it)))
				{
					it = this->erase(it);
				}
				else
				{
					++it;
				}
			}

			return old_size - this->size();
		}

		constexpr void reverse() noexcept
		{
			links_* node = &ptrs_;

			std::ranges::swap(node->next_, node->prev_);
			node = node->prev_;

			while (node != &ptrs_)
			{
				std::ranges::swap(node->next_, node->prev_);
				node = node->prev_;
			}
		}
	private:

		struct node_;
		constexpr iterator insert_node_(const_iterator pos, node_* new_node) noexcept
		{
			links_* prev = const_cast<links_*>(pos.ptrs_)->prev_;
			prev->next_ = static_cast<links_*>(new_node);
			new_node->prev_ = prev;
			new_node->next_ = const_cast<links_*>(pos.ptrs_);
			const_cast<links_*>(pos.ptrs_)->prev_ = static_cast<links_*>(new_node);
			++size_;
			return iterator{ static_cast<links_*>(new_node) };
		}

	public:
		constexpr void splice(const_iterator pos, list&& other) noexcept
		{
			for (iterator it = other.begin(); it != other.end(); ++it)
			{
				node_* as_node = static_cast<node_*>(it.ptrs_);
				this->insert_node_(pos, as_node);
			}
			other.ptrs_ = { &other.ptrs_, &other.ptrs_ };
			other.size_ = 0;
		}

		constexpr void splice(const_iterator pos, list& other) noexcept
		{
			this->splice(pos, std::move(other));
		}

		constexpr void splice(const_iterator pos, list&& other, const_iterator it) noexcept
		{
			links_* as_node = const_cast<links_*>(it.ptrs_);
			links_* prev = as_node->prev_;
			prev->next_ = as_node->next_;
			as_node->next_->prev_ = prev;
			--other.size_;

			this->insert_node_(pos, static_cast<node_*>(as_node));
		}

		constexpr void splice(const_iterator pos, list& other, const_iterator it) noexcept
		{
			this->splice(pos, std::move(other), it);
		}

		constexpr void splice(const_iterator pos, list&& other,
			const_iterator first, const_iterator last) noexcept
		{
			for (const_iterator it = first; it != last;)
			{
				auto tmp = std::ranges::next(it);
				this->splice(pos, other, it);
				it = tmp;
			}
		}

		constexpr void splice(const_iterator pos, list& other,
			const_iterator first, const_iterator last) noexcept
		{
			this->splice(pos, std::move(other), first, last);
		}

		template <typename Compare>
		constexpr void merge(list&& other, Compare comp) noexcept
		{
			if (this == &other)
			{
				return;
			}

			auto pos = this->begin();
			auto it = other.begin();

			if (it == other.end())
			{
				return;
			}

			if (pos == this->end())
			{
				this->splice(pos, other, it);
				return;
			}

			for (;;)
			{
				const T& value = *it;
				if (std::invoke(comp, value, *pos))
				{
					auto tmp = std::ranges::next(it);
					this->splice(pos, other, it);
					it = tmp;

					if (it == other.end())
					{
						return;
					}
				}
				else
				{
					++pos;

					if (pos == this->end())
					{
						this->splice(pos, other, it, other.end());
						return;
					}
				}
			}
		}

		template <std::indirect_binary_predicate Compare>
		constexpr void merge(list& other, Compare comp) noexcept
		{
			this->merge(std::move(other), std::ref(comp));
		}

		constexpr void merge(list& other) noexcept
		{
			this->merge(std::move(other), std::less{});
		}

		constexpr void merge(list&& other) noexcept
		{
			this->merge(std::move(other), std::less{});
		}

		template <typename Compare>
		constexpr void sort(Compare comp)
		{
			if (this->empty())
			{
				return;
			}

			using temp_alloc_t = typename traits::template rebind_alloc<links_*>;
			temp_alloc_t temp_alloc{ alloc_ };
			
			auto storage = std::allocator_traits<temp_alloc_t>::allocate(temp_alloc, this->size());
			links_** ptr = &storage[0];

			for (iterator it = begin(); it != end(); ++it)
			{
				*ptr = it.ptrs_;
				++ptr;
			}

			auto as_range = std::ranges::subrange(&storage[0], &storage[size()]);

			std::ranges::sort(as_range, std::ref(comp),
				[](links_* node) -> const T&
				{
					node_* as_node = static_cast<node_*>(node);
					return as_node->storage_.value_;
				});

			for (auto pair : as_range | std::views::slide(2))
			{
				links_* lhs = pair[0];
				links_* rhs = pair[1];
				lhs->next_ = rhs;
				rhs->prev_ = lhs;
			}

			ptrs_.next_ = as_range.front();
			ptrs_.prev_ = as_range.back();

			as_range.front()->prev_ = &ptrs_;
			as_range.back()->next_ = &ptrs_;

			std::allocator_traits<temp_alloc_t>::deallocate(temp_alloc, storage, this->size());
		}

		constexpr void sort()
		{
			this->sort(std::less{});
		}

		constexpr void resize(size_type count) requires (std::is_default_constructible_v<T>)
		{
			if (count < this->size())
			{
				while (this->size() != count)
				{
					this->pop_back();
				}
			}
			else
			{
				while (this->size() != count)
				{
					this->emplace_back();
				}
			}
		}

		constexpr void resize(size_type count, const value_type& value)
		{
			if (count < this->size())
			{
				while (count)
				{
					this->pop_back();
					--count;
				}
			}
			else
			{
				while (this->size() != count)
				{
					this->push_back(value);
				}
			}
		}

		constexpr allocator_type get_allocator() const noexcept
		{
			return static_cast<allocator_type>(alloc_);
		}

		[[nodiscard]]
		constexpr size_type size() const noexcept
		{
			return size_;
		}

		[[nodiscard]]
		consteval size_type max_size() const noexcept
		{
			return static_cast<size_type>(-1);
		}

		[[nodiscard]]
		constexpr bool empty() const noexcept
		{
			return size() == 0;
		}

		constexpr iterator begin() noexcept
		{
			return iterator{ ptrs_.next_ };
		}

		constexpr iterator end() noexcept
		{
			return iterator{ &ptrs_ };
		}

		constexpr const_iterator begin() const noexcept
		{
			return const_iterator{ ptrs_.next_ };
		}

		constexpr const_iterator end() const noexcept
		{
			return const_iterator{ &ptrs_ };
		}

		constexpr const_iterator cbegin() const noexcept
		{
			return this->begin();
		}

		constexpr const_iterator cend() const noexcept
		{
			return this->cend();
		}

		constexpr reverse_iterator rbegin() noexcept
		{
			return std::make_reverse_iterator(this->begin());
		}

		constexpr reverse_iterator rend() noexcept
		{
			return std::make_reverse_iterator(this->end());
		}

		constexpr const_reverse_iterator rbegin() const noexcept
		{
			return std::make_reverse_iterator(this->cbegin());
		}

		constexpr const_reverse_iterator rend() const noexcept
		{
			return std::make_reverse_iterator(this->cend());
		}

		constexpr const_reverse_iterator crbegin() const noexcept
		{
			return this->rbegin();
		}

		constexpr const_reverse_iterator crend() const noexcept
		{
			return this->rend();
		}

		constexpr reference front() noexcept
		{
			return *begin();
		}

		constexpr const_reference front() const noexcept
		{
			return *cbegin();
		}

		constexpr reference back() noexcept
		{
			return *std::ranges::prev(end());
		}

		constexpr const_reference back() const noexcept
		{
			return *std::ranges::prev(cend());
		}

		constexpr void clear()
		{
			links_* current = ptrs_.next_;
			while (size_)
			{
				links_* tmp = current->next_;

				std::destroy_at(std::addressof(static_cast<node_*>(current)->storage_.value_));
				traits::destroy(alloc_, static_cast<node_*>(current));
				traits::deallocate(alloc_, static_cast<node_*>(current), 1);

				current = tmp;
				--size_;
			}
			ptrs_.next_ = &ptrs_;
			ptrs_.prev_ = &ptrs_;
		}

		constexpr size_type unique()
		{
			return this->unique(std::equal_to{});
		}

		template <typename BinaryPredicate>
		constexpr size_type unique(BinaryPredicate p)
		{
			if (this->empty())
			{
				return 0;
			}

			size_type counter = 0;

			for (iterator it = begin(); it != end(); ++it)
			{
				const T& value = *it;
				size_type to_be_erased = 0;
				
				auto it2 = std::ranges::next(it);
				while (it2 != end() && p(value, *it2))
				{
					++it2;
					to_be_erased++;
				}

				this->erase(std::ranges::next(it), it2);
				counter += to_be_erased;
			}

			return counter;
		}

		constexpr void swap(list& other) noexcept
		{
			if constexpr (std::allocator_traits<node_allocator>::propagate_on_container_swap::value)
			{
				std::ranges::swap(alloc_, other.alloc_);
			}

			if (this->empty())
			{
				if (!other.empty())
				{
					this->ptrs_.next_ = other.ptrs_.next_;
					this->ptrs_.next_->prev_ = &ptrs_;
					this->ptrs_.prev_ = other.ptrs_.prev_;
					this->ptrs_.prev_->next_ = &ptrs_;
					other.ptrs_ = { &other.ptrs_, &other.ptrs_ };
				}
			}
			else if (other.empty())
			{
				other.ptrs_.next_ = this->ptrs_.next_;
				other.ptrs_.next_->prev_ = &other.ptrs_;
				other.ptrs_.prev_ = this->ptrs_.prev_;
				other.ptrs_.prev_->next_ = &other.ptrs_;
				this->ptrs_ = { &ptrs_, &ptrs_ };
			}
			else
			{
				std::ranges::swap(this->ptrs_, other.ptrs_);
				std::ranges::swap(this->ptrs_.prev_->next_, other.ptrs_.prev_->next_);
				std::ranges::swap(this->ptrs_.next_->prev_, other.ptrs_.next_->prev_);
			}

			std::ranges::swap(size_, other.size_);
		}

		friend constexpr bool operator==(const list& lhs, const list& rhs)
			noexcept(noexcept(std::declval<const T&>() == std::declval<const T&>()))
		{
			return std::ranges::equal(lhs, rhs);
		}

		friend constexpr detail::synth_three_way_result<T>
			operator<=>(const list& lhs, const list& rhs)
		{
			return std::lexicographical_compare_three_way(
				lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
				detail::synth_three_way
			);
		}

		constexpr ~list()
		{
			links_* current = ptrs_.next_;
			while (size_)
			{
				links_* tmp = current->next_;

				std::destroy_at(std::addressof(static_cast<node_*>(current)->storage_.value_));
				traits::destroy(alloc_, static_cast<node_*>(current));
				traits::deallocate(alloc_, static_cast<node_*>(current), 1);

				current = tmp;
				--size_;
			}
		}

	private:
		struct node_;

		using node_allocator = typename
			std::allocator_traits<allocator_type>::template rebind_alloc<node_>;
		using traits = typename std::allocator_traits<node_allocator>;

		struct links_
		{
			links_* next_ = nullptr;
			links_* prev_ = nullptr;
		};

		struct node_ : links_
		{
			constexpr node_() = default;

			template <typename ... Args>
			constexpr node_(std::in_place_t, Args&& ... args)
			{
				std::construct_at(std::addressof(storage_.value_),
					std::forward<Args>(args)...);
			}

			union storage_t
			{
				constexpr storage_t() noexcept {};

				constexpr ~storage_t()
					requires std::is_trivially_destructible_v<value_type>
				= default;

				constexpr ~storage_t() noexcept {}

				value_type value_;
			} storage_;
		};

		template <bool Const>
		struct iterator_base
		{
			friend struct iterator_base<!Const>;
			friend class list;

			using difference_type = typename list::difference_type;
			using value_type = T;
			using pointer = std::conditional_t<Const, typename list::const_pointer,
				typename list::pointer>;
			using reference = std::conditional_t<Const, typename list::const_reference,
				typename list::reference>;
			using iterator_category = std::bidirectional_iterator_tag;
			using iterator_concept = std::bidirectional_iterator_tag;

			constexpr iterator_base() noexcept = default;

			constexpr iterator_base(const iterator_base& other) noexcept = default;
			constexpr iterator_base& operator=(const iterator_base& other) noexcept = default;


			constexpr iterator_base(const iterator_base<false>& other) noexcept
				requires (Const == true)
			: ptrs_{ other.ptrs_ }
			{}

			constexpr iterator_base& operator=(const iterator_base<false>& other) noexcept
				requires (Const == true)
			{
				ptrs_ = other.ptrs_;
				return *this;
			}

			constexpr reference operator*() const noexcept
			{
				if constexpr (Const == true)
				{
					return static_cast<const node_* const>(ptrs_)->storage_.value_;
				}
				else
				{
					return static_cast<node_* const>(ptrs_)->storage_.value_;
				}
			}

			constexpr iterator_base& operator++() noexcept
			{
				ptrs_ = ptrs_->next_;
				return *this;
			}

			constexpr iterator_base operator++(int) noexcept
			{
				auto tmp = *this;
				++(*this);
				return tmp;
			}

			constexpr iterator_base& operator--() noexcept
			{
				ptrs_ = ptrs_->prev_;
				return *this;
			}

			constexpr iterator_base operator--(int) noexcept
			{
				auto tmp = *this;
				--(*this);
				return tmp;
			}

			constexpr pointer operator->()
			{
				return std::addressof(**this);
			}

			friend constexpr bool operator==(const iterator_base& lhs, const iterator_base& rhs) noexcept
			{
				return lhs.ptrs_ == rhs.ptrs_;
			}

		private:

			constexpr iterator_base(links_* node) noexcept
				requires (Const == false)
			: ptrs_{ node }
			{}

			constexpr iterator_base(const links_* node) noexcept
				requires (Const == true)
			: ptrs_{ node }
			{}

			std::conditional_t<Const,
				const links_, links_>* ptrs_ = nullptr;
		};

		links_ ptrs_{ &ptrs_, &ptrs_ };
		std::size_t size_{};

		[[no_unique_address]] node_allocator alloc_;
	};

	template <typename T, typename Alloc>
	constexpr void swap(list<T, Alloc>& lhs, list<T, Alloc>& rhs)
		noexcept(noexcept(lhs.swap(rhs)))
	{
		lhs.swap(rhs);
	}

	template <typename InputIt,
		typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
	list(InputIt, InputIt, Alloc = Alloc())
		->list<typename std::iterator_traits<InputIt>::value_type, Alloc>;

	template <std::ranges::input_range R,
		typename Alloc = std::allocator<std::ranges::range_value_t<R>>>
	list(std::from_range_t, R&&, Alloc = Alloc())
		-> list<std::ranges::range_value_t<R>, Alloc>;

	static_assert(std::ranges::bidirectional_range<list<int>>);
	static_assert(std::ranges::output_range<list<int>, int>);
	static_assert(std::bidirectional_iterator<std::ranges::iterator_t<list<int>>>);
	static_assert(std::ranges::sized_range<list<int>>);
	static_assert(std::is_copy_constructible_v<list<int>>);
	static_assert(std::is_copy_assignable_v<list<int>>);
	static_assert(std::is_default_constructible_v<list<int>>);
	static_assert(std::is_move_constructible_v<list<int>>);
	static_assert(std::is_move_assignable_v<list<int>>);
	static_assert(std::is_destructible_v<list<int>>);

	namespace pmr
	{
		template <typename T>
		using list = list<T, std::pmr::polymorphic_allocator<T>>;
	}
}

#endif // CONSTEXPR_LIST
