// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_permissions.h"

#include "base/logging.h"

namespace ppapi {

PpapiPermissions::PpapiPermissions() : permissions_(0) {
}

PpapiPermissions::PpapiPermissions(uint32 perms) : permissions_(perms) {
}

PpapiPermissions::~PpapiPermissions() {
}

// static
PpapiPermissions PpapiPermissions::AllPermissions() {
  return PpapiPermissions(
      PERMISSION_DEV |
      PERMISSION_PRIVATE |
      PERMISSION_BYPASS_USER_GESTURE);
}

bool PpapiPermissions::HasPermission(Permission perm) const {
  // Check that "perm" is a power of two to make sure the caller didn't set
  // more than one permission bit. We may want to change how permissions are
  // represented in the future so don't want callers making assumptions about
  // bits.
  uint32 perm_int = static_cast<uint32>(perm);
  DCHECK((perm_int & (perm_int - 1)) == 0);
  return !!(permissions_ & perm_int);
}

}  // namespace ppapi
