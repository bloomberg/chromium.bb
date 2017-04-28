// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGFilterPainter_h
#define SVGFilterPainter_h

#include <memory>
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutObject;
class LayoutSVGResourceFilter;

class SVGFilterRecordingContext {
  USING_FAST_MALLOC(SVGFilterRecordingContext);
  WTF_MAKE_NONCOPYABLE(SVGFilterRecordingContext);

 public:
  explicit SVGFilterRecordingContext(GraphicsContext& initial_context)
      : initial_context_(initial_context) {}

  GraphicsContext* BeginContent();
  sk_sp<PaintRecord> EndContent(const FloatRect&);
  void Abort();

  GraphicsContext& PaintingContext() const { return initial_context_; }

 private:
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
  GraphicsContext& initial_context_;
};

class SVGFilterPainter {
  STACK_ALLOCATED();

 public:
  SVGFilterPainter(LayoutSVGResourceFilter& filter) : filter_(filter) {}

  // Returns the context that should be used to paint the filter contents, or
  // null if the content should not be recorded.
  GraphicsContext* PrepareEffect(const LayoutObject&,
                                 SVGFilterRecordingContext&);
  void FinishEffect(const LayoutObject&, SVGFilterRecordingContext&);

 private:
  LayoutSVGResourceFilter& filter_;
};

}  // namespace blink

#endif  // SVGFilterPainter_h
