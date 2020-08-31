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
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/scroll_offset.h"

#if defined(OS_MACOSX)
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
  constexpr explicit FloatPoint(const IntPoint& p) : x_(p.X()), y_(p.Y()) {}
  explicit FloatPoint(const SkPoint& p) : x_(p.x()), y_(p.y()) {}
  constexpr explicit FloatPoint(const FloatSize& s)
      : x_(s.Width()), y_(s.Height()) {}
  constexpr explicit FloatPoint(const IntSize& s)
      : x_(s.Width()), y_(s.Height()) {}
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

  constexpr float X() const { return x_; }
  constexpr float Y() const { return y_; }

  void SetX(float x) { x_ = x; }
  void SetY(float y) { y_ = y; }
  void Set(float x, float y) {
    x_ = x;
    y_ = y;
  }
  void Move(float dx, float dy) {
    x_ += dx;
    y_ += dy;
  }
  void Move(const IntSize& a) {
    x_ += a.Width();
    y_ += a.Height();
  }
  void Move(const FloatSize& a) {
    x_ += a.Width();
    y_ += a.Height();
  }
  void MoveBy(const IntPoint& a) {
    x_ += a.X();
    y_ += a.Y();
  }
  void MoveBy(const FloatPoint& a) {
    x_ += a.X();
    y_ += a.Y();
  }
  void Scale(float sx, float sy) {
    x_ *= sx;
    y_ *= sy;
  }

  float Dot(const FloatPoint& a) const { return x_ * a.X() + y_ * a.Y(); }

  float SlopeAngleRadians() const;
  float length() const;
  float LengthSquared() const { return x_ * x_ + y_ * y_; }

  FloatPoint ExpandedTo(const FloatPoint& other) const;
  FloatPoint ShrunkTo(const FloatPoint& other) const;

  FloatPoint TransposedPoint() const { return FloatPoint(y_, x_); }

  FloatPoint ScaledBy(float scale) const {
    return FloatPoint(x_ * scale, y_ * scale);
  }

#if defined(OS_MACOSX)
  FloatPoint(const CGPoint&);
  operator CGPoint() const;
#endif

  constexpr operator gfx::PointF() const { return gfx::PointF(x_, y_); }
  constexpr explicit operator gfx::Vector2dF() const {
    return gfx::Vector2dF(x_, y_);
  }
  explicit operator SkPoint() const { return SkPoint::Make(x_, y_); }
  explicit operator gfx::ScrollOffset() const {
    return gfx::ScrollOffset(x_, y_);
  }
  operator gfx::Point3F() const { return gfx::Point3F(x_, y_, 0.f); }

  String ToString() const;

 private:
  float x_, y_;

  friend struct ::WTF::DefaultHash<blink::FloatSize>;
  friend struct ::WTF::HashTraits<blink::FloatSize>;
};

inline FloatPoint& operator+=(FloatPoint& a, const FloatSize& b) {
  a.Move(b.Width(), b.Height());
  return a;
}

inline FloatPoint& operator+=(FloatPoint& a, const FloatPoint& b) {
  a.Move(b.X(), b.Y());
  return a;
}

inline FloatPoint& operator-=(FloatPoint& a, const FloatSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

constexpr FloatPoint operator+(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

constexpr FloatPoint operator+(const FloatPoint& a, const IntSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

constexpr FloatPoint operator+(const IntPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

constexpr FloatPoint operator+(const FloatPoint& a, const FloatPoint& b) {
  return FloatPoint(a.X() + b.X(), a.Y() + b.Y());
}

constexpr FloatPoint operator+(const FloatPoint& a, const IntPoint& b) {
  return FloatPoint(a.X() + b.X(), a.Y() + b.Y());
}

constexpr FloatSize operator-(const FloatPoint& a, const FloatPoint& b) {
  return FloatSize(a.X() - b.X(), a.Y() - b.Y());
}

constexpr FloatSize operator-(const FloatPoint& a, const IntPoint& b) {
  return FloatSize(a.X() - b.X(), a.Y() - b.Y());
}

constexpr FloatPoint operator-(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() - b.Width(), a.Y() - b.Height());
}

constexpr FloatPoint operator-(const FloatPoint& a) {
  return FloatPoint(-a.X(), -a.Y());
}

constexpr bool operator==(const FloatPoint& a, const FloatPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

constexpr bool operator!=(const FloatPoint& a, const FloatPoint& b) {
  return !(a == b);
}

inline float operator*(const FloatPoint& a, const FloatPoint& b) {
  // dot product
  return a.Dot(b);
}

inline IntPoint RoundedIntPoint(const FloatPoint& p) {
  return IntPoint(clampTo<int>(roundf(p.X())), clampTo<int>(roundf(p.Y())));
}

inline IntSize RoundedIntSize(const FloatPoint& p) {
  return IntSize(clampTo<int>(roundf(p.X())), clampTo<int>(roundf(p.Y())));
}

inline IntPoint FlooredIntPoint(const FloatPoint& p) {
  return IntPoint(clampTo<int>(floorf(p.X())), clampTo<int>(floorf(p.Y())));
}

inline IntPoint FlooredIntPoint(const gfx::PointF& p) {
  return IntPoint(clampTo<int>(floorf(p.x())), clampTo<int>(floorf(p.y())));
}

inline IntPoint CeiledIntPoint(const FloatPoint& p) {
  return IntPoint(clampTo<int>(ceilf(p.X())), clampTo<int>(ceilf(p.Y())));
}

inline IntSize FlooredIntSize(const FloatPoint& p) {
  return IntSize(clampTo<int>(floorf(p.X())), clampTo<int>(floorf(p.Y())));
}

inline FloatSize ToFloatSize(const FloatPoint& a) {
  return FloatSize(a.X(), a.Y());
}

// Find point where lines through the two pairs of points intersect.
// Returns false if the lines don't intersect.
PLATFORM_EXPORT bool FindIntersection(const FloatPoint& p1,
                                      const FloatPoint& p2,
                                      const FloatPoint& d1,
                                      const FloatPoint& d2,
                                      FloatPoint& intersection);

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
      return HashInts(bit_cast<Bits>(key.X()), bit_cast<Bits>(key.Y()));
    }
    static bool Equal(const blink::FloatPoint& a, const blink::FloatPoint& b) {
      return bit_cast<Bits>(a.X()) == bit_cast<Bits>(b.X()) &&
             bit_cast<Bits>(a.Y()) == bit_cast<Bits>(b.Y());
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
