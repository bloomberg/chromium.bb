// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_invalidation_handler.h"

namespace syncer {

FakeInvalidationHandler::FakeInvalidationHandler()
    : state_(DEFAULT_INVALIDATION_ERROR),
      last_source_(LOCAL_INVALIDATION),
      invalidation_count_(0) {}

FakeInvalidationHandler::~FakeInvalidationHandler() {}

InvalidatorState FakeInvalidationHandler::GetInvalidatorState() const {
  return state_;
}

const ObjectIdInvalidationMap&
FakeInvalidationHandler::GetLastInvalidationMap() const {
  return last_invalidation_map_;
}

IncomingInvalidationSource
FakeInvalidationHandler::GetLastInvalidationSource() const {
  return last_source_;
}

int FakeInvalidationHandler::GetInvalidationCount() const {
  return invalidation_count_;
}

void FakeInvalidationHandler::OnInvalidatorStateChange(InvalidatorState state) {
  state_ = state;
}

void FakeInvalidationHandler::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map,
    IncomingInvalidationSource source) {
  last_invalidation_map_ = invalidation_map;
  last_source_ = source;
  ++invalidation_count_;
}

}  // namespace syncer
