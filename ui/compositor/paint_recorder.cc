// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_recorder.h"

#include "cc/playback/display_item_list.h"
#include "cc/playback/drawing_display_item.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/compositor/paint_cache.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/skia_util.h"

namespace ui {

PaintRecorder::PaintRecorder(const PaintContext& context, PaintCache* cache)
    : context_(context),
      owned_canvas_(
          // If the |context| has a canvas, we'll just point to it in |canvas_|
          // so use anything here to initialize the gfx::Canvas without any
          // allocations.
          // The SkCanvas reference returned by beginRecording is shared with
          // the recorder_ so no need to store a RefPtr to it on this class.
          (context.canvas_
               ? context.canvas_->sk_canvas()
               : skia::SharePtr(
                     context.recorder_->beginRecording(
                         gfx::RectToSkRect(context.bounds_),
                         nullptr /* no SkRTreeFactory */,
                         SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag))
                     .get()),
          context.device_scale_factor_),
      // In the case that we use a shared canvas from PaintContext, the user of
      // PaintRecorder is expected to Save/Restore any changes made to the
      // context while using PaintRecorder.
      canvas_(context.canvas_ ? context.canvas_ : &owned_canvas_),
      cache_(cache) {
#if DCHECK_IS_ON()
  DCHECK(!context.inside_paint_recorder_);
  context.inside_paint_recorder_ = true;
#endif
}

PaintRecorder::PaintRecorder(const PaintContext& context)
    : PaintRecorder(context, nullptr) {
}

PaintRecorder::~PaintRecorder() {
#if DCHECK_IS_ON()
  context_.inside_paint_recorder_ = false;
#endif

  if (!context_.list_)
    return;

  auto* item = context_.list_->CreateAndAppendItem<cc::DrawingDisplayItem>();
  item->SetNew(skia::AdoptRef(context_.recorder_->endRecordingAsPicture()));
  if (cache_)
    cache_->SetCache(item);
}

}  // namespace ui
