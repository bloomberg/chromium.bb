// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_CACHEINVALIDATION_DEPS_STRING_UTIL_H_
#define GOOGLE_CACHEINVALIDATION_DEPS_STRING_UTIL_H_

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"

namespace invalidation {

using base::StringAppendV;

inline std::string SimpleItoa(int v) {
  return base::IntToString(v);
}

inline std::string SimpleItoa(int64 v) {
  return base::Int64ToString(v);
}

}  // namespace invalidation

#endif  // GOOGLE_CACHEINVALIDATION_DEPS_STRING_UTIL_H_
