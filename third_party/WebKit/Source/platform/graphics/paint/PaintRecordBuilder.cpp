// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintRecordBuilder.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

PaintRecordBuilder::PaintRecordBuilder(const FloatRect& bounds,
                                       SkMetaData* meta_data,
                                       GraphicsContext* containing_context,
                                       PaintController* paint_controller)
    : paint_controller_(nullptr), bounds_(bounds) {
  GraphicsContext::DisabledMode disabled_mode =
      GraphicsContext::kNothingDisabled;
  if (containing_context && containing_context->ContextDisabled())
    disabled_mode = GraphicsContext::kFullyDisabled;

  if (paint_controller) {
    paint_controller_ = paint_controller;
  } else {
    paint_controller_ptr_ = PaintController::Create();
    paint_controller_ = paint_controller_ptr_.get();
  }

  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    paint_controller_->UpdateCurrentPaintChunkProperties(
        nullptr, PropertyTreeState::Root());
  }

#if DCHECK_IS_ON()
  paint_controller_->SetUsage(PaintController::kForPaintRecordBuilder);
#endif

  const HighContrastSettings* high_contrast_settings =
      containing_context ? &containing_context->high_contrast_settings()
                         : nullptr;
  context_ = WTF::WrapUnique(
      new GraphicsContext(*paint_controller_, disabled_mode, meta_data));
  if (high_contrast_settings)
    context_->SetHighContrast(*high_contrast_settings);

  if (containing_context) {
    context_->SetDeviceScaleFactor(containing_context->DeviceScaleFactor());
    context_->SetPrinting(containing_context->Printing());
  }
}

PaintRecordBuilder::~PaintRecordBuilder() {
#if DCHECK_IS_ON()
  paint_controller_->SetUsage(PaintController::kForNormalUsage);
#endif
}

sk_sp<PaintRecord> PaintRecordBuilder::EndRecording() {
  context_->BeginRecording(bounds_);
  paint_controller_->CommitNewDisplayItems();
  paint_controller_->GetPaintArtifact().Replay(*context_);
  return context_->EndRecording();
}

void PaintRecordBuilder::EndRecording(
    PaintCanvas& canvas,
    const PropertyTreeState& property_tree_state) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    canvas.drawPicture(EndRecording());
  } else {
    paint_controller_->CommitNewDisplayItems();
    paint_controller_->GetPaintArtifact().Replay(canvas, property_tree_state);
  }
}

}  // namespace blink
