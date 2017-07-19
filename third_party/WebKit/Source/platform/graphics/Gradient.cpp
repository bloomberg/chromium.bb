/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/Gradient.h"

#include <algorithm>
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintShader.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkTLazy.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace blink {

Gradient::Gradient(Type type,
                   GradientSpreadMethod spread_method,
                   ColorInterpolation interpolation)
    : type_(type),
      spread_method_(spread_method),
      color_interpolation_(interpolation),
      stops_sorted_(true) {}

Gradient::~Gradient() {}

static inline bool CompareStops(const Gradient::ColorStop& a,
                                const Gradient::ColorStop& b) {
  return a.stop < b.stop;
}

void Gradient::AddColorStop(const Gradient::ColorStop& stop) {
  if (stops_.IsEmpty()) {
    stops_sorted_ = true;
  } else {
    stops_sorted_ = stops_sorted_ && CompareStops(stops_.back(), stop);
  }

  stops_.push_back(stop);
  cached_shader_.reset();
}

void Gradient::AddColorStops(const Vector<Gradient::ColorStop>& stops) {
  for (const auto& stop : stops)
    AddColorStop(stop);
}

void Gradient::SortStopsIfNecessary() {
  if (stops_sorted_)
    return;

  stops_sorted_ = true;

  if (!stops_.size())
    return;

  std::stable_sort(stops_.begin(), stops_.end(), CompareStops);
}

// FIXME: This would be more at home as Color::operator SkColor.
static inline SkColor MakeSkColor(const Color& c) {
  return SkColorSetARGB(c.Alpha(), c.Red(), c.Green(), c.Blue());
}

// Collect sorted stop position and color information into the pos and colors
// buffers, ensuring stops at both 0.0 and 1.0.
// TODO(fmalita): theoretically Skia should provide the same 0.0/1.0 padding
// (making this logic redundant), but in practice there are rendering diffs;
// investigate.
void Gradient::FillSkiaStops(ColorBuffer& colors, OffsetBuffer& pos) const {
  if (stops_.IsEmpty()) {
    // A gradient with no stops must be transparent black.
    pos.push_back(WebCoreFloatToSkScalar(0));
    colors.push_back(SK_ColorTRANSPARENT);
  } else if (stops_.front().stop > 0) {
    // Copy the first stop to 0.0. The first stop position may have a slight
    // rounding error, but we don't care in this float comparison, since
    // 0.0 comes through cleanly and people aren't likely to want a gradient
    // with a stop at (0 + epsilon).
    pos.push_back(WebCoreFloatToSkScalar(0));
    if (color_filter_) {
      colors.push_back(
          color_filter_->filterColor(MakeSkColor(stops_.front().color)));
    } else {
      colors.push_back(MakeSkColor(stops_.front().color));
    }
  }

  for (const auto& stop : stops_) {
    pos.push_back(WebCoreFloatToSkScalar(stop.stop));
    if (color_filter_)
      colors.push_back(color_filter_->filterColor(MakeSkColor(stop.color)));
    else
      colors.push_back(MakeSkColor(stop.color));
  }

  // Copy the last stop to 1.0 if needed. See comment above about this float
  // comparison.
  DCHECK(!pos.IsEmpty());
  if (pos.back() < 1) {
    pos.push_back(WebCoreFloatToSkScalar(1));
    colors.push_back(colors.back());
  }
}

sk_sp<PaintShader> Gradient::CreateShaderInternal(
    const SkMatrix& local_matrix) {
  SortStopsIfNecessary();
  DCHECK(stops_sorted_);

  ColorBuffer colors;
  colors.ReserveCapacity(stops_.size());
  OffsetBuffer pos;
  pos.ReserveCapacity(stops_.size());

  FillSkiaStops(colors, pos);
  DCHECK_GE(colors.size(), 2ul);
  DCHECK_EQ(pos.size(), colors.size());

  SkShader::TileMode tile = SkShader::kClamp_TileMode;
  switch (spread_method_) {
    case kSpreadMethodReflect:
      tile = SkShader::kMirror_TileMode;
      break;
    case kSpreadMethodRepeat:
      tile = SkShader::kRepeat_TileMode;
      break;
    case kSpreadMethodPad:
      tile = SkShader::kClamp_TileMode;
      break;
  }

  uint32_t flags = color_interpolation_ == ColorInterpolation::kPremultiplied
                       ? SkGradientShader::kInterpolateColorsInPremul_Flag
                       : 0;
  sk_sp<PaintShader> shader =
      CreateShader(colors, pos, tile, flags, local_matrix, colors.back());
  DCHECK(shader);

  return shader;
}

void Gradient::ApplyToFlags(PaintFlags& flags, const SkMatrix& local_matrix) {
  if (!cached_shader_ || local_matrix != cached_shader_->GetLocalMatrix() ||
      flags.getColorFilter().get() != color_filter_.get()) {
    color_filter_ = flags.getColorFilter();
    flags.setColorFilter(nullptr);
    cached_shader_ = CreateShaderInternal(local_matrix);
  }

  flags.setShader(cached_shader_);

  // Legacy behavior: gradients are always dithered.
  flags.setDither(true);
}

namespace {

class LinearGradient final : public Gradient {
 public:
  LinearGradient(const FloatPoint& p0,
                 const FloatPoint& p1,
                 GradientSpreadMethod spread_method,
                 ColorInterpolation interpolation)
      : Gradient(Type::kLinear, spread_method, interpolation),
        p0_(p0),
        p1_(p1) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkShader::TileMode tile_mode,
                                  uint32_t flags,
                                  const SkMatrix& local_matrix,
                                  SkColor fallback_color) const override {
    SkPoint pts[2] = {p0_.Data(), p1_.Data()};
    return PaintShader::MakeLinearGradient(
        pts, colors.data(), pos.data(), static_cast<int>(colors.size()),
        tile_mode, flags, &local_matrix, fallback_color);
  }

 private:
  const FloatPoint p0_;
  const FloatPoint p1_;
};

class RadialGradient final : public Gradient {
 public:
  RadialGradient(const FloatPoint& p0,
                 float r0,
                 const FloatPoint& p1,
                 float r1,
                 float aspect_ratio,
                 GradientSpreadMethod spread_method,
                 ColorInterpolation interpolation)
      : Gradient(Type::kRadial, spread_method, interpolation),
        p0_(p0),
        p1_(p1),
        r0_(r0),
        r1_(r1),
        aspect_ratio_(aspect_ratio) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkShader::TileMode tile_mode,
                                  uint32_t flags,
                                  const SkMatrix& local_matrix,
                                  SkColor fallback_color) const override {
    SkTCopyOnFirstWrite<SkMatrix> adjusted_local_matrix(local_matrix);
    if (aspect_ratio_ != 1) {
      // CSS3 elliptical gradients: apply the elliptical scaling at the
      // gradient center point.
      DCHECK(p0_ == p1_);
      adjusted_local_matrix.writable()->preScale(1, 1 / aspect_ratio_, p0_.X(),
                                                 p0_.Y());
    }

    // The radii we give to Skia must be positive. If we're given a
    // negative radius, ask for zero instead.
    const SkScalar radius0 = std::max(WebCoreFloatToSkScalar(r0_), 0.0f);
    const SkScalar radius1 = std::max(WebCoreFloatToSkScalar(r1_), 0.0f);
    return PaintShader::MakeTwoPointConicalGradient(
        p0_.Data(), radius0, p1_.Data(), radius1, colors.data(), pos.data(),
        static_cast<int>(colors.size()), tile_mode, flags,
        adjusted_local_matrix, fallback_color);
  }

 private:
  const FloatPoint p0_;
  const FloatPoint p1_;
  const float r0_;
  const float r1_;
  const float aspect_ratio_;  // For elliptical gradient, width / height.
};

class ConicGradient final : public Gradient {
 public:
  ConicGradient(const FloatPoint& position,
                float angle,
                ColorInterpolation interpolation)
      : Gradient(Type::kConic, kSpreadMethodPad, interpolation),
        position_(position),
        angle_(angle) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkShader::TileMode tile_mode,
                                  uint32_t flags,
                                  const SkMatrix& local_matrix,
                                  SkColor fallback_color) const override {
    DCHECK_NE(tile_mode, SkShader::kMirror_TileMode);

    // Skia's sweep gradient angles are relative to the x-axis, not the y-axis.
    const float skia_angle = angle_ - 90;
    SkTCopyOnFirstWrite<SkMatrix> adjusted_local_matrix(local_matrix);
    if (skia_angle) {
      adjusted_local_matrix.writable()->preRotate(skia_angle, position_.X(),
                                                  position_.Y());
    }

    return PaintShader::MakeSweepGradient(
        position_.X(), position_.Y(), colors.data(), pos.data(),
        static_cast<int>(colors.size()), flags, adjusted_local_matrix,
        fallback_color);
  }

 private:
  const FloatPoint position_;
  const float angle_;
};

}  // anonymous ns

PassRefPtr<Gradient> Gradient::CreateLinear(const FloatPoint& p0,
                                            const FloatPoint& p1,
                                            GradientSpreadMethod spread_method,
                                            ColorInterpolation interpolation) {
  return AdoptRef(new LinearGradient(p0, p1, spread_method, interpolation));
}

PassRefPtr<Gradient> Gradient::CreateRadial(const FloatPoint& p0,
                                            float r0,
                                            const FloatPoint& p1,
                                            float r1,
                                            float aspect_ratio,
                                            GradientSpreadMethod spread_method,
                                            ColorInterpolation interpolation) {
  return AdoptRef(new RadialGradient(p0, r0, p1, r1, aspect_ratio,
                                     spread_method, interpolation));
}

PassRefPtr<Gradient> Gradient::CreateConic(const FloatPoint& position,
                                           float angle,
                                           ColorInterpolation interpolation) {
  return AdoptRef(new ConicGradient(position, angle, interpolation));
}

}  // namespace blink
