// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_COUNT_DOWN_LATCH_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_COUNT_DOWN_LATCH_H_

#include <cstdint>

#include "chromeos/components/nearby/library/exception.h"

namespace location {
namespace nearby {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
class CountDownLatch {
 public:
  virtual ~CountDownLatch() {}

  virtual Exception::Value await() = 0;  // throws Exception::INTERRUPTED
  virtual ExceptionOr<bool> await(
      std::int32_t timeout_millis) = 0;  // throws Exception::INTERRUPTED
  virtual void countDown() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_COUNT_DOWN_LATCH_H_
