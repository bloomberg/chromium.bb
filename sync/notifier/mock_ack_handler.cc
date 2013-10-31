// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/mock_ack_handler.h"

#include "sync/internal_api/public/base/ack_handle.h"
#include "sync/internal_api/public/base/invalidation.h"

namespace syncer {

namespace {

struct AckHandleMatcher {
  AckHandleMatcher(const AckHandle& handle);
  bool operator()(const syncer::Invalidation& invalidation) const;

  syncer::AckHandle handle_;
};

AckHandleMatcher::AckHandleMatcher(const AckHandle& handle)
  : handle_(handle) {}

bool AckHandleMatcher::operator()(
    const syncer::Invalidation& invalidation) const {
  return handle_.Equals(invalidation.ack_handle());
}

}  // namespace

MockAckHandler::MockAckHandler() {}

MockAckHandler::~MockAckHandler() {}

void MockAckHandler::RegisterInvalidation(Invalidation* invalidation) {
  unacked_invalidations_.push_back(*invalidation);
  invalidation->set_ack_handler(WeakHandleThis());
}

void MockAckHandler::RegisterUnsentInvalidation(Invalidation* invalidation) {
  unsent_invalidations_.push_back(*invalidation);
}

bool MockAckHandler::IsUnacked(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  InvalidationVector::const_iterator it = std::find_if(
      unacked_invalidations_.begin(),
      unacked_invalidations_.end(),
      matcher);
  return it != unacked_invalidations_.end();
}

bool MockAckHandler::IsUnsent(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  InvalidationVector::const_iterator it1 = std::find_if(
      unsent_invalidations_.begin(),
      unsent_invalidations_.end(),
      matcher);
  return it1 != unsent_invalidations_.end();
}

void MockAckHandler::Acknowledge(
    const invalidation::ObjectId& id,
    const AckHandle& handle) {
  AckHandleMatcher matcher(handle);
  InvalidationVector::iterator it = std::find_if(
      unacked_invalidations_.begin(),
      unacked_invalidations_.end(),
      matcher);
  if (it != unacked_invalidations_.end()) {
    acked_invalidations_.push_back(*it);
    unacked_invalidations_.erase(it);
  }
}

void MockAckHandler::Drop(
    const invalidation::ObjectId& id,
    const AckHandle& handle) {
}

WeakHandle<AckHandler> MockAckHandler::WeakHandleThis() {
  return WeakHandle<AckHandler>(AsWeakPtr());
}

}  // namespace syncer
