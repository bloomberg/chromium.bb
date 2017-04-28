// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasPaintInvalidator_h
#define HTMLCanvasPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutHTMLCanvas;
struct PaintInvalidatorContext;

class HTMLCanvasPaintInvalidator {
  STACK_ALLOCATED();

 public:
  HTMLCanvasPaintInvalidator(const LayoutHTMLCanvas& html_canvas,
                             const PaintInvalidatorContext& context)
      : html_canvas_(html_canvas), context_(context) {}

  PaintInvalidationReason InvalidatePaint();

 private:
  const LayoutHTMLCanvas& html_canvas_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
