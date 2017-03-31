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
                   GradientSpreadMethod spreadMethod,
                   ColorInterpolation interpolation)
    : m_type(type),
      m_spreadMethod(spreadMethod),
      m_colorInterpolation(interpolation),
      m_stopsSorted(true) {}

Gradient::~Gradient() {}

static inline bool compareStops(const Gradient::ColorStop& a,
                                const Gradient::ColorStop& b) {
  return a.stop < b.stop;
}

void Gradient::addColorStop(const Gradient::ColorStop& stop) {
  if (m_stops.isEmpty()) {
    m_stopsSorted = true;
  } else {
    m_stopsSorted = m_stopsSorted && compareStops(m_stops.back(), stop);
  }

  m_stops.push_back(stop);
  m_cachedShader.reset();
}

void Gradient::addColorStops(const Vector<Gradient::ColorStop>& stops) {
  for (const auto& stop : stops)
    addColorStop(stop);
}

void Gradient::sortStopsIfNecessary() {
  if (m_stopsSorted)
    return;

  m_stopsSorted = true;

  if (!m_stops.size())
    return;

  std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
}

// FIXME: This would be more at home as Color::operator SkColor.
static inline SkColor makeSkColor(const Color& c) {
  return SkColorSetARGB(c.alpha(), c.red(), c.green(), c.blue());
}

// Collect sorted stop position and color information into the pos and colors
// buffers, ensuring stops at both 0.0 and 1.0.
// TODO(fmalita): theoretically Skia should provide the same 0.0/1.0 padding
// (making this logic redundant), but in practice there are rendering diffs;
// investigate.
void Gradient::fillSkiaStops(ColorBuffer& colors, OffsetBuffer& pos) const {
  if (m_stops.isEmpty()) {
    // A gradient with no stops must be transparent black.
    pos.push_back(WebCoreFloatToSkScalar(0));
    colors.push_back(SK_ColorTRANSPARENT);
  } else if (m_stops.front().stop > 0) {
    // Copy the first stop to 0.0. The first stop position may have a slight
    // rounding error, but we don't care in this float comparison, since
    // 0.0 comes through cleanly and people aren't likely to want a gradient
    // with a stop at (0 + epsilon).
    pos.push_back(WebCoreFloatToSkScalar(0));
    colors.push_back(makeSkColor(m_stops.front().color));
  }

  for (const auto& stop : m_stops) {
    pos.push_back(WebCoreFloatToSkScalar(stop.stop));
    colors.push_back(makeSkColor(stop.color));
  }

  // Copy the last stop to 1.0 if needed. See comment above about this float
  // comparison.
  DCHECK(!pos.isEmpty());
  if (pos.back() < 1) {
    pos.push_back(WebCoreFloatToSkScalar(1));
    colors.push_back(colors.back());
  }
}

sk_sp<PaintShader> Gradient::createShaderInternal(const SkMatrix& localMatrix) {
  sortStopsIfNecessary();
  DCHECK(m_stopsSorted);

  ColorBuffer colors;
  colors.reserveCapacity(m_stops.size());
  OffsetBuffer pos;
  pos.reserveCapacity(m_stops.size());

  fillSkiaStops(colors, pos);
  DCHECK_GE(colors.size(), 2ul);
  DCHECK_EQ(pos.size(), colors.size());

  SkShader::TileMode tile = SkShader::kClamp_TileMode;
  switch (m_spreadMethod) {
    case SpreadMethodReflect:
      tile = SkShader::kMirror_TileMode;
      break;
    case SpreadMethodRepeat:
      tile = SkShader::kRepeat_TileMode;
      break;
    case SpreadMethodPad:
      tile = SkShader::kClamp_TileMode;
      break;
  }

  uint32_t flags = m_colorInterpolation == ColorInterpolation::Premultiplied
                       ? SkGradientShader::kInterpolateColorsInPremul_Flag
                       : 0;
  sk_sp<SkShader> shader = createShader(colors, pos, tile, flags, localMatrix);
  if (!shader) {
    // use last color, since our "geometry" was degenerate (e.g. radius==0)
    shader = SkShader::MakeColorShader(colors.back());
  }

  return WrapSkShader(std::move(shader));
}

void Gradient::applyToFlags(PaintFlags& flags, const SkMatrix& localMatrix) {
  if (!m_cachedShader || localMatrix != m_cachedShader->getLocalMatrix())
    m_cachedShader = createShaderInternal(localMatrix);

  flags.setShader(m_cachedShader);

  // Legacy behavior: gradients are always dithered.
  flags.setDither(true);
}

namespace {

class LinearGradient final : public Gradient {
 public:
  LinearGradient(const FloatPoint& p0,
                 const FloatPoint& p1,
                 GradientSpreadMethod spreadMethod,
                 ColorInterpolation interpolation)
      : Gradient(Type::Linear, spreadMethod, interpolation),
        m_p0(p0),
        m_p1(p1) {}

 protected:
  sk_sp<SkShader> createShader(const ColorBuffer& colors,
                               const OffsetBuffer& pos,
                               SkShader::TileMode tileMode,
                               uint32_t flags,
                               const SkMatrix& localMatrix) const override {
    SkPoint pts[2] = {m_p0.data(), m_p1.data()};
    return SkGradientShader::MakeLinear(pts, colors.data(), pos.data(),
                                        static_cast<int>(colors.size()),
                                        tileMode, flags, &localMatrix);
  }

 private:
  FloatPoint m_p0;
  FloatPoint m_p1;
};

class RadialGradient final : public Gradient {
 public:
  RadialGradient(const FloatPoint& p0,
                 float r0,
                 const FloatPoint& p1,
                 float r1,
                 float aspectRatio,
                 GradientSpreadMethod spreadMethod,
                 ColorInterpolation interpolation)
      : Gradient(Type::Radial, spreadMethod, interpolation),
        m_p0(p0),
        m_p1(p1),
        m_r0(r0),
        m_r1(r1),
        m_aspectRatio(aspectRatio) {}

 protected:
  sk_sp<SkShader> createShader(const ColorBuffer& colors,
                               const OffsetBuffer& pos,
                               SkShader::TileMode tileMode,
                               uint32_t flags,
                               const SkMatrix& localMatrix) const override {
    SkTCopyOnFirstWrite<SkMatrix> adjustedLocalMatrix(localMatrix);
    if (m_aspectRatio != 1) {
      // CSS3 elliptical gradients: apply the elliptical scaling at the
      // gradient center point.
      DCHECK(m_p0 == m_p1);
      adjustedLocalMatrix.writable()->preScale(1, 1 / m_aspectRatio, m_p0.x(),
                                               m_p0.y());
    }

    // Since the two-point radial gradient is slower than the plain radial,
    // only use it if we have to.
    if (m_p0 == m_p1 && m_r0 <= 0.0f) {
      return SkGradientShader::MakeRadial(m_p1.data(), m_r1, colors.data(),
                                          pos.data(),
                                          static_cast<int>(colors.size()),
                                          tileMode, flags, adjustedLocalMatrix);
    }

    // The radii we give to Skia must be positive. If we're given a
    // negative radius, ask for zero instead.
    const SkScalar radius0 = std::max(WebCoreFloatToSkScalar(m_r0), 0.0f);
    const SkScalar radius1 = std::max(WebCoreFloatToSkScalar(m_r1), 0.0f);
    return SkGradientShader::MakeTwoPointConical(
        m_p0.data(), radius0, m_p1.data(), radius1, colors.data(), pos.data(),
        static_cast<int>(colors.size()), tileMode, flags, adjustedLocalMatrix);
  }

 private:
  FloatPoint m_p0;
  FloatPoint m_p1;
  float m_r0;
  float m_r1;
  float m_aspectRatio;  // For elliptical gradient, width / height.
};

}  // anonymous ns

PassRefPtr<Gradient> Gradient::createLinear(const FloatPoint& p0,
                                            const FloatPoint& p1,
                                            GradientSpreadMethod spreadMethod,
                                            ColorInterpolation interpolation) {
  return adoptRef(new LinearGradient(p0, p1, spreadMethod, interpolation));
}

PassRefPtr<Gradient> Gradient::createRadial(const FloatPoint& p0,
                                            float r0,
                                            const FloatPoint& p1,
                                            float r1,
                                            float aspectRatio,
                                            GradientSpreadMethod spreadMethod,
                                            ColorInterpolation interpolation) {
  return adoptRef(new RadialGradient(p0, r0, p1, r1, aspectRatio, spreadMethod,
                                     interpolation));
}

}  // namespace blink
