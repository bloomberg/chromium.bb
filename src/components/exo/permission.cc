// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/permission.h"

#include "base/time/time.h"

namespace exo {

Permission::Permission(Permission::Capability capability,
                       base::TimeDelta timeout)
    : capability_(capability), expiry_(base::Time::Now() + timeout) {}

void Permission::Revoke() {
  // Revoke the permission by setting its expiry to be in the past.
  expiry_ = {};
}

bool Permission::Check(Permission::Capability capability) const {
  return capability_ == capability && base::Time::Now() < expiry_;
}

}  // namespace exo
