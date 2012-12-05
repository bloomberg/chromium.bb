// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_state_tracker.h"

namespace syncer {

InvalidationState::InvalidationState()
    : version(kint64min),
      expected(AckHandle::InvalidAckHandle()),
      current(AckHandle::InvalidAckHandle()) {
}

InvalidationState::~InvalidationState() {
}

bool operator==(const InvalidationState& lhs, const InvalidationState& rhs) {
  return lhs.version == rhs.version &&
      lhs.expected.Equals(rhs.expected) &&
      lhs.current.Equals(rhs.current);
}

}  // namespace syncer
