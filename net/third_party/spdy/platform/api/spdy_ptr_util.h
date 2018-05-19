// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_PTR_UTIL_H_
#define NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_PTR_UTIL_H_

#include <memory>
#include <utility>

#include "net/third_party/spdy/platform/impl/spdy_ptr_util_impl.h"

namespace spdy {

template <typename T, typename... Args>
std::unique_ptr<T> SpdyMakeUnique(Args&&... args) {
  return SpdyMakeUniqueImpl<T>(std::forward<Args>(args)...);
}

template <typename T>
std::unique_ptr<T> SpdyWrapUnique(T* ptr) {
  return SpdyWrapUniqueImpl<T>(ptr);
}

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_PTR_UTIL_H_
