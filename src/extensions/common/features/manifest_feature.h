// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

class ManifestFeature : public SimpleFeature {
 public:
  ManifestFeature();
  ~ManifestFeature() override;

  // TODO(crbug.com/1078984): This should also override IsAvailableToManifest so
  // that a permission or manifest feature can declare dependency on other
  // manifest features.

  Feature::Availability IsAvailableToContext(
      const Extension* extension,
      Feature::Context context,
      const GURL& url,
      Feature::Platform platform) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_
