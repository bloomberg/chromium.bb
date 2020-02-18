// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "serializable.h"

namespace crdtp {
// =============================================================================
// Serializable - An object to be emitted as a sequence of bytes.
// =============================================================================

std::vector<uint8_t> Serializable::TakeSerialized() && {
  std::vector<uint8_t> out;
  AppendSerialized(&out);
  return out;
}
}  // namespace crdtp
