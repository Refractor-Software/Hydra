// Copyright (C) William Pimentel-Tonche and contributors. All rights reserved.

#pragma once

#include <absl/container/flat_hash_map.h>

namespace hydra::containers
{
    template <typename K, typename V>
    class hash_map : absl::flat_hash_map<K, V>
    {
        using base = absl::flat_hash_map<K, V>;
        using iterator = base::iterator;
        using const_iterator = base::const_iterator;
        using size_type = base::size_type;

        friend base &
        _base_of(hash_map &M)
        {
            return M;
        }

        friend const base &
        _base_of(const hash_map &M)
        {
            return M;
        }

        using base::size;
        using base::empty;
        using base::find;
        using base::reserve;
        using base::clear;
        using base::insert;
        using base::emplace;
        using base::erase;

    public:
        using base::begin;
        using base::end;
        using base::cbegin;
        using base::cend;
        using base::operator[];

        uint64 Num() const
        {
            return size();
        }

        bool IsEmpty() const
        {
            return empty();
        }

        V *TryGet(const K &key)
        {
            auto it = find(key);
            return it != end() ? &it->second : nullptr;
        }

        const V *TryGet(const K &key) const
        {
            auto it = find(key);
            return it != end() ? &it->second : nullptr;
        }

        void Reserve(size_type count)
        {
            reserve(count);
        }

        void Clear()
        {
            clear();
        }

        iterator Find(const K &key)
        {
            return find(key);
        }

        iterator Insert(const K &key, V &&value)
        {
            return insert(key, value);
        }

        iterator Append(const K &key, V &&value)
        {
            return emplace(key, value).first;
        }

        void Remove(const K &key)
        {
            erase(key);
        }
    };
}