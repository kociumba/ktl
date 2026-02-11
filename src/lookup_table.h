// this header implements a compile time optimized lookup table
// 
// lookup_table aims to replicate the speed and utility of array[CONSTANT] from c
// but for any key type c++ can support
//
// the ability to use the lookup table at runtime is preserved
// if used as such it becomes an std::unordered_map

#ifndef KTL_LOOKUP_TABLE_H
#define KTL_LOOKUP_TABLE_H

#include "common.h"

#include <algorithm>
#include <array>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace ktl_ns {

// lookup_table is a compile time optimized lookup table that can be used as a constexpr map
template <typename Key,
    typename Val,
    size_t N,
    class Alloc = std::allocator<std::pair<const Key, Val>>>
struct lookup_table {
    std::array<std::pair<const Key, Val>, N> comptime_data;
    std::unordered_map<Key, Val, std::hash<Key>, std::equal_to<Key>, Alloc> runtime_map;

    // constructs a lookup_table from a comp time array
    constexpr lookup_table(std::pair<Key, Val> (&&arr)[N], const Alloc& alloc = Alloc())
        : comptime_data(std::to_array(std::move(arr))), runtime_map(alloc) {}

    // finds a value in the compile time data
    constexpr Val* find_comptime(const Key& K) {
        for (auto& [key, val] : comptime_data) {
            if (key == K) return &val;
        }
        return nullptr;
    }

    // finds a value in the compile time data
    constexpr const Val* find_comptime(const Key& K) const {
        for (const auto& [key, val] : comptime_data) {
            if (key == K) return &val;
        }
        return nullptr;
    }

    // looks up a value in the lookup table, first checking the compile time data, then the runtime map
    const Val* lookup(const Key& K) const {
        if (auto* v = find_comptime(K)) return v;

        auto it = runtime_map.find(K);
        return it != runtime_map.end() ? &it->second : nullptr;
    }

    // looks up a value in the lookup table, first checking the compile time data, then the runtime map
    Val* lookup(const Key& K) {
        if (auto* v = find_comptime(K)) return v;

        auto it = runtime_map.find(K);
        return it != runtime_map.end() ? &it->second : nullptr;
    }

    constexpr Val& operator[](const Key& K) {
        if consteval {
            Val* v = find_comptime(K);
            if (v) return *v;
            std::unreachable();
        } else {
            if (auto* v = find_comptime(K)) return *v;
            return runtime_map[K];
        }
    }

    constexpr const Val& operator[](const Key& K) const {
        if consteval {
            const Val* v = find_comptime(K);
            if (v) return *v;
            std::unreachable();
        } else {
            if (auto* v = lookup(K)) return *v;
            std::unreachable();
        }
    }

    // inserts a value into the lookup_table, only supports runtime keys and values
    void insert(Key key, Val val) { runtime_map.emplace(std::move(key), std::move(val)); }
    
    // removes a value from the lookup_table, only supports runtime keys
    void remove(const Key& key) { runtime_map.erase(key); }

    // looks up a value in the lookup_table, returning nullptr if not found
    Val* get(const Key& K) const {
        if (auto* v = lookup(K)) return v;
        return nullptr;
    }

    // checks if a key is contained in the lookup_table
    bool contains(const Key& K) const { return lookup(K) != nullptr; }
};

template <typename Key, typename Value, size_t N>
lookup_table(std::pair<Key, Value> (&&)[N]) -> lookup_table<Key, Value, N>;

template <typename Key, typename Value, size_t N, typename Alloc>
lookup_table(std::pair<Key, Value> (&&)[N], Alloc) -> lookup_table<Key, Value, N, Alloc>;

}  // namespace ktl_ns

#endif /* KTL_LOOKUP_TABLE_H */
