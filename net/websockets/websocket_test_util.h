// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
#define NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_

#include "base/basictypes.h"

namespace net {

class LinearCongruentialGenerator {
 public:
  explicit LinearCongruentialGenerator(uint32 seed);
  uint32 Generate();

 private:
  uint64 current_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
