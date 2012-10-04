// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_MODEL_TYPE_STATE_MAP_TEST_UTIL_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_MODEL_TYPE_STATE_MAP_TEST_UTIL_H_

// Convince googletest to use the correct overload for PrintTo().
#include "sync/internal_api/public/base/invalidation_test_util.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

::testing::Matcher<const ModelTypeInvalidationMap&> Eq(
    const ModelTypeInvalidationMap& expected);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_MODEL_TYPE_STATE_MAP_TEST_UTIL_H_
