// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STD_UTIL_H_
#define BASE_STD_UTIL_H_

#include <map>

namespace openscreen {

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

}  // namespace openscreen

#endif  // BASE_STD_UTIL_H_