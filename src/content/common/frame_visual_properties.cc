// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_visual_properties.h"

namespace content {

FrameVisualProperties::FrameVisualProperties() = default;

FrameVisualProperties::FrameVisualProperties(
    const FrameVisualProperties& other) = default;

FrameVisualProperties::~FrameVisualProperties() = default;

FrameVisualProperties& FrameVisualProperties::operator=(
    const FrameVisualProperties& other) = default;

}  // namespace content
