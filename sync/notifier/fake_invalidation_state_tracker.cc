// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_invalidation_state_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

const int64 FakeInvalidationStateTracker::kMinVersion = kint64min;

FakeInvalidationStateTracker::FakeInvalidationStateTracker() {}

FakeInvalidationStateTracker::~FakeInvalidationStateTracker() {}

int64 FakeInvalidationStateTracker::GetMaxVersion(
    const invalidation::ObjectId& id) const {
  InvalidationStateMap::const_iterator it = state_map_.find(id);
  return (it == state_map_.end()) ? kMinVersion : it->second.version;
}

InvalidationStateMap
FakeInvalidationStateTracker::GetAllInvalidationStates() const {
  return state_map_;
}

void FakeInvalidationStateTracker::SetMaxVersion(
    const invalidation::ObjectId& id, int64 max_version) {
  InvalidationStateMap::const_iterator it = state_map_.find(id);
  if ((it != state_map_.end()) && (max_version <= it->second.version)) {
    ADD_FAILURE();
    return;
  }
  state_map_[id].version = max_version;
}

void FakeInvalidationStateTracker::Forget(const ObjectIdSet& ids) {
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    state_map_.erase(*it);
  }
}

void FakeInvalidationStateTracker::SetBootstrapData(
    const std::string& data) {
  bootstrap_data_ = data;
}

std::string FakeInvalidationStateTracker::GetBootstrapData() const {
  return bootstrap_data_;
}

}  // namespace syncer
