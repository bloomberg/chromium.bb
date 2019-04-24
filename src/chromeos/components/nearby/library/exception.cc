// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/library/exception.h"

namespace location {
namespace nearby {

// TODO(kyleqian): Remove this file pending Nearby library import. This is a
// temporary placeholder for exception.cc from the Nearby library. See bug
// #861813 -> https://crbug.com/861813.
template <typename T>
ExceptionOr<T>::ExceptionOr(T result)
    : result_(result), exception_(Exception::NONE) {}

template <typename T>
ExceptionOr<T>::ExceptionOr(Exception::Value exception)
    : result_(), exception_(exception) {}

template <typename T>
bool ExceptionOr<T>::ok() const {
  return Exception::NONE == exception_;
}

template <typename T>
T ExceptionOr<T>::result() const {
  return result_;
}

template <typename T>
Exception::Value ExceptionOr<T>::exception() const {
  return exception_;
}

}  // namespace nearby
}  // namespace location
