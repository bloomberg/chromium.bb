/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IntRect_h
#define IntRect_h

#include "build/build_config.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRectOutsets.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/VectorTraits.h"

#if defined(OS_MACOSX)
typedef struct CGRect CGRect;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

struct SkRect;
struct SkIRect;

namespace gfx {
class Rect;
}

namespace blink {

class FloatRect;
class LayoutRect;

class PLATFORM_EXPORT IntRect {
  USING_FAST_MALLOC(IntRect);

 public:
  IntRect() {}
  IntRect(const IntPoint& location, const IntSize& size)
      : location_(location), size_(size) {}
  IntRect(int x, int y, int width, int height)
      : location_(IntPoint(x, y)), size_(IntSize(width, height)) {}

  explicit IntRect(
      const FloatRect&);  // don't do this implicitly since it's lossy
  explicit IntRect(
      const LayoutRect&);  // don't do this implicitly since it's lossy

  IntPoint Location() const { return location_; }
  IntSize Size() const { return size_; }

  void SetLocation(const IntPoint& location) { location_ = location; }
  void SetSize(const IntSize& size) { size_ = size; }

  int X() const { return location_.X(); }
  int Y() const { return location_.Y(); }
  int MaxX() const { return X() + Width(); }
  int MaxY() const { return Y() + Height(); }
  int Width() const { return size_.Width(); }
  int Height() const { return size_.Height(); }

  void SetX(int x) { location_.SetX(x); }
  void SetY(int y) { location_.SetY(y); }
  void SetWidth(int width) { size_.SetWidth(width); }
  void SetHeight(int height) { size_.SetHeight(height); }

  bool IsEmpty() const { return size_.IsEmpty(); }

  // NOTE: The result is rounded to integer values, and thus may be not the
  // exact center point.
  IntPoint Center() const {
    return IntPoint(X() + Width() / 2, Y() + Height() / 2);
  }

  void Move(const IntSize& size) { location_ += size; }
  void MoveBy(const IntPoint& offset) {
    location_.Move(offset.X(), offset.Y());
  }
  void Move(int dx, int dy) { location_.Move(dx, dy); }
  void SaturatedMove(int dx, int dy) { location_.SaturatedMove(dx, dy); }

  void Expand(const IntSize& size) { size_ += size; }
  void Expand(int dw, int dh) { size_.Expand(dw, dh); }
  void Expand(const IntRectOutsets& outsets) {
    location_.Move(-outsets.Left(), -outsets.Top());
    size_.Expand(outsets.Left() + outsets.Right(),
                 outsets.Top() + outsets.Bottom());
  }

  void Contract(const IntSize& size) { size_ -= size; }
  void Contract(int dw, int dh) { size_.Expand(-dw, -dh); }

  void ShiftXEdgeTo(int edge) {
    int delta = edge - X();
    SetX(edge);
    SetWidth(std::max(0, Width() - delta));
  }
  void ShiftMaxXEdgeTo(int edge) {
    int delta = edge - MaxX();
    SetWidth(std::max(0, Width() + delta));
  }
  void ShiftYEdgeTo(int edge) {
    int delta = edge - Y();
    SetY(edge);
    SetHeight(std::max(0, Height() - delta));
  }
  void ShiftMaxYEdgeTo(int edge) {
    int delta = edge - MaxY();
    SetHeight(std::max(0, Height() + delta));
  }

  IntPoint MinXMinYCorner() const { return location_; }  // typically topLeft
  IntPoint MaxXMinYCorner() const {
    return IntPoint(location_.X() + size_.Width(), location_.Y());
  }  // typically topRight
  IntPoint MinXMaxYCorner() const {
    return IntPoint(location_.X(), location_.Y() + size_.Height());
  }  // typically bottomLeft
  IntPoint MaxXMaxYCorner() const {
    return IntPoint(location_.X() + size_.Width(),
                    location_.Y() + size_.Height());
  }  // typically bottomRight

  bool Intersects(const IntRect&) const;
  bool Contains(const IntRect&) const;

  // This checks to see if the rect contains x,y in the traditional sense.
  // Equivalent to checking if the rect contains a 1x1 rect below and to the
  // right of (px,py).
  bool Contains(int px, int py) const {
    return px >= X() && px < MaxX() && py >= Y() && py < MaxY();
  }
  bool Contains(const IntPoint& point) const {
    return Contains(point.X(), point.Y());
  }

  void Intersect(const IntRect&);
  void Unite(const IntRect&);
  void UniteIfNonZero(const IntRect&);

  // Besides non-empty rects, this method also unites empty rects (as points or
  // line segments).  For example, union of (100, 100, 0x0) and (200, 200, 50x0)
  // is (100, 100, 150x100).
  void UniteEvenIfEmpty(const IntRect&);

  void InflateX(int dx) {
    location_.SetX(location_.X() - dx);
    size_.SetWidth(size_.Width() + dx + dx);
  }
  void InflateY(int dy) {
    location_.SetY(location_.Y() - dy);
    size_.SetHeight(size_.Height() + dy + dy);
  }
  void Inflate(int d) {
    InflateX(d);
    InflateY(d);
  }
  void Scale(float s);

  IntSize DifferenceToPoint(const IntPoint&) const;
  int DistanceSquaredToPoint(const IntPoint& p) const {
    return DifferenceToPoint(p).DiagonalLengthSquared();
  }

  IntRect TransposedRect() const {
    return IntRect(location_.TransposedPoint(), size_.TransposedSize());
  }

#if defined(OS_MACOSX)
  operator CGRect() const;
#endif

  operator SkRect() const;
  operator SkIRect() const;

  operator gfx::Rect() const;

  String ToString() const;

 private:
  IntPoint location_;
  IntSize size_;
};

inline IntRect Intersection(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Intersect(b);
  return c;
}

inline IntRect UnionRect(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.Unite(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRect(const Vector<IntRect>&);

inline IntRect UnionRectEvenIfEmpty(const IntRect& a, const IntRect& b) {
  IntRect c = a;
  c.UniteEvenIfEmpty(b);
  return c;
}

PLATFORM_EXPORT IntRect UnionRectEvenIfEmpty(const Vector<IntRect>&);

inline bool operator==(const IntRect& a, const IntRect& b) {
  return a.Location() == b.Location() && a.Size() == b.Size();
}

inline bool operator!=(const IntRect& a, const IntRect& b) {
  return a.Location() != b.Location() || a.Size() != b.Size();
}

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const IntRect&, std::ostream*);

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::IntRect);

#endif  // IntRect_h
