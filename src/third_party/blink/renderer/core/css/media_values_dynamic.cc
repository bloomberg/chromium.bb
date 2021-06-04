// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values_dynamic.h"

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

MediaValues* MediaValuesDynamic::Create(Document& document) {
  return MediaValuesDynamic::Create(document.GetFrame());
}

MediaValues* MediaValuesDynamic::Create(LocalFrame* frame) {
  if (!frame || !frame->View() || !frame->GetDocument() ||
      !frame->GetDocument()->GetLayoutView())
    return MakeGarbageCollected<MediaValuesCached>();
  return MakeGarbageCollected<MediaValuesDynamic>(frame);
}

MediaValuesDynamic::MediaValuesDynamic(LocalFrame* frame)
    : frame_(frame),
      viewport_dimensions_overridden_(false),
      viewport_width_override_(0),
      viewport_height_override_(0) {
  DCHECK(frame_);
}

MediaValuesDynamic::MediaValuesDynamic(LocalFrame* frame,
                                       bool overridden_viewport_dimensions,
                                       double viewport_width,
                                       double viewport_height)
    : frame_(frame),
      viewport_dimensions_overridden_(overridden_viewport_dimensions),
      viewport_width_override_(viewport_width),
      viewport_height_override_(viewport_height) {
  DCHECK(frame_);
}

MediaValues* MediaValuesDynamic::Copy() const {
  return MakeGarbageCollected<MediaValuesDynamic>(
      frame_, viewport_dimensions_overridden_, viewport_width_override_,
      viewport_height_override_);
}

bool MediaValuesDynamic::ComputeLength(double value,
                                       CSSPrimitiveValue::UnitType type,
                                       int& result) const {
  return MediaValues::ComputeLength(value, type,
                                    CalculateDefaultFontSize(frame_),
                                    ViewportWidth(), ViewportHeight(), result);
}

bool MediaValuesDynamic::ComputeLength(double value,
                                       CSSPrimitiveValue::UnitType type,
                                       double& result) const {
  return MediaValues::ComputeLength(value, type,
                                    CalculateDefaultFontSize(frame_),
                                    ViewportWidth(), ViewportHeight(), result);
}

double MediaValuesDynamic::ViewportWidth() const {
  if (viewport_dimensions_overridden_)
    return viewport_width_override_;
  return CalculateViewportWidth(frame_);
}

double MediaValuesDynamic::ViewportHeight() const {
  if (viewport_dimensions_overridden_)
    return viewport_height_override_;
  return CalculateViewportHeight(frame_);
}

int MediaValuesDynamic::DeviceWidth() const {
  return CalculateDeviceWidth(frame_);
}

int MediaValuesDynamic::DeviceHeight() const {
  return CalculateDeviceHeight(frame_);
}

float MediaValuesDynamic::DevicePixelRatio() const {
  return CalculateDevicePixelRatio(frame_);
}

int MediaValuesDynamic::ColorBitsPerComponent() const {
  return CalculateColorBitsPerComponent(frame_);
}

int MediaValuesDynamic::MonochromeBitsPerComponent() const {
  return CalculateMonochromeBitsPerComponent(frame_);
}

mojom::blink::PointerType MediaValuesDynamic::PrimaryPointerType() const {
  return CalculatePrimaryPointerType(frame_);
}

int MediaValuesDynamic::AvailablePointerTypes() const {
  return CalculateAvailablePointerTypes(frame_);
}

mojom::blink::HoverType MediaValuesDynamic::PrimaryHoverType() const {
  return CalculatePrimaryHoverType(frame_);
}

int MediaValuesDynamic::AvailableHoverTypes() const {
  return CalculateAvailableHoverTypes(frame_);
}

bool MediaValuesDynamic::ThreeDEnabled() const {
  return CalculateThreeDEnabled(frame_);
}

bool MediaValuesDynamic::InImmersiveMode() const {
  return CalculateInImmersiveMode(frame_);
}

const String MediaValuesDynamic::MediaType() const {
  return CalculateMediaType(frame_);
}

blink::mojom::DisplayMode MediaValuesDynamic::DisplayMode() const {
  return CalculateDisplayMode(frame_);
}

bool MediaValuesDynamic::StrictMode() const {
  return CalculateStrictMode(frame_);
}

ColorSpaceGamut MediaValuesDynamic::ColorGamut() const {
  return CalculateColorGamut(frame_);
}

mojom::blink::PreferredColorScheme MediaValuesDynamic::GetPreferredColorScheme()
    const {
  return CalculatePreferredColorScheme(frame_);
}

mojom::blink::PreferredContrast MediaValuesDynamic::GetPreferredContrast()
    const {
  return CalculatePreferredContrast(frame_);
}

bool MediaValuesDynamic::PrefersReducedMotion() const {
  return CalculatePrefersReducedMotion(frame_);
}

bool MediaValuesDynamic::PrefersReducedData() const {
  return CalculatePrefersReducedData(frame_);
}

ForcedColors MediaValuesDynamic::GetForcedColors() const {
  return CalculateForcedColors();
}

NavigationControls MediaValuesDynamic::GetNavigationControls() const {
  return CalculateNavigationControls(frame_);
}

ScreenSpanning MediaValuesDynamic::GetScreenSpanning() const {
  return CalculateScreenSpanning(frame_);
}

DevicePosture MediaValuesDynamic::GetDevicePosture() const {
  return CalculateDevicePosture(frame_);
}

Document* MediaValuesDynamic::GetDocument() const {
  return frame_->GetDocument();
}

bool MediaValuesDynamic::HasValues() const {
  return frame_;
}

void MediaValuesDynamic::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  MediaValues::Trace(visitor);
}

void MediaValuesDynamic::OverrideViewportDimensions(double width,
                                                    double height) {
  viewport_dimensions_overridden_ = true;
  viewport_width_override_ = width;
  viewport_height_override_ = height;
}

}  // namespace blink
