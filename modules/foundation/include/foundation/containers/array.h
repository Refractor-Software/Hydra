// Copyright (C) William Pimentel-Tonche. All rights reserved.

#pragma once

#include <memory_resource>

namespace hydra::containers
{
    template <typename element_type>
    class array : std::pmr::vector<element_type>
    {
        using base = std::pmr::vector<element_type>;
        using iterator = base::iterator;
        using const_iterator = base::const_iterator;
        using size_type = base::size_type;

    public:
        explicit array(std::pmr::memory_resource *Mr = std::pmr::get_default_resource()) : base(Mr)
        {
        }

        array(std::initializer_list<element_type> Init,
              std::pmr::memory_resource *Mr = std::pmr::get_default_resource())
            : base(Init, Mr)
        {
        }

        using base::operator[];
        using base::begin;
        using base::cbegin;
        using base::cend;
        using base::end;

        [[nodiscard]] bool
        is_empty() const
        {
            return base::empty();
        }

        size_type
        num_elements() const
        {
            return base::size();
        }

        size_type
        capacity() const
        {
            return base::capacity();
        }

        void
        reserve(size_type count)
        {
            base::reserve(count);
        }

        void
        shrink_to_fit()
        {
            base::shrink_to_fit();
        }

        void
        append(const element_type &value)
        {
            base::push_back(value);
        }

        void
        append(element_type &&value)
        {
            base::push_back(std::move(value));
        }

        template <typename... args>
        element_type &
        emplace(args &&...constructor_args)
        {
            return base::emplace_back(std::forward<args>(constructor_args)...);
        }

        iterator
        insert_at(size_type index, const element_type &value)
        {
            return base::insert(base::begin() + index, value);
        }

        void
        remove_unordered(size_type index)
        {
            (*this)[index] = std::move(base::back());
            base::pop_back();
        }

        void
        remove_ordered(size_type index)
        {
            base::erase(base::begin() + index);
        }

        element_type
        pop_last()
        {
            element_type result = std::move(base::back());
            base::pop_back();
            return result;
        }

        void
        reset()
        {
            base::clear();
        }

        void
        resize_to(size_type new_size)
        {
            base::resize(new_size);
        }
    };
} // namespace hydra::containers
