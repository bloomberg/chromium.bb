/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_GRAPHICS_CONTEXT_3D_PROVIDER_H_

#include <cstdint>
#include "base/callback_forward.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class GrContext;

namespace cc {
class ImageDecodeCache;
}  // namespace cc

namespace gpu {
struct Capabilities;
struct GpuFeatureInfo;

namespace gles2 {
class GLES2Interface;
}

namespace webgpu {
class WebGPUInterface;
}
}

namespace viz {
class GLHelper;
}

namespace blink {

enum CanvasColorSpace {
  kSRGBCanvasColorSpace,
  kLinearRGBCanvasColorSpace,
  kRec2020CanvasColorSpace,
  kP3CanvasColorSpace,
  kMaxCanvasColorSpace = kP3CanvasColorSpace
};

enum CanvasPixelFormat {
  kRGBA8CanvasPixelFormat,
  kF16CanvasPixelFormat,
  kMaxCanvasPixelFormat = kF16CanvasPixelFormat
};

inline SkColorType PixelFormatToSkColorType(CanvasPixelFormat pixel_format) {
  switch (pixel_format) {
    case kF16CanvasPixelFormat:
      return kRGBA_F16_SkColorType;
    case kRGBA8CanvasPixelFormat:
      return kN32_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

inline sk_sp<SkColorSpace> CanvasColorSpaceToSkColorSpace(
    CanvasColorSpace color_space) {
  SkColorSpace::Gamut gamut = SkColorSpace::kSRGB_Gamut;
  SkColorSpace::RenderTargetGamma gamma = SkColorSpace::kSRGB_RenderTargetGamma;
  switch (color_space) {
    case kSRGBCanvasColorSpace:
      break;
    case kLinearRGBCanvasColorSpace:
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
    case kRec2020CanvasColorSpace:
      gamut = SkColorSpace::kRec2020_Gamut;
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
    case kP3CanvasColorSpace:
      gamut = SkColorSpace::kDCIP3_D65_Gamut;
      gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
  }
  return SkColorSpace::MakeRGB(gamma, gamut);
}

class WebGraphicsContext3DProvider {
 public:
  virtual ~WebGraphicsContext3DProvider() = default;

  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;
  virtual gpu::webgpu::WebGPUInterface* WebGPUInterface() = 0;
  virtual bool BindToCurrentThread() = 0;
  virtual GrContext* GetGrContext() = 0;
  virtual const gpu::Capabilities& GetCapabilities() const = 0;
  virtual const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const = 0;
  // Creates a viz::GLHelper after first call and returns that instance. This
  // method cannot return null.
  virtual viz::GLHelper* GetGLHelper() = 0;

  virtual void SetLostContextCallback(base::RepeatingClosure) = 0;
  virtual void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char* msg, int32_t id)>) = 0;
  // Return a static software image decode cache that matches this color
  // space and pixel format.
  virtual cc::ImageDecodeCache* ImageDecodeCache(
      CanvasColorSpace color_space,
      CanvasPixelFormat pixel_format) = 0;
};

}  // namespace blink

#endif
