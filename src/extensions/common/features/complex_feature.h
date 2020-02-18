// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"

namespace extensions {

// A ComplexFeature is composed of one or many Features. A ComplexFeature
// is available if any Feature (i.e. permission rule) that composes it is
// available, but not if only some combination of Features is available.
class ComplexFeature : public Feature {
 public:
  // Takes ownership of Feature*s contained in |features|.
  explicit ComplexFeature(std::vector<Feature*>* features);
  ~ComplexFeature() override;

  // extensions::Feature:
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     Manifest::Location location,
                                     int manifest_version,
                                     Platform platform) const override;
  Availability IsAvailableToContext(const Extension* extension,
                                    Context context,
                                    const GURL& url,
                                    Platform platform) const override;
  Availability IsAvailableToEnvironment() const override;
  bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const override;
  bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const override;

 protected:
  // Feature:
  bool IsInternal() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FeaturesGenerationTest, FeaturesTest);

  using FeatureList = std::vector<std::unique_ptr<Feature>>;
  FeatureList features_;

  DISALLOW_COPY_AND_ASSIGN(ComplexFeature);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
