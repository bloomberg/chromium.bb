// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_MOCK_INVALIDATION_STATE_TRACKER_H_
#define SYNC_NOTIFIER_MOCK_INVALIDATION_STATE_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockInvalidationStateTracker
    : public InvalidationStateTracker,
      public base::SupportsWeakPtr<MockInvalidationStateTracker> {
 public:
  MockInvalidationStateTracker();
  virtual ~MockInvalidationStateTracker();

  MOCK_CONST_METHOD0(GetAllMaxVersions, InvalidationVersionMap());
  MOCK_METHOD2(SetMaxVersion, void(const invalidation::ObjectId&, int64));
  MOCK_CONST_METHOD0(GetInvalidationState, std::string());
  MOCK_METHOD1(SetInvalidationState, void(const std::string&));
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_MOCK_INVALIDATION_STATE_TRACKER_H_
