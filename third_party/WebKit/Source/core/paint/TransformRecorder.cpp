// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TransformRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

namespace blink {

TransformRecorder::TransformRecorder(GraphicsContext& context,
                                     const DisplayItemClient& client,
                                     const AffineTransform& transform)
    : context_(context), client_(client) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  skip_recording_for_identity_transform_ = transform.IsIdentity();

  if (skip_recording_for_identity_transform_)
    return;

  context_.GetPaintController().CreateAndAppend<BeginTransformDisplayItem>(
      client_, transform);
}

TransformRecorder::~TransformRecorder() {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  if (skip_recording_for_identity_transform_)
    return;

  context_.GetPaintController().EndItem<EndTransformDisplayItem>(client_);
}

}  // namespace blink
