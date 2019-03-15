// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_features.h"

namespace features {

const base::Feature kResamplingScrollEvents{"ResamplingScrollEvents",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kScrollPredictorTypeChoice{
    "ScrollPredictorTypeChoice", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendMouseLeaveEvents{"SendMouseLeaveEvents",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNoHoverAfterLayoutChange{
    "NoHoverAfterLayoutChange", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNoHoverDuringScroll{"NoHoverDuringScroll",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCompositorTouchAction{"CompositorTouchAction",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFallbackCursorMode{"FallbackCursorMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
}
