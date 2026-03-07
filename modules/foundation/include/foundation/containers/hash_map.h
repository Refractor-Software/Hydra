// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <absl/container/flat_hash_map.h>

namespace hydra::containers
{
    template<typename K, typename V>
    class hash_map : absl::flat_hash_map<K, V>
    {
        using base           = absl::flat_hash_map<K, V>;
        using iterator       = base::iterator;
        using const_iterator = base::const_iterator;
        using size_type      = base::size_type;

        friend base&
        _base_of( hash_map& M )
        {
            return M;
        }

        friend const base&
        _base_of( const hash_map& M )
        {
            return M;
        }

    public:
        using base::begin;
        using base::end;
        using base::cbegin;
        using base::cend;
        using base::operator[];
        using base::clear;
    };

    template<typename K, typename V>
    bool
    IsEmpty( const hash_map<K, V>& M )
    {
        return _base_of(M).empty();
    }

    template<typename K, typename V>
    auto
    Length( const hash_map<K, V>& M )
    {
        return _base_of(M).size();
    }

    template<typename K, typename V>
    V*
    TryGet( hash_map<K, V>& M, const K& Key )
    {
        auto It = _base_of(M).find(Key);
        return It != _base_of(M).end()
                   ? &It->second
                   : nullptr;
    }

    template<typename K, typename V>
    const V*
    TryGet( const hash_map<K, V>& M, const K& Key )
    {
        auto It = _base_of(M).find(Key);
        return It != _base_of(M).end()
                   ? &It->second
                   : nullptr;
    }

    template<typename K, typename V>
    void
    Reserve( hash_map<K, V>& M, typename hash_map<K, V>::size_type Count )
    {
        _base_of(M).reserve(Count);
    }

    template<typename K, typename V>
    void
    Clear( hash_map<K, V>& M )
    {
        _base_of(M).clear();
    }

    template<typename K, typename V, typename... Args>
    auto
    Find( hash_map<K, V>& M, Args&&... A ) -> decltype( _base_of(M).find(std::forward<Args>(A)...) )
    {
        return _base_of(M).find(std::forward<Args>(A)...);
    }

    template<typename K, typename V, typename... Args>
    auto
    Insert( hash_map<K, V>& M, Args&&... A ) -> decltype( _base_of(M).insert(std::forward<Args>(A)...) )
    {
        return _base_of(M).insert(std::forward<Args>(A)...);
    }

    template<typename K, typename V, typename... Args>
    auto
    Append( hash_map<K, V>& M, Args&&... A ) -> decltype( _base_of(M).emplace(std::forward<Args>(A)...) )
    {
        return _base_of(M).emplace(std::forward<Args>(A)...);
    }

    template<typename K, typename V, typename... Args>
    auto
    Erase( hash_map<K, V>& M, Args&&... A ) -> decltype( _base_of(M).erase(std::forward<Args>(A)...) )
    {
        return _base_of(M).erase(std::forward<Args>(A)...);
    }
}
