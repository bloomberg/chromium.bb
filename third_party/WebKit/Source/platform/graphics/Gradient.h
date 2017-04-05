/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Torch Mobile, Inc.
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

#ifndef Gradient_h
#define Gradient_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintShader.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

class SkMatrix;
class SkShader;

namespace blink {

class FloatPoint;

class PLATFORM_EXPORT Gradient : public RefCounted<Gradient> {
  WTF_MAKE_NONCOPYABLE(Gradient);

 public:
  enum class Type { Linear, Radial, Conic };

  enum class ColorInterpolation {
    Premultiplied,
    Unpremultiplied,
  };

  static PassRefPtr<Gradient> createLinear(
      const FloatPoint& p0,
      const FloatPoint& p1,
      GradientSpreadMethod = SpreadMethodPad,
      ColorInterpolation = ColorInterpolation::Unpremultiplied);

  static PassRefPtr<Gradient> createRadial(
      const FloatPoint& p0,
      float r0,
      const FloatPoint& p1,
      float r1,
      float aspectRatio = 1,
      GradientSpreadMethod = SpreadMethodPad,
      ColorInterpolation = ColorInterpolation::Unpremultiplied);

  static PassRefPtr<Gradient> createConic(
      const FloatPoint& position,
      float angle,
      ColorInterpolation = ColorInterpolation::Unpremultiplied);

  virtual ~Gradient();

  Type getType() const { return m_type; }

  struct ColorStop {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    float stop;
    Color color;

    ColorStop(float s, const Color& c) : stop(s), color(c) {}
  };
  void addColorStop(const ColorStop&);
  void addColorStop(float value, const Color& color) {
    addColorStop(ColorStop(value, color));
  }
  void addColorStops(const Vector<Gradient::ColorStop>&);

  void applyToFlags(PaintFlags&, const SkMatrix& localMatrix);

 protected:
  Gradient(Type, GradientSpreadMethod, ColorInterpolation);

  using ColorBuffer = Vector<SkColor, 8>;
  using OffsetBuffer = Vector<SkScalar, 8>;
  virtual sk_sp<SkShader> createShader(const ColorBuffer&,
                                       const OffsetBuffer&,
                                       SkShader::TileMode,
                                       uint32_t flags,
                                       const SkMatrix&) const = 0;

 private:
  sk_sp<PaintShader> createShaderInternal(const SkMatrix& localMatrix);

  void sortStopsIfNecessary();
  void fillSkiaStops(ColorBuffer&, OffsetBuffer&) const;

  const Type m_type;
  const GradientSpreadMethod m_spreadMethod;
  const ColorInterpolation m_colorInterpolation;

  Vector<ColorStop, 2> m_stops;
  bool m_stopsSorted;

  mutable sk_sp<PaintShader> m_cachedShader;
};

}  // namespace blink

#endif
