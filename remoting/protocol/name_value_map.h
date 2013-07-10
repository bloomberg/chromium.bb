// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions that allow to map enum values to strings.

#include <stddef.h>

#include "base/logging.h"

namespace remoting {
namespace protocol {

template <typename T>
struct NameMapElement {
  const T value;
  const char* const name;
};

template <typename T, size_t N>
const char* ValueToName(const NameMapElement<T> (&map)[N], T value) {
  for (size_t i = 0; i < N; ++i) {
    if (map[i].value == value)
      return map[i].name;
  }
  NOTREACHED();
  return NULL;
}

template <typename T, size_t N>
bool NameToValue(const NameMapElement<T> (&map)[N],
                 const std::string& name,
                 T* result) {
  for (size_t i = 0; i < N; ++i) {
    if (map[i].name == name) {
      *result = map[i].value;
      return true;
    }
  }
  return false;
}

}  // namespace protocol
}  // namespace remoting
