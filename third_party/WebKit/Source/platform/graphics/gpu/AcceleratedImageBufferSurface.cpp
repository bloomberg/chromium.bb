/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"

#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

AcceleratedImageBufferSurface::AcceleratedImageBufferSurface(
    const IntSize& size,
    const CanvasColorParams& color_params)
    : ImageBufferSurface(size, color_params) {
  context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper_)
    return;
  GrContext* gr_context =
      context_provider_wrapper_->ContextProvider()->GetGrContext();

  CHECK(gr_context);

  SkImageInfo info = SkImageInfo::Make(size.Width(), size.Height(),
                                       color_params.GetSkColorType(),
                                       color_params.GetSkAlphaType());
  // In legacy mode the backing SkSurface should not have any color space.
  // If color correct rendering is enabled only for SRGB, still the backing
  // surface should not have any color space and the treatment of legacy data
  // as SRGB will be managed by wrapping the internal SkCanvas inside a
  // SkColorSpaceXformCanvas. If color correct rendering is enbaled for other
  // color spaces, we set the color space properly.
  if (RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled())
    info = info.makeColorSpace(color_params.GetSkColorSpaceForSkSurfaces());

  SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  surface_ = SkSurface::MakeRenderTarget(gr_context, SkBudgeted::kYes, info,
                                         0 /* sampleCount */,
                                         ColorParams().GetSkSurfaceProps());
  if (!surface_)
    return;

  sk_sp<SkColorSpace> xform_canvas_color_space = nullptr;
  if (!color_params.LinearPixelMath())
    xform_canvas_color_space = color_params.GetSkColorSpace();
  canvas_ = WTF::WrapUnique(
      new SkiaPaintCanvas(surface_->getCanvas(), xform_canvas_color_space));
  Clear();

  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas_->save();
}

bool AcceleratedImageBufferSurface::IsValid() const {
  // Note: SharedGpuContext::ContextProviderWrapper() is called in order to
  // actively detect not-yet-handled context losses.
  return surface_ && context_provider_wrapper_;
}

RefPtr<StaticBitmapImage> AcceleratedImageBufferSurface::NewImageSnapshot(
    AccelerationHint,
    SnapshotReason) {
  if (!IsValid())
    return nullptr;
  // Must make a copy of the WeakPtr because CreateFromSkImage only takes
  // r-value references.
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      context_provider_wrapper_;
  RefPtr<AcceleratedStaticBitmapImage> image =
      AcceleratedStaticBitmapImage::CreateFromSkImage(
          surface_->makeImageSnapshot(), std::move(context_provider_wrapper));
  image->RetainOriginalSkImageForCopyOnWrite();
  return image;
}

GLuint AcceleratedImageBufferSurface::GetBackingTextureHandleForOverwrite() {
  if (!surface_)
    return 0;
  return skia::GrBackendObjectToGrGLTextureInfo(
             surface_->getTextureHandle(
                 SkSurface::kDiscardWrite_TextureHandleAccess))
      ->fID;
}

bool AcceleratedImageBufferSurface::WritePixels(const SkImageInfo& orig_info,
                                                const void* pixels,
                                                size_t row_bytes,
                                                int x,
                                                int y) {
  return surface_->getCanvas()->writePixels(orig_info, pixels, row_bytes, x, y);
}

}  // namespace blink
