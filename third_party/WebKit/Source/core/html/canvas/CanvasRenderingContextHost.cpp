// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasRenderingContextHost.h"

#include "core/html/canvas/CanvasRenderingContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

CanvasRenderingContextHost::CanvasRenderingContextHost() = default;

ScriptPromise CanvasRenderingContextHost::Commit(
    scoped_refptr<StaticBitmapImage> bitmap_image,
    const SkIRect& damage_rect,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(kInvalidStateError,
                                    "Commit() was called on a rendering "
                                    "context that was not created from an "
                                    "OffscreenCanvas.");
  return exception_state.Reject(script_state);
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage(const IntSize& size) const {
  if (!IsValidImageSize(size))
    return nullptr;
  sk_sp<SkSurface> surface =
      SkSurface::MakeRasterN32Premul(size.Width(), size.Height());
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize(Size());
}

}  // namespace blink
