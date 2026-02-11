// this header implements a sparse set (stable vector), the implementation is almoast 1 for one taken from:
//      https://github.com/johnBuffer/StableIndexVector
//
// this version removes oop design from the original and adds support for using this data structure with
// custom allocators, as opposed to only default allocators in the original
//
// there is also another smaller tweak, allowing for custom id types, without hardcoding int64_t

#ifndef SPARSE_SET_H
#define SPARSE_SET_H

#include "common.h"

#include <concepts>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace ktl_ns {

template <typename T, std::integral ID = size_t, typename Alloc = std::allocator<T>>
struct sparse_set;

// handle is a simple storage aware container for ids, it can be used like a pointer
// to the object stored in the set, and is aware of the validity of the object it points to
//
// handles are type erased and thus have the overhead of 3 accessor function pointers
// but this allows for storing handles independednt of the allocator used by the set
template <typename T, std::integral ID = size_t>
struct handle {
    ID id = 0;
    ID validity_id = 0;
    void* set_ref = nullptr;

    bool (*valid_fn)(const void*, ID, ID) = nullptr;
    T& (*get_fn)(void*, ID) = nullptr;
    const T& (*get_const_fn)(const void*, ID) = nullptr;

    handle() = default;

    template <typename Alloc>
    handle(ID id, ID validity_id, sparse_set<T, ID, Alloc>* set)
        : id{id},
          validity_id{validity_id},
          set_ref{set},
          valid_fn([](const void* set, ID id, ID v) -> bool {
              return static_cast<const sparse_set<T, ID, Alloc>*>(set)->valid(id, v);
          }),
          get_fn([](void* set, ID id) -> T& {
              return (*static_cast<sparse_set<T, ID, Alloc>*>(set))[id];
          }),
          get_const_fn([](const void* set, ID id) -> const T& {
              return (*static_cast<const sparse_set<T, ID, Alloc>*>(set))[id];
          }) {}

    [[nodiscard]] bool valid() const { return set_ref && valid_fn(set_ref, id, validity_id); }
    operator bool() const { return valid(); }

    T* operator->() { return &get_fn(set_ref, id); }
    const T* operator->() const { return &get_const_fn(set_ref, id); }
    T& operator*() { return get_fn(set_ref, id); }
    const T& operator*() const { return get_const_fn(set_ref, id); }
};

// handle is a simple storage aware container for ids, it can be used like a pointer
// to the object stored in the set, and is aware of the validity of the object it points to
//
// basic_handles are a fully specialized handle
// they offer better performance, harder integration due to the custom allocator support
template <typename T, std::integral ID = size_t, typename Alloc = std::allocator<T>>
struct basic_handle {
    using set_type = sparse_set<T, ID, Alloc>;

    ID id = 0;
    ID validity_id = 0;
    set_type* set_ref = nullptr;

    basic_handle() = default;
    basic_handle(ID i, ID v, set_type* s) : id(i), validity_id(v), set_ref(s) {}

    bool valid() const { return set_ref && set_ref->valid(id, validity_id); }
    explicit operator bool() const { return valid(); }

    T* operator->() { return &(*set_ref)[id]; }
    const T* operator->() const { return &(*set_ref)[id]; }
    T& operator*() { return (*set_ref)[id]; }
    const T& operator*() const { return (*set_ref)[id]; }

    operator handle<T, ID>() const { return {id, validity_id, set_ref}; }
};

// sparese_set implements a cache friendly, stable id vector, reallocations might still happen
// but iterating, inserting and removing elements are all O(1) operations
//
// this implementation unlike the original, allows for a custom ID type and
// a custom allocator, any allocator that can be used with std::vector is supported
template <typename T, std::integral ID, typename Alloc>
struct sparse_set {
    // INTERNAL: holds per element metadata
    struct _metadata {
        ID rid = 0;
        ID validity_id = 0;
    };

    using ID_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ID>;
    using meta_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<_metadata>;
    static constexpr ID invalid_id = std::numeric_limits<ID>::max();

    std::vector<T, Alloc> _data;
    std::vector<_metadata, meta_alloc> metadata;
    std::vector<ID, ID_alloc> indexes;

    sparse_set(const Alloc& alloc = Alloc{})
        : _data(alloc), metadata(meta_alloc(alloc)), indexes(ID_alloc(alloc)) {}

    // appends an object to the end of the set by copying it
    ID push_back(const T& obj) {
        const ID id = _get_free_slot();
        _data.push_back(obj);
        return id;
    }

    // appends an object to the end of the set by constructing it in place
    template <typename... TArgs>
    ID emplace_back(TArgs&&... args) {
        const ID id = _get_free_slot();
        _data.emplace_back(std::forward<TArgs>(args)...);
        return id;
    }

    // removes an object from the set
    void erase(ID id) {
        const ID data_id = indexes[id];
        const ID last_data_id = _data.size() - 1;
        const ID last_id = metadata[last_data_id].rid;

        ++metadata[data_id].validity_id;

        std::swap(_data[data_id], _data[last_data_id]);
        std::swap(metadata[data_id], metadata[last_data_id]);
        std::swap(indexes[id], indexes[last_id]);

        _data.pop_back();
    }

    // removes an object from the set
    void erase_via_data(size_t idx) { erase(metadata[idx].rid); }

    // removes an object from the set
    void erase(const handle<T, ID>& h) {
        ktl_assert(h.set_ref == this);
        ktl_assert(h.valid());
        erase(h.id);
    }

    void erase(const basic_handle<T, ID, Alloc>& h) {
        ktl_assert(h.set_ref == this);
        ktl_assert(h.valid());
        erase(h.id);
    }

    // returns the index in the data vector corresponding to the given id
    [[nodiscard]] size_t get_data_idx(ID id) const { return indexes[id]; }

    // array access by object id
    T& operator[](ID id) { return _data[indexes[id]]; }
    // const array access by object id
    T const& operator[](ID id) const { return _data[indexes[id]]; }

    // returns the current size of the set
    [[nodiscard]] size_t size() const { return _data.size(); }
    // returns whether the set is empty
    [[nodiscard]] bool empty() const { return _data.empty(); }
    // returns the allocated capacity of the set
    [[nodiscard]] size_t capacity() const { return _data.capacity(); }

    // creates a handle for the given id
    basic_handle<T, ID, Alloc> create_handle(ID id) {
        assert(get_data_idx(id) < size());
        return {id, metadata[indexes[id]].validity_id, this};
    }

    // creates a handle using an objects position in the data vector
    basic_handle<T, ID, Alloc> create_handle_from_data(size_t idx) {
        assert(idx < size());
        return {metadata[idx].rid, metadata[idx].validity_id, this};
    }

    // returns whether the given id and correspond is valid in the set, using it's last known validity id
    [[nodiscard]] bool valid(ID id, ID validity_id) const {
        assert(id < indexes.size());
        return validity_id == metadata[indexes[id]].validity_id;
    }

    typename std::vector<T>::iterator begin() noexcept { return _data.begin(); }
    typename std::vector<T>::iterator end() noexcept { return _data.end(); }
    typename std::vector<T>::const_iterator begin() const noexcept { return _data.begin(); }
    typename std::vector<T>::const_iterator end() const noexcept { return _data.end(); }

    // remove all objects that satisfy the given predicate
    template <typename Func>
    requires std::predicate<Func, const T&> void remove_if(Func&& predicate) {
        for (size_t i = 0; i < _data.size();) {
            if (predicate(_data[i])) {
                erase_via_data(i);
            } else {
                ++i;
            }
        }
    }

    // pre allocates, 'size' of object slots in the set
    void reserve(size_t size) {
        _data.reserve(size);
        metadata.reserve(size);
        indexes.reserve(size);
    }

    // returns the validity id for the given object id
    [[nodiscard]] ID get_validity_id(ID id) const { return metadata[indexes[id]].validity_id; }

    // returns a pointer to the internal data array
    T* data() { return _data.data(); }
    // returns a reference to the internal data array
    std::vector<T>& get_data() { return _data; }
    // returns a const reference to the internal data array
    const std::vector<T>& get_data() const { return _data; }

    // returns the id that will be used by the next object added
    [[nodiscard]] ID get_next_id() const {
        if (metadata.size() > _data.size()) { return metadata[_data.size()].rid; }
        return _data.size();
    }

    // removes all objects from the set, and invalidates all existing ids
    void clear() {
        _data.clear();

        for (auto& m : metadata) {
            ++m.validity_id;
        }
    }

    // returns whether the given id is currently valid in the set
    [[nodiscard]] bool valid_id(ID id) const { return id < indexes.size(); }

    // INTERNAL: creates a new object slot in the set
    ID _get_free_slot() {
        const ID id = _get_free_id();
        indexes[id] = _data.size();
        return id;
    }

    // INTERNAL: gets a free id for a new object
    ID _get_free_id() {
        if (metadata.size() > _data.size()) {
            ++metadata[_data.size()].validity_id;
            return metadata[_data.size()].rid;
        }
        const ID new_id = _data.size();
        metadata.push_back({new_id, 0});
        indexes.push_back(new_id);
        return new_id;
    }
};

}  // namespace ktl_ns

#endif /* SPARSE_SET_H */
