/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
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

#ifndef LayoutPoint_h
#define LayoutPoint_h

#include <iosfwd>
#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class PLATFORM_EXPORT LayoutPoint {
  DISALLOW_NEW();

 public:
  LayoutPoint() = default;
  LayoutPoint(LayoutUnit x, LayoutUnit y) : x_(x), y_(y) {}
  LayoutPoint(int x, int y) : x_(LayoutUnit(x)), y_(LayoutUnit(y)) {}
  LayoutPoint(const IntPoint& point) : x_(point.X()), y_(point.Y()) {}
  explicit LayoutPoint(const FloatPoint& point)
      : x_(point.X()), y_(point.Y()) {}
  explicit LayoutPoint(const DoublePoint& point)
      : x_(point.X()), y_(point.Y()) {}
  explicit LayoutPoint(const LayoutSize& size)
      : x_(size.Width()), y_(size.Height()) {}

  static LayoutPoint Zero() { return LayoutPoint(); }

  LayoutUnit X() const { return x_; }
  LayoutUnit Y() const { return y_; }

  void SetX(LayoutUnit x) { x_ = x; }
  void SetY(LayoutUnit y) { y_ = y; }

  void Move(const LayoutSize& s) { Move(s.Width(), s.Height()); }
  void Move(const IntSize& s) { Move(s.Width(), s.Height()); }
  void MoveBy(const LayoutPoint& offset) { Move(offset.X(), offset.Y()); }
  void Move(int dx, int dy) { Move(LayoutUnit(dx), LayoutUnit(dy)); }
  void Move(LayoutUnit dx, LayoutUnit dy) {
    x_ += dx;
    y_ += dy;
  }
  void Scale(float sx, float sy) {
    x_ *= sx;
    y_ *= sy;
  }

  LayoutPoint ExpandedTo(const LayoutPoint&) const;
  LayoutPoint ShrunkTo(const LayoutPoint&) const;

  void ClampNegativeToZero() { *this = ExpandedTo(Zero()); }

  LayoutPoint TransposedPoint() const { return LayoutPoint(y_, x_); }

  String ToString() const;

 private:
  LayoutUnit x_, y_;
};

ALWAYS_INLINE LayoutPoint& operator+=(LayoutPoint& a, const LayoutSize& b) {
  a.Move(b.Width(), b.Height());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator+=(LayoutPoint& a, const LayoutPoint& b) {
  a.Move(b.X(), b.Y());
  return a;
}

inline LayoutPoint& operator+=(LayoutPoint& a, const IntSize& b) {
  a.Move(b.Width(), b.Height());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator-=(LayoutPoint& a, const LayoutPoint& b) {
  a.Move(-b.X(), -b.Y());
  return a;
}

ALWAYS_INLINE LayoutPoint& operator-=(LayoutPoint& a, const LayoutSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

inline LayoutPoint& operator-=(LayoutPoint& a, const IntSize& b) {
  a.Move(-b.Width(), -b.Height());
  return a;
}

inline LayoutPoint operator+(const LayoutPoint& a, const LayoutSize& b) {
  return LayoutPoint(a.X() + b.Width(), a.Y() + b.Height());
}

ALWAYS_INLINE LayoutPoint operator+(const LayoutPoint& a,
                                    const LayoutPoint& b) {
  return LayoutPoint(a.X() + b.X(), a.Y() + b.Y());
}

ALWAYS_INLINE LayoutSize operator-(const LayoutPoint& a, const LayoutPoint& b) {
  return LayoutSize(a.X() - b.X(), a.Y() - b.Y());
}

ALWAYS_INLINE LayoutSize operator-(const LayoutPoint& a, const IntPoint& b) {
  return LayoutSize(a.X() - b.X(), a.Y() - b.Y());
}

inline LayoutPoint operator-(const LayoutPoint& a, const LayoutSize& b) {
  return LayoutPoint(a.X() - b.Width(), a.Y() - b.Height());
}

inline LayoutPoint operator-(const LayoutPoint& a, const IntSize& b) {
  return LayoutPoint(a.X() - b.Width(), a.Y() - b.Height());
}

inline LayoutPoint operator-(const LayoutPoint& point) {
  return LayoutPoint(-point.X(), -point.Y());
}

ALWAYS_INLINE bool operator==(const LayoutPoint& a, const LayoutPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

inline bool operator!=(const LayoutPoint& a, const LayoutPoint& b) {
  return a.X() != b.X() || a.Y() != b.Y();
}

inline LayoutPoint ToPoint(const LayoutSize& size) {
  return LayoutPoint(size.Width(), size.Height());
}

inline LayoutPoint ToLayoutPoint(const LayoutSize& p) {
  return LayoutPoint(p.Width(), p.Height());
}

inline LayoutSize ToSize(const LayoutPoint& a) {
  return LayoutSize(a.X(), a.Y());
}

inline IntPoint FlooredIntPoint(const LayoutPoint& point) {
  return IntPoint(point.X().Floor(), point.Y().Floor());
}

inline IntPoint RoundedIntPoint(const LayoutPoint& point) {
  return IntPoint(point.X().Round(), point.Y().Round());
}

inline IntPoint RoundedIntPoint(const LayoutSize& size) {
  return IntPoint(size.Width().Round(), size.Height().Round());
}

inline IntPoint CeiledIntPoint(const LayoutPoint& point) {
  return IntPoint(point.X().Ceil(), point.Y().Ceil());
}

inline LayoutPoint FlooredLayoutPoint(const FloatPoint& p) {
  return LayoutPoint(LayoutUnit::FromFloatFloor(p.X()),
                     LayoutUnit::FromFloatFloor(p.Y()));
}

inline LayoutPoint CeiledLayoutPoint(const FloatPoint& p) {
  return LayoutPoint(LayoutUnit::FromFloatCeil(p.X()),
                     LayoutUnit::FromFloatCeil(p.Y()));
}

inline IntSize PixelSnappedIntSize(const LayoutSize& s, const LayoutPoint& p) {
  return IntSize(SnapSizeToPixel(s.Width(), p.X()),
                 SnapSizeToPixel(s.Height(), p.Y()));
}

inline IntSize RoundedIntSize(const LayoutPoint& p) {
  return IntSize(p.X().Round(), p.Y().Round());
}

inline LayoutSize ToLayoutSize(const LayoutPoint& p) {
  return LayoutSize(p.X(), p.Y());
}

inline LayoutPoint FlooredLayoutPoint(const FloatSize& s) {
  return FlooredLayoutPoint(FloatPoint(s));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutPoint&);

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const LayoutPoint&, std::ostream*);

}  // namespace blink

#endif  // LayoutPoint_h
