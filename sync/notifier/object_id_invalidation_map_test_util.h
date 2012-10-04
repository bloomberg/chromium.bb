// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_OBJECT_ID_INVALIDATION_MAP_TEST_UTILH_
#define SYNC_NOTIFIER_OBJECT_ID_INVALIDATION_MAP_TEST_UTILH_

// Convince googletest to use the correct overload for PrintTo().
#include "sync/internal_api/public/base/invalidation_test_util.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

::testing::Matcher<const ObjectIdInvalidationMap&> Eq(
    const ObjectIdInvalidationMap& expected);

}  // namespace syncer

#endif  // SYNC_NOTIFIER_OBJECT_ID_INVALIDATION_MAP_TEST_UTILH_
