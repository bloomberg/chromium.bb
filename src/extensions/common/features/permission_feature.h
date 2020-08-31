// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_PERMISSION_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_PERMISSION_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

class PermissionFeature : public SimpleFeature {
 public:
  PermissionFeature();
  ~PermissionFeature() override;

  // TODO(crbug.com/1078984): This should also override IsAvailableToManifest so
  // that a permission or manifest feature can declare dependency on other
  // permission features.

  Feature::Availability IsAvailableToContext(
      const Extension* extension,
      Feature::Context context,
      const GURL& url,
      Feature::Platform platform) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_PERMISSION_FEATURE_H_
