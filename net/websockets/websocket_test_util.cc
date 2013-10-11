// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_test_util.h"

#include "base/basictypes.h"

namespace net {

namespace {
const uint64 kA =
    (static_cast<uint64>(0x5851f42d) << 32) + static_cast<uint64>(0x4c957f2d);
const uint64 kC = 12345;
const uint64 kM = static_cast<uint64>(1) << 48;

}  // namespace

LinearCongruentialGenerator::LinearCongruentialGenerator(uint32 seed)
    : current_(seed) {}

uint32 LinearCongruentialGenerator::Generate() {
  uint64 result = current_;
  current_ = (current_ * kA + kC) % kM;
  return static_cast<uint32>(result >> 16);
}

}  // namespace net
