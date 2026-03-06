// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <memory_resource>
#include <foundation/primitive_types.h>

namespace hydra :: containers
{
    template<typename T>
    class Array : std::pmr::vector<T>
    {
        using Base = std::pmr::vector<T>;

    public:
        explicit
        Array( std::pmr::memory_resource* mr = std::pmr::get_default_resource() )
        : Base(mr)
        {
        }

        using Base::operator[];
        using Base::begin;
        using Base::end;

        constexpr friend void
        PushBack( Array& A, T& Value )
        {
            A.push_back(Value);
        }

        constexpr friend void
        PushBack( Array& A, const T& Value )
        {
            A.push_back(Value);
        }

        constexpr friend void
        Reserve( Array& A, u64 Count )
        {
            A.reserve(Count);
        }

        constexpr friend void
        Clear( Array& A )
        {
            A.clear();
        }

        constexpr friend bool
        IsEmpty( Array& A )
        {
            return A.empty();
        }

        constexpr friend u64
        NumElements( Array& A )
        {
            return A.size();
        }

        constexpr friend void
        SwapRemove( Array& A, u64 Index )
        {
            A[Index] = std::move(A.back());
            A.pop_back();
        }
    };
}
