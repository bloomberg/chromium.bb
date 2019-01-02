// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXCEPTION_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXCEPTION_H_

namespace location {
namespace nearby {

// TODO(kyleqian): Remove this file pending Nearby library import. This is a
// temporary placeholder for exception.h from the Nearby library. See bug
// #861813 -> https://crbug.com/861813.
struct Exception {
  enum Value {
    NONE,
    IO,
    INTERRUPTED,
    INVALID_PROTOCOL_BUFFER,
    EXECUTION,
  };
};

// ExceptionOr models the concept of the return value of a function that might
// throw an exception.
//
// If ok() returns true, result() is a usable return value. Otherwise,
// exception() explains why such a value is not present.
//
// A typical pattern of usage is as follows:
//
//      if (!e.ok()) {
//        if (Exception::EXCEPTION_TYPE_1 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_1.
//        } else if (Exception::EXCEPTION_TYPE_2 == e.exception()) {
//          // Handle Exception::EXCEPTION_TYPE_2.
//        }
//
//         return;
//      }
//
//      // Use e.result().
template <typename T>
class ExceptionOr {
 public:
  explicit ExceptionOr(T result);
  explicit ExceptionOr(Exception::Value exception);

  bool ok() const;

  T result() const;
  Exception::Value exception() const;

 private:
  T result_;
  Exception::Value exception_;
};

}  // namespace nearby
}  // namespace location

#include "chromeos/components/nearby/library/exception.cc"

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_EXCEPTION_H_
