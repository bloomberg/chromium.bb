// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_recorder.h"

#include "cc/resources/display_item_list.h"
#include "cc/resources/drawing_display_item.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace ui {

PaintRecorder::PaintRecorder(const PaintContext& context)
    : context_(context), canvas_(context.canvas_) {
#if DCHECK_IS_ON()
  DCHECK(!context.inside_paint_recorder_);
  context.inside_paint_recorder_ = true;
#endif

  if (context.list_) {
    SkRTreeFactory* no_factory = nullptr;
    // This SkCancas is shared with the recorder_ so no need to store a RefPtr
    // to it on this class.
    skia::RefPtr<SkCanvas> skcanvas =
        skia::SharePtr(context.recorder_->beginRecording(
            gfx::RectToSkRect(context.bounds_), no_factory,
            SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag));
    owned_canvas_ = make_scoped_ptr(gfx::Canvas::CreateCanvasWithoutScaling(
        skcanvas.get(), context.device_scale_factor_));
    canvas_ = owned_canvas_.get();
  }
}

PaintRecorder::~PaintRecorder() {
#if DCHECK_IS_ON()
  context_.inside_paint_recorder_ = false;
#endif

  if (context_.list_) {
    context_.list_->AppendItem(cc::DrawingDisplayItem::Create(
        skia::AdoptRef(context_.recorder_->endRecordingAsPicture())));
  }
}

}  // namespace ui
