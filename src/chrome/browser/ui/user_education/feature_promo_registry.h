// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_

#include <map>

#include "chrome/browser/ui/user_education/feature_promo_specification.h"

namespace base {
struct Feature;
}

// Stores parameters for in-product help promos. For each registered
// IPH, has the bubble parameters and a method for getting an anchor
// view for a given BrowserView. Promos should be registered here when
// feasible.
class FeaturePromoRegistry {
 public:
  FeaturePromoRegistry();
  ~FeaturePromoRegistry();

  // Determines whether or not a particular feature is registered.
  bool IsFeatureRegistered(const base::Feature& iph_feature) const;

  // Returns the FeaturePromoSpecification to start an IPH for
  // the given feature. |iph_feature| is the feature to show for.
  // |browser_view| is the window it should show in.
  //
  // The params must be used immediately since it contains a View
  // pointer that may become stale. This may return nothing in which
  // case the promo shouldn't show.
  const FeaturePromoSpecification* GetParamsForFeature(
      const base::Feature& iph_feature) const;

  // Registers a feature promo.
  //
  // Prefer putting these calls in the body of RegisterKnownFeatures()
  // when possible.
  void RegisterFeature(FeaturePromoSpecification spec);

  const std::map<const base::Feature*, FeaturePromoSpecification>&
  GetRegisteredFeaturePromoSpecifications() {
    return feature_promo_data_;
  }

  void ClearFeaturesForTesting();

 private:
  std::map<const base::Feature*, FeaturePromoSpecification> feature_promo_data_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
