// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CALLABLE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CALLABLE_H_

#include "chromeos/components/nearby/library/exception.h"

namespace location {
namespace nearby {

// The Callable interface should be implemented by any class whose instances are
// intended to be executed by a thread, and need to return a result. The class
// must define a method named call() with no arguments and a specific return
// type.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Callable.html
template <typename T>
class Callable {
 public:
  virtual ~Callable() {}

  virtual ExceptionOr<T> call() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CALLABLE_H_
