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
  InvalidationVersionMap::const_iterator it = versions_.find(id);
  return (it == versions_.end()) ? kMinVersion : it->second;
}

InvalidationVersionMap
FakeInvalidationStateTracker::GetAllMaxVersions() const {
  return versions_;
}

void FakeInvalidationStateTracker::SetMaxVersion(
    const invalidation::ObjectId& id, int64 max_version) {
  InvalidationVersionMap::const_iterator it = versions_.find(id);
  if ((it != versions_.end()) && (max_version <= it->second)) {
    ADD_FAILURE();
    return;
  }
  versions_[id] = max_version;
}

void FakeInvalidationStateTracker::Forget(const ObjectIdSet& ids) {
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    versions_.erase(*it);
  }
}

void FakeInvalidationStateTracker::SetInvalidationState(
    const std::string& state) {
  state_ = state;
}

std::string FakeInvalidationStateTracker::GetInvalidationState() const {
  return state_;
}

}  // namespace syncer
