// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FramePaintTiming_h
#define FramePaintTiming_h

#include "core/frame/LocalFrame.h"
#include "core/paint/PaintTiming.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/heap/Handle.h"

namespace blink {

class FramePaintTiming {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(FramePaintTiming);

 public:
  FramePaintTiming(GraphicsContext& context, const LocalFrame* frame)
      : context_(context), frame_(frame) {
    context_.GetPaintController().BeginFrame(frame_);
  }

  ~FramePaintTiming() {
    DCHECK(frame_->GetDocument());
    FrameFirstPaint result = context_.GetPaintController().EndFrame(frame_);
    PaintTiming::From(*frame_->GetDocument())
        .NotifyPaint(result.first_painted, result.text_painted,
                     result.image_painted);
  }

 private:
  GraphicsContext& context_;
  Member<const LocalFrame> frame_;
};

}  // namespace blink

#endif  // FramePaintTiming_h
