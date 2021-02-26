// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/atomic_boolean.h"

namespace location {
namespace nearby {
namespace chrome {

AtomicBoolean::AtomicBoolean(bool initial_value) : value_(initial_value) {}

AtomicBoolean::~AtomicBoolean() = default;

bool AtomicBoolean::Get() const {
  return value_.load();
}

bool AtomicBoolean::Set(bool value) {
  return value_.exchange(value);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
