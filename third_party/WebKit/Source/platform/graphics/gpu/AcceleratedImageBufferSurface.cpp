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

#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

AcceleratedImageBufferSurface::AcceleratedImageBufferSurface(
    const IntSize& size,
    OpacityMode opacity_mode,
    sk_sp<SkColorSpace> color_space,
    SkColorType color_type)
    : ImageBufferSurface(size, opacity_mode, color_space, color_type) {
  if (!SharedGpuContext::IsValid())
    return;
  GrContext* gr_context = SharedGpuContext::Gr();
  context_id_ = SharedGpuContext::ContextId();
  CHECK(gr_context);

  SkAlphaType alpha_type =
      (kOpaque == opacity_mode) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  SkImageInfo info = SkImageInfo::Make(size.Width(), size.Height(), color_type,
                                       alpha_type, color_space);
  SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  surface_ = SkSurface::MakeRenderTarget(
      gr_context, SkBudgeted::kYes, info, 0 /* sampleCount */,
      kOpaque == opacity_mode ? nullptr : &disable_lcd_props);
  if (!surface_)
    return;

  canvas_ = WTF::WrapUnique(new SkiaPaintCanvas(surface_->getCanvas()));
  Clear();

  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas_->save();
}

bool AcceleratedImageBufferSurface::IsValid() const {
  return surface_ && SharedGpuContext::IsValid() &&
         context_id_ == SharedGpuContext::ContextId();
}

sk_sp<SkImage> AcceleratedImageBufferSurface::NewImageSnapshot(AccelerationHint,
                                                               SnapshotReason) {
  return surface_->makeImageSnapshot();
}

GLuint AcceleratedImageBufferSurface::GetBackingTextureHandleForOverwrite() {
  if (!surface_)
    return 0;
  return skia::GrBackendObjectToGrGLTextureInfo(
             surface_->getTextureHandle(
                 SkSurface::kDiscardWrite_TextureHandleAccess))
      ->fID;
}

}  // namespace blink
