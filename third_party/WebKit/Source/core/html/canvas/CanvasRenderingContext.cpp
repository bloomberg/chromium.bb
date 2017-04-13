/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/canvas/CanvasRenderingContext.h"

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"

namespace blink {

CanvasRenderingContext::CanvasRenderingContext(
    HTMLCanvasElement* canvas,
    OffscreenCanvas* offscreen_canvas,
    const CanvasContextCreationAttributes& attrs)
    : canvas_(canvas),
      offscreen_canvas_(offscreen_canvas),
      color_space_(kLegacyCanvasColorSpace),
      pixel_format_(kRGBA8CanvasPixelFormat),
      linear_pixel_math_(false),
      creation_attributes_(attrs) {
  if (RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled() &&
      RuntimeEnabledFeatures::colorCorrectRenderingEnabled()) {
    // Set the default color space to SRGB and continue
    color_space_ = kSRGBCanvasColorSpace;
    if (creation_attributes_.colorSpace() == kRec2020CanvasColorSpaceName)
      color_space_ = kRec2020CanvasColorSpace;
    else if (creation_attributes_.colorSpace() == kP3CanvasColorSpaceName)
      color_space_ = kP3CanvasColorSpace;

    // For now, we only support RGBA8 (for SRGB) and F16 (for all). Everything
    // else falls back to SRGB + RGBA8.
    if (creation_attributes_.pixelFormat() == kF16CanvasPixelFormatName) {
      pixel_format_ = kF16CanvasPixelFormat;
      linear_pixel_math_ = true;
    } else {
      color_space_ = kSRGBCanvasColorSpace;
      pixel_format_ = kRGBA8CanvasPixelFormat;
    }
  }

  // Make m_creationAttributes reflect the effective colorSpace, pixelFormat and
  // linearPixelMath rather than the requested one.
  creation_attributes_.setColorSpace(ColorSpaceAsString());
  creation_attributes_.setPixelFormat(PixelFormatAsString());
  creation_attributes_.setLinearPixelMath(LinearPixelMath());
}

WTF::String CanvasRenderingContext::ColorSpaceAsString() const {
  switch (color_space_) {
    case kLegacyCanvasColorSpace:
      return kLegacyCanvasColorSpaceName;
    case kSRGBCanvasColorSpace:
      return kSRGBCanvasColorSpaceName;
    case kRec2020CanvasColorSpace:
      return kRec2020CanvasColorSpaceName;
    case kP3CanvasColorSpace:
      return kP3CanvasColorSpaceName;
  };
  CHECK(false);
  return "";
}

WTF::String CanvasRenderingContext::PixelFormatAsString() const {
  switch (pixel_format_) {
    case kRGBA8CanvasPixelFormat:
      return kRGBA8CanvasPixelFormatName;
    case kRGB10A2CanvasPixelFormat:
      return kRGB10A2CanvasPixelFormatName;
    case kRGBA12CanvasPixelFormat:
      return kRGBA12CanvasPixelFormatName;
    case kF16CanvasPixelFormat:
      return kF16CanvasPixelFormatName;
  };
  CHECK(false);
  return "";
}

gfx::ColorSpace CanvasRenderingContext::GfxColorSpace() const {
  switch (color_space_) {
    case kLegacyCanvasColorSpace:
      return gfx::ColorSpace::CreateSRGB();
    case kSRGBCanvasColorSpace:
      if (pixel_format_ == kF16CanvasPixelFormat)
        return gfx::ColorSpace::CreateSCRGBLinear();
      return gfx::ColorSpace::CreateSRGB();
    case kRec2020CanvasColorSpace:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::IEC61966_2_1);
    case kP3CanvasColorSpace:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTEST432_1,
                             gfx::ColorSpace::TransferID::IEC61966_2_1);
  }
  NOTREACHED();
  return gfx::ColorSpace();
}

sk_sp<SkColorSpace> CanvasRenderingContext::SkSurfaceColorSpace() const {
  if (SkSurfacesUseColorSpace())
    return GfxColorSpace().ToSkColorSpace();
  return nullptr;
}

bool CanvasRenderingContext::SkSurfacesUseColorSpace() const {
  return color_space_ != kLegacyCanvasColorSpace;
}

ColorBehavior CanvasRenderingContext::ColorBehaviorForMediaDrawnToCanvas()
    const {
  if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
    return ColorBehavior::TransformTo(GfxColorSpace());
  return ColorBehavior::TransformToGlobalTarget();
}

SkColorType CanvasRenderingContext::ColorType() const {
  if (pixel_format_ == kF16CanvasPixelFormat)
    return kRGBA_F16_SkColorType;
  return kN32_SkColorType;
}

void CanvasRenderingContext::Dispose() {
  if (finalize_frame_scheduled_) {
    Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
  }

  // HTMLCanvasElement and CanvasRenderingContext have a circular reference.
  // When the pair is no longer reachable, their destruction order is non-
  // deterministic, so the first of the two to be destroyed needs to notify
  // the other in order to break the circular reference.  This is to avoid
  // an error when CanvasRenderingContext::didProcessTask() is invoked
  // after the HTMLCanvasElement is destroyed.
  if (canvas()) {
    canvas()->DetachContext();
    canvas_ = nullptr;
  }
  if (offscreenCanvas()) {
    offscreenCanvas()->DetachContext();
    offscreen_canvas_ = nullptr;
  }
}

void CanvasRenderingContext::DidDraw(const SkIRect& dirty_rect) {
  canvas()->DidDraw(SkRect::Make(dirty_rect));
  NeedsFinalizeFrame();
}

void CanvasRenderingContext::DidDraw() {
  canvas()->DidDraw();
  NeedsFinalizeFrame();
}

void CanvasRenderingContext::NeedsFinalizeFrame() {
  if (!finalize_frame_scheduled_) {
    finalize_frame_scheduled_ = true;
    Platform::Current()->CurrentThread()->AddTaskObserver(this);
  }
}

void CanvasRenderingContext::DidProcessTask() {
  Platform::Current()->CurrentThread()->RemoveTaskObserver(this);
  finalize_frame_scheduled_ = false;
  // The end of a script task that drew content to the canvas is the point
  // at which the current frame may be considered complete.
  if (canvas())
    canvas()->FinalizeFrame();
  if (offscreenCanvas())
    offscreenCanvas()->FinalizeFrame();
  FinalizeFrame();
}

CanvasRenderingContext::ContextType CanvasRenderingContext::ContextTypeFromId(
    const String& id) {
  if (id == "2d")
    return kContext2d;
  if (id == "experimental-webgl")
    return kContextExperimentalWebgl;
  if (id == "webgl")
    return kContextWebgl;
  if (id == "webgl2")
    return kContextWebgl2;
  if (id == "bitmaprenderer" &&
      RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled()) {
    return kContextImageBitmap;
  }
  return kContextTypeCount;
}

CanvasRenderingContext::ContextType
CanvasRenderingContext::ResolveContextTypeAliases(
    CanvasRenderingContext::ContextType type) {
  if (type == kContextExperimentalWebgl)
    return kContextWebgl;
  return type;
}

bool CanvasRenderingContext::WouldTaintOrigin(
    CanvasImageSource* image_source,
    SecurityOrigin* destination_security_origin) {
  const KURL& source_url = image_source->SourceURL();
  bool has_url = (source_url.IsValid() && !source_url.IsAboutBlankURL());

  if (has_url) {
    if (source_url.ProtocolIsData() ||
        clean_urls_.Contains(source_url.GetString()))
      return false;
    if (dirty_urls_.Contains(source_url.GetString()))
      return true;
  }

  bool taint_origin =
      image_source->WouldTaintOrigin(destination_security_origin);
  if (has_url) {
    if (taint_origin)
      dirty_urls_.insert(source_url.GetString());
    else
      clean_urls_.insert(source_url.GetString());
  }
  return taint_origin;
}

DEFINE_TRACE(CanvasRenderingContext) {
  visitor->Trace(canvas_);
  visitor->Trace(offscreen_canvas_);
}

}  // namespace blink
