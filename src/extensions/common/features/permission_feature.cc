// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/permission_feature.h"

#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

PermissionFeature::PermissionFeature() {
}

PermissionFeature::~PermissionFeature() {
}

Feature::Availability PermissionFeature::IsAvailableToContextImpl(
    const Extension* extension,
    Feature::Context context,
    const GURL& url,
    Feature::Platform platform,
    int context_id,
    bool check_developer_mode) const {
  Availability availability = SimpleFeature::IsAvailableToContextImpl(
      extension, context, url, platform, context_id, check_developer_mode);
  if (!availability.is_available())
    return availability;

  if (extension && !extension->permissions_data()->HasAPIPermission(name()))
    return CreateAvailability(NOT_PRESENT, extension->GetType());

  return CreateAvailability(IS_AVAILABLE);
}

}  // namespace extensions
