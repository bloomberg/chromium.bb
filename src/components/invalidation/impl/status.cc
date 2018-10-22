// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/status.h"

namespace syncer {

Status::Status(StatusCode status_code, const std::string& message)
    : code(status_code), message(message) {}

Status Status::Success() {
  return Status(StatusCode::SUCCESS, std::string());
}

Status::~Status() = default;

}  // namespace syncer
