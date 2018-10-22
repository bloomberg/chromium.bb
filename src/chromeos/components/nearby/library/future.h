// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_FUTURE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_FUTURE_H_

#include "chromeos/components/nearby/library/exception.h"

namespace location {
namespace nearby {

// A Future represents the result of an asynchronous computation.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Future.html
template <typename T>
class Future {
 public:
  virtual ~Future() {}

  virtual ExceptionOr<T>
  get() = 0;  // throws Exception::INTERRUPTED, Exception::EXECUTION
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_FUTURE_H_
