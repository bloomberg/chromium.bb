// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TIME_H_
#define PLATFORM_API_TIME_H_

#include <cstdint>

namespace openscreen {
namespace platform {

typedef uint64_t Microseconds;

Microseconds GetMonotonicTimeNow();

}  // namespace platform
}  // namespace openscreen

#endif
