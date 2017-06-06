// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_recorder.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/compositor/paint_cache.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/skia_util.h"

namespace ui {

// This class records a reference to the context, the canvas returned
// by its recorder_, and the cache. Thus all 3 of these must remain
// valid for the lifetime of this object.
// If a |cache| is provided, this records into the |cache|'s PaintOpBuffer
// directly, then appends that to the |context|. If not, then this records
// to the |context|'s PaintOpBuffer.
PaintRecorder::PaintRecorder(const PaintContext& context,
                             const gfx::Size& recording_size,
                             PaintCache* cache)
    : context_(context),
      record_canvas_(cache ? cache->ResetCache() : context_.list_->StartPaint(),
                     gfx::RectToSkRect(gfx::Rect(recording_size))),
      canvas_(&record_canvas_, context.device_scale_factor_),
      cache_(cache),
      recording_size_(recording_size) {
#if DCHECK_IS_ON()
  DCHECK(!context.inside_paint_recorder_);
  context.inside_paint_recorder_ = true;
#endif
}

PaintRecorder::PaintRecorder(const PaintContext& context,
                             const gfx::Size& recording_size)
    : PaintRecorder(context, recording_size, nullptr) {}

PaintRecorder::~PaintRecorder() {
#if DCHECK_IS_ON()
  context_.inside_paint_recorder_ = false;
#endif
  // If using cache, append what we've saved there to the PaintContext.
  // Otherwise, the content is already stored in the PaintContext, and we can
  // just close it.
  if (cache_) {
    cache_->FinalizeCache();
    cache_->UseCache(context_, recording_size_);
  } else {
    gfx::Rect bounds_in_layer = context_.ToLayerSpaceBounds(recording_size_);
    context_.list_->EndPaintOfUnpaired(bounds_in_layer);
  }
}

}  // namespace ui
