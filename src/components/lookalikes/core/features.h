// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_FEATURES_H_
#define COMPONENTS_LOOKALIKES_CORE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace lookalikes {
namespace features {

// This feature enables interstitial warnings for target embedding lookalikes.
COMPONENT_EXPORT(LOOKALIKES_FEATURES)
extern const base::Feature kDetectTargetEmbeddingLookalikes;

}  // namespace features
}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_FEATURES_H_
