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

#ifndef FloatPoint_h
#define FloatPoint_h

#include <algorithm>
#include <iosfwd>

#include "build/build_config.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/MathExtras.h"
#include "third_party/skia/include/core/SkPoint.h"

#if defined(OS_MACOSX)
typedef struct CGPoint CGPoint;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class DoublePoint;
class IntPoint;
class IntSize;
class LayoutPoint;
class LayoutSize;

class PLATFORM_EXPORT FloatPoint {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FloatPoint() : x_(0), y_(0) {}
  FloatPoint(float x, float y) : x_(x), y_(y) {}
  FloatPoint(const IntPoint&);
  explicit FloatPoint(const SkPoint& point) : x_(point.x()), y_(point.y()) {}
  explicit FloatPoint(const DoublePoint&);
  explicit FloatPoint(const LayoutPoint&);
  explicit FloatPoint(const FloatSize& size)
      : x_(size.Width()), y_(size.Height()) {}
  explicit FloatPoint(const LayoutSize&);
  explicit FloatPoint(const IntSize& size)
      : x_(size.Width()), y_(size.Height()) {}

  static FloatPoint Zero() { return FloatPoint(); }

  static FloatPoint NarrowPrecision(double x, double y);

  float X() const { return x_; }
  float Y() const { return y_; }

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
  void Move(const LayoutSize&);
  void Move(const FloatSize& a) {
    x_ += a.Width();
    y_ += a.Height();
  }
  void MoveBy(const IntPoint& a) {
    x_ += a.X();
    y_ += a.Y();
  }
  void MoveBy(const LayoutPoint&);
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

  FloatPoint ExpandedTo(const FloatPoint& other) const {
    return FloatPoint(std::max(x_, other.x_), std::max(y_, other.y_));
  }

  FloatPoint ShrunkTo(const FloatPoint& other) const {
    return FloatPoint(std::min(x_, other.x_), std::min(y_, other.y_));
  }

  FloatPoint TransposedPoint() const { return FloatPoint(y_, x_); }

  FloatPoint ScaledBy(float scale) const {
    return FloatPoint(x_ * scale, y_ * scale);
  }

#if defined(OS_MACOSX)
  FloatPoint(const CGPoint&);
  operator CGPoint() const;
#if defined(__OBJC__) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
  FloatPoint(const NSPoint&);
  operator NSPoint() const;
#endif
#endif

  // Can we remove this one?
  SkPoint Data() const;

  operator SkPoint() const { return SkPoint::Make(x_, y_); }

  String ToString() const;

 private:
  float x_, y_;
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

inline FloatPoint operator+(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

inline FloatPoint operator+(const FloatPoint& a, const IntSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

inline FloatPoint operator+(const IntPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

inline FloatPoint operator+(const FloatPoint& a, const FloatPoint& b) {
  return FloatPoint(a.X() + b.X(), a.Y() + b.Y());
}

inline FloatPoint operator+(const FloatPoint& a, const IntPoint& b) {
  return FloatPoint(a.X() + b.X(), a.Y() + b.Y());
}

inline FloatSize operator-(const FloatPoint& a, const FloatPoint& b) {
  return FloatSize(a.X() - b.X(), a.Y() - b.Y());
}

inline FloatSize operator-(const FloatPoint& a, const IntPoint& b) {
  return FloatSize(a.X() - b.X(), a.Y() - b.Y());
}

inline FloatPoint operator-(const FloatPoint& a, const FloatSize& b) {
  return FloatPoint(a.X() - b.Width(), a.Y() - b.Height());
}

inline FloatPoint operator-(const FloatPoint& a) {
  return FloatPoint(-a.X(), -a.Y());
}

inline bool operator==(const FloatPoint& a, const FloatPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

inline bool operator!=(const FloatPoint& a, const FloatPoint& b) {
  return a.X() != b.X() || a.Y() != b.Y();
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

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const FloatPoint&, std::ostream*);

}  // namespace blink

#endif
