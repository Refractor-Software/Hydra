// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <absl/container/flat_hash_set.h>

namespace hydra::containers
{
    template<typename element_type>
    class hash_set : absl::flat_hash_set<element_type>
    {
        using base           = absl::flat_hash_set<element_type>;
        using iterator       = base::iterator;
        using const_iterator = base::const_iterator;
        using size_type      = base::size_type;

        friend base&
        _base_of( hash_set& S )
        {
            return S;
        }

        friend const base&
        _base_of( const hash_set& S )
        {
            return S;
        }

    public:
        using base::begin;
        using base::end;
        using base::cbegin;
        using base::cend;
    };

    template<typename T>
    bool
    IsEmpty( const hash_set<T>& S )
    {
        return _base_of(S).empty();
    }

    template<typename T>
    hash_set<T>::size_type
    Length( const hash_set<T>& S )
    {
        return _base_of(S).size();
    }

    template<typename T>
    hash_set<T>::size_type
    MaxLength( const hash_set<T>& S )
    {
        return _base_of(S).max_size();
    }

    template<typename T>
    bool
    Contains( const hash_set<T>& S, const T& Value )
    {
        return _base_of(S).contains(Value);
    }

    // Returns a pointer to the canonical stored element, or nullptr.
    // Useful for interned strings — you want the stored copy, not your lookup key.
    template<typename T>
    const T*
    TryGet( const hash_set<T>& S, const T& Value )
    {
        auto It = _base_of(S).find(Value);
        return It != _base_of(S).end()
                   ? &*It
                   : nullptr;
    }

    template<typename T, typename... Args>
    auto
    Find( hash_set<T>& S, Args&&... A ) -> decltype( _base_of(S).find(std::forward<Args>(A)...) )
    {
        return _base_of(S).find(std::forward<Args>(A)...);
    }

    template<typename T, typename... Args>
    auto
    Find( const hash_set<T>& S, Args&&... A ) -> decltype( _base_of(S).find(std::forward<Args>(A)...) )
    {
        return _base_of(S).find(std::forward<Args>(A)...);
    }

    template<typename T, typename... Args>
    auto
    Insert( hash_set<T>& S, Args&&... A ) -> decltype( _base_of(S).insert(std::forward<Args>(A)...) )
    {
        return _base_of(S).insert(std::forward<Args>(A)...);
    }

    template<typename T, typename... Args>
    auto
    Emplace( hash_set<T>& S, Args&&... A ) -> decltype( _base_of(S).emplace(std::forward<Args>(A)...) )
    {
        return _base_of(S).emplace(std::forward<Args>(A)...);
    }

    template<typename T, typename... Args>
    auto
    Erase( hash_set<T>& S, Args&&... A ) -> decltype( _base_of(S).erase(std::forward<Args>(A)...) )
    {
        return _base_of(S).erase(std::forward<Args>(A)...);
    }

    template<typename T>
    void
    Clear( hash_set<T>& S )
    {
        _base_of(S).clear();
    }

    template<typename T>
    void
    Reserve( hash_set<T>& S, typename hash_set<T>::size_type Count )
    {
        _base_of(S).reserve(Count);
    }
}
