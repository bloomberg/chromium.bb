// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_invalidation_state_tracker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/task_runner.h"
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

void FakeInvalidationStateTracker::SetMaxVersionAndPayload(
    const invalidation::ObjectId& id,
    int64 max_version,
    const std::string& payload) {
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

void FakeInvalidationStateTracker::SetInvalidatorClientId(
    const std::string& client_id) {
  Clear();
  invalidator_client_id_ = client_id;
}

std::string FakeInvalidationStateTracker::GetInvalidatorClientId() const {
  return invalidator_client_id_;
}

void FakeInvalidationStateTracker::SetBootstrapData(
    const std::string& data) {
  bootstrap_data_ = data;
}

std::string FakeInvalidationStateTracker::GetBootstrapData() const {
  return bootstrap_data_;
}

void FakeInvalidationStateTracker::Clear() {
  invalidator_client_id_ = "";
  state_map_ = InvalidationStateMap();
  bootstrap_data_ = "";
}

void FakeInvalidationStateTracker::GenerateAckHandles(
    const ObjectIdSet& ids,
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::Callback<void(const AckHandleMap&)> callback) {
  AckHandleMap ack_handles;
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    state_map_[*it].expected = AckHandle::CreateUnique();
    ack_handles.insert(std::make_pair(*it, state_map_[*it].expected));
  }
  if (!task_runner->PostTask(FROM_HERE, base::Bind(callback, ack_handles)))
    ADD_FAILURE();
}

void FakeInvalidationStateTracker::Acknowledge(const invalidation::ObjectId& id,
                                               const AckHandle& ack_handle) {
  InvalidationStateMap::iterator it = state_map_.find(id);
  if (it == state_map_.end())
    ADD_FAILURE();
  it->second.current = ack_handle;
}

}  // namespace syncer
