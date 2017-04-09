// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPaintInvalidator.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason HTMLCanvasPaintInvalidator::InvalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(html_canvas_, context_).InvalidatePaintIfNeeded();

  HTMLCanvasElement* element = toHTMLCanvasElement(html_canvas_.GetNode());
  if (element->IsDirty()) {
    element->DoDeferredPaintInvalidation();
    if (reason < kPaintInvalidationRectangle)
      reason = kPaintInvalidationRectangle;
  }

  return reason;
}

}  // namespace blink
