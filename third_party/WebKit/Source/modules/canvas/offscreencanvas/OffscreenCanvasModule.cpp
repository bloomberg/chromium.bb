// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/offscreencanvas/OffscreenCanvasModule.h"

#include "core/dom/ExecutionContext.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/canvas/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

namespace blink {

void OffscreenCanvasModule::getContext(
    ExecutionContext* execution_context,
    OffscreenCanvas& offscreen_canvas,
    const String& id,
    const CanvasContextCreationAttributes& attributes,
    ExceptionState& exception_state,
    OffscreenRenderingContext& result) {
  if (offscreen_canvas.IsNeutered()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "OffscreenCanvas object is detached");
    return;
  }

  // OffscreenCanvas cannot be transferred after getContext, so this execution
  // context will always be the right one from here on.
  CanvasRenderingContext* context = offscreen_canvas.GetCanvasRenderingContext(
      execution_context, id, attributes);
  if (context)
    context->SetOffscreenCanvasGetContextResult(result);
}

}  // namespace blink
