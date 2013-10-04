// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_TEST_UTIL_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_TEST_UTIL_H_

#include <iosfwd>

#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class AckHandle;
class Invalidation;

void PrintTo(const AckHandle& ack_handle, ::std::ostream* os);
::testing::Matcher<const AckHandle&> Eq(const AckHandle& expected);

void PrintTo(const Invalidation& invalidation, ::std::ostream* os);

::testing::Matcher<const Invalidation&> Eq(const Invalidation& expected);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_INVALIDATION_TEST_UTIL_H_
