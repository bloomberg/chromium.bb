// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/mock_invalidation.h"

#include "base/logging.h"
#include "sync/test/mock_invalidation_tracker.h"

namespace syncer {

scoped_ptr<MockInvalidation> MockInvalidation::BuildUnknownVersion() {
  return scoped_ptr<MockInvalidation>(
      new MockInvalidation(true, -1, std::string()));
}

scoped_ptr<MockInvalidation> MockInvalidation::Build(
    int64 version,
    const std::string& payload) {
  return scoped_ptr<MockInvalidation>(
      new MockInvalidation(false, version, payload));
}

MockInvalidation::~MockInvalidation() {
}

bool MockInvalidation::IsUnknownVersion() const {
  return is_unknown_version_;
}

const std::string& MockInvalidation::GetPayload() const {
  DCHECK(!is_unknown_version_);
  return payload_;
}

int64 MockInvalidation::GetVersion() const {
  DCHECK(!is_unknown_version_);
  return version_;
}

void MockInvalidation::Acknowledge() {
  // Do nothing.
}

void MockInvalidation::Drop() {
  // Do nothing.
}

MockInvalidation::MockInvalidation(bool is_unknown_version,
                                   int64 version,
                                   const std::string& payload)
    : is_unknown_version_(is_unknown_version),
      version_(version),
      payload_(payload) {
}

}  // namespace syncer
