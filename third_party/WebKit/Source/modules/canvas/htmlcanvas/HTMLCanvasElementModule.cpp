// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/htmlcanvas/HTMLCanvasElementModule.h"

#include "core/dom/DOMNodeIds.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/offscreencanvas/OffscreenCanvas.h"

namespace blink {

void HTMLCanvasElementModule::getContext(
    HTMLCanvasElement& canvas,
    const String& type,
    const CanvasContextCreationAttributes& attributes,
    ExceptionState& exception_state,
    RenderingContext& result) {
  if (canvas.SurfaceLayerBridge()) {
    // The existence of canvas surfaceLayerBridge indicates that
    // HTMLCanvasElement.transferControlToOffscreen() has been called.
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Cannot get context from a canvas that "
                                      "has transferred its control to "
                                      "offscreen.");
    return;
  }

  CanvasRenderingContext* context =
      canvas.GetCanvasRenderingContext(type, attributes);
  if (context) {
    context->SetCanvasGetContextResult(result);
  }
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreen(
    HTMLCanvasElement& canvas,
    ExceptionState& exception_state) {
  if (canvas.SurfaceLayerBridge()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot transfer control from a canvas for more than one time.");
    return nullptr;
  }

  canvas.CreateLayer();

  return TransferControlToOffscreenInternal(canvas, exception_state);
}

OffscreenCanvas* HTMLCanvasElementModule::TransferControlToOffscreenInternal(
    HTMLCanvasElement& canvas,
    ExceptionState& exception_state) {
  if (canvas.RenderingContext()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "Cannot transfer control from a canvas that has a rendering context.");
    return nullptr;
  }
  OffscreenCanvas* offscreen_canvas =
      OffscreenCanvas::Create(canvas.width(), canvas.height());

  int canvas_id = DOMNodeIds::IdForNode(&canvas);
  offscreen_canvas->SetPlaceholderCanvasId(canvas_id);
  canvas.RegisterPlaceholder(canvas_id);

  SurfaceLayerBridge* bridge = canvas.SurfaceLayerBridge();
  if (bridge) {
    offscreen_canvas->SetFrameSinkId(bridge->GetFrameSinkId().client_id(),
                                     bridge->GetFrameSinkId().sink_id());
  }
  return offscreen_canvas;
}
}  // namespace blink
