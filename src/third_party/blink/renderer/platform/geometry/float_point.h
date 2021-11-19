/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_H_

#include <iosfwd>
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(OS_MAC)
typedef struct CGPoint CGPoint;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT FloatPoint {
  DISALLOW_NEW();

 public:
  constexpr FloatPoint() : x_(0), y_(0) {}
  constexpr FloatPoint(float x, float y) : x_(x), y_(y) {}
  constexpr explicit FloatPoint(const gfx::Point& p) : x_(p.x()), y_(p.y()) {}
  explicit FloatPoint(const SkPoint& p) : x_(p.x()), y_(p.y()) {}
  constexpr explicit FloatPoint(const FloatSize& s)
      : x_(s.width()), y_(s.height()) {}
  constexpr explicit FloatPoint(const IntSize& s)
      : x_(s.width()), y_(s.height()) {}
  constexpr explicit FloatPoint(const gfx::PointF& p) : x_(p.x()), y_(p.y()) {}
  constexpr explicit FloatPoint(const gfx::Vector2dF& v)
      : x_(v.x()), y_(v.y()) {}
  // We also have conversion operators to FloatPoint defined LayoutPoint,
  // LayoutSize and DoublePoint.

  static constexpr FloatPoint Zero() { return FloatPoint(); }

  static FloatPoint NarrowPrecision(double x, double y);

  bool IsValid() const {
    return x_ != -std::numeric_limits<float>::infinity() &&
           y_ != -std::numeric_limits<float>::infinity();
  }

  constexpr float x() const { return x_; }
  constexpr float y() const { return y_; }

  void set_x(float x) { x_ = x; }
  void set_y(float y) { y_ = y; }
  void SetPoint(float x, float y) {
    x_ = x;
    y_ = y;
  }
  void Offset(float dx, float dy) {
    x_ += dx;
    y_ += dy;
  }
  void Offset(const IntSize& a) {
    x_ += a.width();
    y_ += a.height();
  }
  void Offset(const FloatSize& a) {
    x_ += a.width();
    y_ += a.height();
  }
  void MoveBy(const gfx::Point& a) {
    x_ += a.x();
    y_ += a.y();
  }
  void MoveBy(const FloatPoint& a) {
    x_ += a.x();
    y_ += a.y();
  }
  void Scale(float sx, float sy) {
    x_ *= sx;
    y_ *= sy;
  }

  float Dot(const FloatPoint& a) const { return x_ * a.x() + y_ * a.y(); }

  float SlopeAngleRadians() const;
  float length() const;
  float LengthSquared() const { return x_ * x_ + y_ * y_; }

  FloatPoint ExpandedTo(const FloatPoint& other) const;
  FloatPoint ShrunkTo(const FloatPoint& other) const;

  FloatPoint TransposedPoint() const { return FloatPoint(y_, x_); }

  FloatPoint ScaledBy(float scale) const {
    return FloatPoint(x_ * scale, y_ * scale);
  }

#if defined(OS_MAC)
  FloatPoint(const CGPoint&);
  operator CGPoint() const;
#endif

  explicit operator SkPoint() const { return SkPoint::Make(x_, y_); }

  // These are deleted during blink geometry type to gfx migration.
  // Use ToGfxPointF(), ToGfxPoint3F() and ToGfxVector2dF() instead.
  operator gfx::PointF() const = delete;
  operator gfx::Point3F() const = delete;
  operator gfx::Vector2dF() const = delete;

  String ToString() const;

 private:
  float x_, y_;

  friend struct ::WTF::DefaultHash<blink::FloatSize>;
  friend struct ::WTF::HashTraits<blink::FloatSize>;
};

inline FloatPoint& operator+=(FloatPoint& a, const FloatSize& b) {
  a.Offset(b.width(), b.height());
  return a;
}

inline FloatPoint& operator+=(FloatPoint& a, const FloatPoint& b) {
  a.Offset(b.x(), b.y());
  return a;
}

inline FloatPoint& operator-=(FloatPoint& a, const FloatSize& b) {
  a.Offset(-b.width(), -b.height());
  return a;
}

constexpr FloatPoint operator+(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.x() + b.width(), a.y() + b.height());
}

constexpr FloatPoint operator+(const FloatPoint& a, const IntSize& b) {
  return FloatPoint(a.x() + b.width(), a.y() + b.height());
}

constexpr FloatPoint operator+(const gfx::Point& a, const FloatSize& b) {
  return FloatPoint(a.x() + b.width(), a.y() + b.height());
}

constexpr FloatPoint operator+(const FloatPoint& a, const FloatPoint& b) {
  return FloatPoint(a.x() + b.x(), a.y() + b.y());
}

constexpr FloatPoint operator+(const FloatPoint& a, const gfx::Point& b) {
  return FloatPoint(a.x() + b.x(), a.y() + b.y());
}

constexpr FloatSize operator-(const FloatPoint& a, const FloatPoint& b) {
  return FloatSize(a.x() - b.x(), a.y() - b.y());
}

constexpr FloatSize operator-(const FloatPoint& a, const gfx::Point& b) {
  return FloatSize(a.x() - b.x(), a.y() - b.y());
}

constexpr FloatPoint operator-(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.x() - b.width(), a.y() - b.height());
}

constexpr FloatPoint operator-(const FloatPoint& a) {
  return FloatPoint(-a.x(), -a.y());
}

constexpr bool operator==(const FloatPoint& a, const FloatPoint& b) {
  return a.x() == b.x() && a.y() == b.y();
}

constexpr bool operator!=(const FloatPoint& a, const FloatPoint& b) {
  return !(a == b);
}

inline float operator*(const FloatPoint& a, const FloatPoint& b) {
  // dot product
  return a.Dot(b);
}

inline gfx::Point RoundedIntPoint(const FloatPoint& p) {
  return gfx::Point(ClampTo<int>(roundf(p.x())), ClampTo<int>(roundf(p.y())));
}

inline IntSize RoundedIntSize(const FloatPoint& p) {
  return IntSize(ClampTo<int>(roundf(p.x())), ClampTo<int>(roundf(p.y())));
}

inline gfx::Point FlooredIntPoint(const FloatPoint& p) {
  return gfx::Point(ClampTo<int>(floorf(p.x())), ClampTo<int>(floorf(p.y())));
}

inline gfx::Point FlooredIntPoint(const gfx::PointF& p) {
  return gfx::Point(ClampTo<int>(floorf(p.x())), ClampTo<int>(floorf(p.y())));
}

inline gfx::Point CeiledIntPoint(const FloatPoint& p) {
  return gfx::Point(ClampTo<int>(ceilf(p.x())), ClampTo<int>(ceilf(p.y())));
}

inline IntSize FlooredIntSize(const FloatPoint& p) {
  return IntSize(ClampTo<int>(floorf(p.x())), ClampTo<int>(floorf(p.y())));
}

inline FloatSize ToFloatSize(const FloatPoint& a) {
  return FloatSize(a.x(), a.y());
}

// Find point where lines through the two pairs of points intersect.
// Returns false if the lines don't intersect.
PLATFORM_EXPORT bool FindIntersection(const FloatPoint& p1,
                                      const FloatPoint& p2,
                                      const FloatPoint& d1,
                                      const FloatPoint& d2,
                                      FloatPoint& intersection);

constexpr gfx::PointF ToGfxPointF(const FloatPoint& p) {
  return gfx::PointF(p.x(), p.y());
}

constexpr gfx::Point3F ToGfxPoint3F(const FloatPoint& p) {
  return gfx::Point3F(p.x(), p.y(), 0.f);
}

constexpr gfx::Vector2dF ToGfxVector2dF(const FloatPoint& p) {
  return gfx::Vector2dF(p.x(), p.y());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatPoint&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&,
                                            const FloatPoint&);

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::FloatPoint> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    typedef typename IntTypes<sizeof(float)>::UnsignedType Bits;
    static unsigned GetHash(const blink::FloatPoint& key) {
      return HashInts(bit_cast<Bits>(key.x()), bit_cast<Bits>(key.y()));
    }
    static bool Equal(const blink::FloatPoint& a, const blink::FloatPoint& b) {
      return bit_cast<Bits>(a.x()) == bit_cast<Bits>(b.x()) &&
             bit_cast<Bits>(a.y()) == bit_cast<Bits>(b.y());
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<blink::FloatPoint> : GenericHashTraits<blink::FloatPoint> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = false;
  static blink::FloatPoint EmptyValue() {
    return blink::FloatPoint(std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::infinity());
  }
  static void ConstructDeletedValue(blink::FloatPoint& slot, bool) {
    slot = blink::FloatPoint(-std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity());
  }
  static bool IsDeletedValue(const blink::FloatPoint& value) {
    return !value.IsValid();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_POINT_H_
