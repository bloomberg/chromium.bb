// Copyright 2022 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TINT_UTILS_UNIQUE_ALLOCATOR_H_
#define SRC_TINT_UTILS_UNIQUE_ALLOCATOR_H_

#include <functional>
#include <unordered_set>
#include <utility>

#include "src/tint/utils/block_allocator.h"

namespace tint::utils {

/// UniqueAllocator is used to allocate unique instances of the template type
/// `T`.
template <typename T, typename HASH = std::hash<T>, typename EQUAL = std::equal_to<T>>
class UniqueAllocator {
  public:
    /// @param args the arguments used to construct the object.
    /// @return a pointer to an instance of `T` with the provided arguments.
    ///         If an existing instance of `T` has been constructed, then the same
    ///         pointer is returned.
    template <typename TYPE = T, typename... ARGS>
    TYPE* Get(ARGS&&... args) {
        // Create a temporary T instance on the stack so that we can hash it, and
        // use it for equality lookup for the std::unordered_set. If the item is not
        // found in the set, then we create the persisted instance with the
        // allocator.
        TYPE key{args...};
        auto hash = HASH{}(key);
        auto it = items.find(Entry{hash, &key});
        if (it != items.end()) {
            return static_cast<TYPE*>(it->ptr);
        }
        auto* ptr = allocator.template Create<TYPE>(std::forward<ARGS>(args)...);
        items.emplace_hint(it, Entry{hash, ptr});
        return ptr;
    }

  protected:
    /// Entry is used as the entry to the unordered_set
    struct Entry {
        /// The pre-calculated hash of the entry
        size_t hash;
        /// Tge pointer to the unique object
        T* ptr;
    };
    /// Comparator is the hashing and equality function used by the unordered_set
    struct Comparator {
        /// Hashing function
        /// @param e the entry
        /// @returns the hash of the entry
        size_t operator()(Entry e) const { return e.hash; }

        /// Equality function
        /// @param a the first entry to compare
        /// @param b the second entry to compare
        /// @returns true if the two entries are equal
        bool operator()(Entry a, Entry b) const { return EQUAL{}(*a.ptr, *b.ptr); }
    };

    /// The block allocator used to allocate the unique objects
    BlockAllocator<T> allocator;
    /// The unordered_set of unique item entries
    std::unordered_set<Entry, Comparator, Comparator> items;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_UNIQUE_ALLOCATOR_H_
