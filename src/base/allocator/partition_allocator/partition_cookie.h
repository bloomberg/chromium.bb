// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

static constexpr size_t kCookieSize = 16;

// Cookie is enabled for debug builds.
#if DCHECK_IS_ON()

static constexpr unsigned char kCookieValue[kCookieSize] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xD0, 0x0D,
    0x13, 0x37, 0xF0, 0x05, 0xBA, 0x11, 0xAB, 0x1E};

constexpr size_t kPartitionCookieSizeAdjustment = kCookieSize;

ALWAYS_INLINE void PartitionCookieCheckValue(unsigned char* cookie_ptr) {
  for (size_t i = 0; i < kCookieSize; ++i, ++cookie_ptr)
    PA_DCHECK(*cookie_ptr == kCookieValue[i]);
}

ALWAYS_INLINE void PartitionCookieWriteValue(unsigned char* cookie_ptr) {
  for (size_t i = 0; i < kCookieSize; ++i, ++cookie_ptr)
    *cookie_ptr = kCookieValue[i];
}

#else

constexpr size_t kPartitionCookieSizeAdjustment = 0;

ALWAYS_INLINE void PartitionCookieCheckValue(unsigned char* address) {}

ALWAYS_INLINE void PartitionCookieWriteValue(unsigned char* cookie_ptr) {}

#endif  // DCHECK_IS_ON()

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_COOKIE_H_
