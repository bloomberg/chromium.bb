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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_

#include <limits>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class ComputedStyle;
class LayoutView;
class Font;
class Element;

class CORE_EXPORT CSSToLengthConversionData {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT FontSizes {
    DISALLOW_NEW();

   public:
    FontSizes() : em_(0), rem_(0), font_(nullptr), zoom_(1) {}
    FontSizes(float em, float rem, const Font*, float zoom);
    FontSizes(const ComputedStyle*, const ComputedStyle* root_style);

    float Em() const { return em_; }
    float Rem() const { return rem_; }
    float Zoom() const;
    float Ex() const;
    float Ch() const;

   private:
    float em_;
    float rem_;
    const Font* font_;
    float zoom_;
  };

  class CORE_EXPORT ViewportSize {
    DISALLOW_NEW();

   public:
    ViewportSize() = default;
    ViewportSize(double width, double height)
        : large_width_(width),
          large_height_(height),
          small_width_(width),
          small_height_(height),
          dynamic_width_(width),
          dynamic_height_(height) {}

    explicit ViewportSize(const LayoutView*);

    // v*
    double Width() const { return LargeWidth(); }
    double Height() const { return LargeHeight(); }

    // lv*
    double LargeWidth() const { return large_width_; }
    double LargeHeight() const { return large_height_; }

    // sv*
    double SmallWidth() const { return small_width_; }
    double SmallHeight() const { return small_height_; }

    // dv*
    double DynamicWidth() const { return dynamic_width_; }
    double DynamicHeight() const { return dynamic_height_; }

   private:
    // v*, lv*
    double large_width_ = 0;
    double large_height_ = 0;
    // sv*
    double small_width_ = 0;
    double small_height_ = 0;
    // dv*
    double dynamic_width_ = 0;
    double dynamic_height_ = 0;
  };

  class CORE_EXPORT ContainerSizes {
    STACK_ALLOCATED();

   public:
    ContainerSizes() = default;
    explicit ContainerSizes(Element* nearest_container)
        : nearest_container_(nearest_container) {}

    absl::optional<double> Width() const;
    absl::optional<double> Height() const;

   private:
    void CacheSizeIfNeeded(PhysicalAxes, absl::optional<double>& cache) const;

    Element* nearest_container_{nullptr};
    mutable PhysicalAxes cached_physical_axes_{kPhysicalAxisNone};
    mutable absl::optional<double> cached_width_;
    mutable absl::optional<double> cached_height_;
  };

  CSSToLengthConversionData() : style_(nullptr), zoom_(1) {}
  CSSToLengthConversionData(const ComputedStyle*,
                            WritingMode,
                            const FontSizes&,
                            const ViewportSize&,
                            const ContainerSizes&,
                            float zoom);
  CSSToLengthConversionData(const ComputedStyle* curr_style,
                            const ComputedStyle* root_style,
                            const LayoutView*,
                            Element* nearest_container,
                            float zoom);

  float Zoom() const { return zoom_; }

  float EmFontSize() const;
  float RemFontSize() const;
  float ExFontSize() const;
  float ChFontSize() const;
  float FontSizeZoom() const { return font_sizes_.Zoom(); }

  // Accessing these marks the style as having viewport units
  double ViewportWidthPercent() const;
  double ViewportHeightPercent() const;
  double ViewportInlineSizePercent() const;
  double ViewportBlockSizePercent() const;
  double ViewportMinPercent() const;
  double ViewportMaxPercent() const;
  double SmallViewportWidthPercent() const;
  double SmallViewportHeightPercent() const;
  double SmallViewportInlineSizePercent() const;
  double SmallViewportBlockSizePercent() const;
  double SmallViewportMinPercent() const;
  double SmallViewportMaxPercent() const;
  double LargeViewportWidthPercent() const;
  double LargeViewportHeightPercent() const;
  double LargeViewportInlineSizePercent() const;
  double LargeViewportBlockSizePercent() const;
  double LargeViewportMinPercent() const;
  double LargeViewportMaxPercent() const;
  double DynamicViewportWidthPercent() const;
  double DynamicViewportHeightPercent() const;
  double DynamicViewportInlineSizePercent() const;
  double DynamicViewportBlockSizePercent() const;
  double DynamicViewportMinPercent() const;
  double DynamicViewportMaxPercent() const;

  // Accessing these marks the style as having container relative units.
  double ContainerWidthPercent() const;
  double ContainerHeightPercent() const;
  double ContainerInlineSizePercent() const;
  double ContainerBlockSizePercent() const;
  double ContainerMinPercent() const;
  double ContainerMaxPercent() const;

  void SetFontSizes(const FontSizes& font_sizes) { font_sizes_ = font_sizes; }
  void SetZoom(float zoom) {
    DCHECK(std::isfinite(zoom));
    DCHECK_GT(zoom, 0);
    zoom_ = zoom;
  }
  void SetContainerSizes(const ContainerSizes& container_sizes) {
    container_sizes_ = container_sizes;
  }

  CSSToLengthConversionData CopyWithAdjustedZoom(float new_zoom) const {
    return CSSToLengthConversionData(style_, writing_mode_, font_sizes_,
                                     viewport_size_, container_sizes_,
                                     new_zoom);
  }

  double ZoomedComputedPixels(double value, CSSPrimitiveValue::UnitType) const;

 private:
  bool IsHorizontalWritingMode() const {
    return blink::IsHorizontalWritingMode(writing_mode_);
  }

  const ComputedStyle* style_;
  WritingMode writing_mode_;
  FontSizes font_sizes_;
  ViewportSize viewport_size_;
  ContainerSizes container_sizes_;
  float zoom_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_
