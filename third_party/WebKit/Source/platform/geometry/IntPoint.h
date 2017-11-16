/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef IntPoint_h
#define IntPoint_h

#include "build/build_config.h"
#include "platform/geometry/IntSize.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/SaturatedArithmetic.h"
#include "platform/wtf/VectorTraits.h"

#if defined(OS_MACOSX)
typedef struct CGPoint CGPoint;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT IntPoint {
  USING_FAST_MALLOC(IntPoint);

 public:
  IntPoint() : x_(0), y_(0) {}
  IntPoint(int x, int y) : x_(x), y_(y) {}
  explicit IntPoint(const IntSize& size)
      : x_(size.Width()), y_(size.Height()) {}

  static IntPoint Zero() { return IntPoint(); }

  int X() const { return x_; }
  int Y() const { return y_; }

  void SetX(int x) { x_ = x; }
  void SetY(int y) { y_ = y; }

  void Move(const IntSize& s) { Move(s.Width(), s.Height()); }
  void MoveBy(const IntPoint& offset) { Move(offset.X(), offset.Y()); }
  void Move(int dx, int dy) {
    x_ += dx;
    y_ += dy;
  }
  void SaturatedMove(int dx, int dy) {
    x_ = ClampAdd(x_, dx);
    y_ = ClampAdd(y_, dy);
  }

  void Scale(float sx, float sy) {
    x_ = lroundf(static_cast<float>(x_ * sx));
    y_ = lroundf(static_cast<float>(y_ * sy));
  }

  IntPoint ExpandedTo(const IntPoint& other) const {
    return IntPoint(x_ > other.x_ ? x_ : other.x_,
                    y_ > other.y_ ? y_ : other.y_);
  }

  IntPoint ShrunkTo(const IntPoint& other) const {
    return IntPoint(x_ < other.x_ ? x_ : other.x_,
                    y_ < other.y_ ? y_ : other.y_);
  }

  int DistanceSquaredToPoint(const IntPoint&) const;

  void ClampNegativeToZero() { *this = ExpandedTo(Zero()); }

  IntPoint TransposedPoint() const { return IntPoint(y_, x_); }

#if defined(OS_MACOSX)
  explicit IntPoint(
      const CGPoint&);  // don't do this implicitly since it's lossy
  operator CGPoint() const;
#endif

  String ToString() const;

 private:
  int x_, y_;
};

inline IntPoint& operator+=(IntPoint& a, const IntSize& b) {
  a.Move(b.Width(), b.Height());
  return a;
}

inline IntPoint& operator-=(IntPoint& a, const IntSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

inline IntPoint operator+(const IntPoint& a, const IntSize& b) {
  return IntPoint(a.X() + b.Width(), a.Y() + b.Height());
}

inline IntPoint operator+(const IntPoint& a, const IntPoint& b) {
  return IntPoint(a.X() + b.X(), a.Y() + b.Y());
}

inline IntSize operator-(const IntPoint& a, const IntPoint& b) {
  return IntSize(a.X() - b.X(), a.Y() - b.Y());
}

inline IntPoint operator-(const IntPoint& a, const IntSize& b) {
  return IntPoint(a.X() - b.Width(), a.Y() - b.Height());
}

inline IntPoint operator-(const IntPoint& point) {
  return IntPoint(-point.X(), -point.Y());
}

inline bool operator==(const IntPoint& a, const IntPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

inline bool operator!=(const IntPoint& a, const IntPoint& b) {
  return a.X() != b.X() || a.Y() != b.Y();
}

inline IntSize ToIntSize(const IntPoint& a) {
  return IntSize(a.X(), a.Y());
}

inline int IntPoint::DistanceSquaredToPoint(const IntPoint& point) const {
  return ((*this) - point).DiagonalLengthSquared();
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const IntPoint&);

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const IntPoint&, std::ostream*);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::IntPoint);

#endif  // IntPoint_h
