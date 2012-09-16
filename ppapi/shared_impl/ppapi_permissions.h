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
  PERMISSION_BYPASS_USER_GESTURE = 1 << 3,

  // NOTE: If you add stuff be sure to update AllPermissions().
};

class PPAPI_SHARED_EXPORT PpapiPermissions {
 public:
  // Initializes the permissions struct with no permissions.
  PpapiPermissions();

  // Initializes with the given permissions bits set.
  explicit PpapiPermissions(uint32 perms);

  ~PpapiPermissions();

  // Returns a permissions class with all features enabled. This is for testing
  // and manually registered plugins.
  static PpapiPermissions AllPermissions();

  bool HasPermission(Permission perm) const;

  // TODO(brettw) bug 147507: Remove this when we fix the permissions bug
  // (this was added for logging).
  uint32 GetBits() const { return permissions_; }

 private:
  uint32 permissions_;

  // Note: Copy & assign supported.
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_
