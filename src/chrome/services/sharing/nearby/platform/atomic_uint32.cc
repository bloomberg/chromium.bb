// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/atomic_uint32.h"

namespace location {
namespace nearby {
namespace chrome {

AtomicUint32::AtomicUint32(std::uint32_t initial_value)
    : value_(initial_value) {}

AtomicUint32::~AtomicUint32() = default;

std::uint32_t AtomicUint32::Get() const {
  return value_;
}

void AtomicUint32::Set(std::uint32_t value) {
  value_ = value;
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
