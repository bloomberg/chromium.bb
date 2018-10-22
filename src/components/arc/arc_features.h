// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef COMPONENTS_ARC_ARC_FEATURES_H_
#define COMPONENTS_ARC_ARC_FEATURES_H_

#include "base/feature_list.h"

namespace arc {

// Please keep alphabetized.
extern const base::Feature kAvailableForChildAccountFeature;
extern const base::Feature kBootCompletedBroadcastFeature;
extern const base::Feature kCleanArcDataOnRegularToChildTransitionFeature;
extern const base::Feature kEnableChildToRegularTransitionFeature;
extern const base::Feature kEnableInputMethodFeature;
extern const base::Feature kEnableRegularToChildTransitionFeature;
extern const base::Feature kNativeBridgeExperimentFeature;
extern const base::Feature kSmartTextSelectionFeature;
extern const base::Feature kUsbHostFeature;
extern const base::Feature kVpnFeature;

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_FEATURES_H_
