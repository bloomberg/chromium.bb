// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine_impl.h"
#include "base/files/file_util.h"

namespace quarantine {

QuarantineImpl::QuarantineImpl(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

QuarantineImpl::~QuarantineImpl() = default;

}  // namespace quarantine
