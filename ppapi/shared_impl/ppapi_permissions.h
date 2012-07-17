// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_
#define PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_

#include "base/basictypes.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

enum Permission {
  // Allows access to dev interfaces.
  PERMISSION_DEV = 1 << 0,

  // Allows access to Browser-internal interfaces.
  PERMISSION_PRIVATE = 1 << 2,

  // Allows ability to bypass user-gesture checks for showing things like
  // file select dialogs.
  PERMISSION_BYPASS_USER_GESTURE = 1 << 3
};

class PPAPI_SHARED_EXPORT PpapiPermissions {
 public:
  // Initializes the permissions struct with no permissions.
  PpapiPermissions();

  // Initializes with the given permissions bits set.
  explicit PpapiPermissions(uint32 perms);

  ~PpapiPermissions();

  bool HasPermission(Permission perm) const;

 private:
  uint32 permissions_;

  // Note: Copy & assign supported.
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_
