// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STD_UTIL_H_
#define UTIL_STD_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"

namespace openscreen {

template <typename T, size_t N>
constexpr size_t countof(T (&array)[N]) {
  return N;
}

// std::basic_string::data() has no mutable overload prior to C++17 [1].
// Hence this overload is provided.
// Note: str[0] is safe even for empty strings, as they are guaranteed to be
// null-terminated [2].
//
// [1] http://en.cppreference.com/w/cpp/string/basic_string/data
// [2] http://en.cppreference.com/w/cpp/string/basic_string/operator_at
template <typename CharT, typename Traits, typename Allocator>
CharT* data(std::basic_string<CharT, Traits, Allocator>& str) {
  return std::addressof(str[0]);
}

template <typename Key, typename Value>
void RemoveValueFromMap(std::map<Key, Value*>* map, Value* value) {
  for (auto it = map->begin(); it != map->end();) {
    if (it->second == value) {
      it = map->erase(it);
    } else {
      ++it;
    }
  }
}

template <typename ForwardIteratingContainer>
bool AreElementsSortedAndUnique(const ForwardIteratingContainer& c) {
  return absl::c_is_sorted(c) && (absl::c_adjacent_find(c) == c.end());
}

template <typename RandomAccessContainer>
void SortAndDedupeElements(RandomAccessContainer* c) {
  std::sort(c->begin(), c->end());
  const auto new_end = std::unique(c->begin(), c->end());
  c->erase(new_end, c->end());
}

}  // namespace openscreen

#endif  // UTIL_STD_UTIL_H_
