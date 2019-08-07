// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_PRESENTATION_FEEDBACK_MAP_H_
#define COMPONENTS_VIZ_COMMON_PRESENTATION_FEEDBACK_MAP_H_

#include "base/containers/flat_map.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

using PresentationFeedbackMap =
    base::flat_map<uint32_t, gfx::PresentationFeedback>;

}

#endif  // COMPONENTS_VIZ_COMMON_PRESENTATION_FEEDBACK_MAP_H_
