/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FloatRoundedRect_h
#define FloatRoundedRect_h

#include <iosfwd>
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/wtf/Allocator.h"
#include "third_party/skia/include/core/SkRRect.h"

namespace blink {

class FloatQuad;

class PLATFORM_EXPORT FloatRoundedRect {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  class PLATFORM_EXPORT Radii {
    DISALLOW_NEW();

   public:
    Radii() {}
    Radii(const FloatSize& top_left,
          const FloatSize& top_right,
          const FloatSize& bottom_left,
          const FloatSize& bottom_right)
        : top_left_(top_left),
          top_right_(top_right),
          bottom_left_(bottom_left),
          bottom_right_(bottom_right) {}

    Radii(const FloatRoundedRect::Radii& int_radii)
        : top_left_(int_radii.TopLeft()),
          top_right_(int_radii.TopRight()),
          bottom_left_(int_radii.BottomLeft()),
          bottom_right_(int_radii.BottomRight()) {}

    void SetTopLeft(const FloatSize& size) { top_left_ = size; }
    void SetTopRight(const FloatSize& size) { top_right_ = size; }
    void SetBottomLeft(const FloatSize& size) { bottom_left_ = size; }
    void SetBottomRight(const FloatSize& size) { bottom_right_ = size; }
    const FloatSize& TopLeft() const { return top_left_; }
    const FloatSize& TopRight() const { return top_right_; }
    const FloatSize& BottomLeft() const { return bottom_left_; }
    const FloatSize& BottomRight() const { return bottom_right_; }

    bool IsZero() const;

    void Scale(float factor);
    // Multiply all radii by |factor| and floor the result to the nearest
    // integer.
    void ScaleAndFloor(float factor);

    void Expand(float top_width,
                float bottom_width,
                float left_width,
                float right_width);
    void Expand(float size) { Expand(size, size, size, size); }

    void Shrink(float top_width,
                float bottom_width,
                float left_width,
                float right_width);
    void Shrink(float size) { Shrink(size, size, size, size); }

    void IncludeLogicalEdges(const Radii& edges,
                             bool is_horizontal,
                             bool include_logical_left_edge,
                             bool include_logical_right_edge);

    String ToString() const;

   private:
    FloatSize top_left_;
    FloatSize top_right_;
    FloatSize bottom_left_;
    FloatSize bottom_right_;
  };

  FloatRoundedRect() {}
  explicit FloatRoundedRect(const FloatRect&, const Radii& = Radii());
  FloatRoundedRect(float x, float y, float width, float height);
  FloatRoundedRect(const FloatRect&,
                   const FloatSize& top_left,
                   const FloatSize& top_right,
                   const FloatSize& bottom_left,
                   const FloatSize& bottom_right);

  const FloatRect& Rect() const { return rect_; }
  const Radii& GetRadii() const { return radii_; }
  bool IsRounded() const { return !radii_.IsZero(); }
  bool IsEmpty() const { return rect_.IsEmpty(); }

  void SetRect(const FloatRect& rect) { rect_ = rect; }
  void SetRadii(const Radii& radii) { radii_ = radii; }

  void Move(const FloatSize& size) { rect_.Move(size); }
  void InflateWithRadii(int size);
  void Inflate(float size) { rect_.Inflate(size); }

  // expandRadii() does not have any effect on corner radii which have zero
  // width or height. This is because the process of expanding the radius of a
  // corner is not allowed to make sharp corners non-sharp. This applies when
  // "spreading" a shadow or a box shape.
  void ExpandRadii(float size) { radii_.Expand(size); }
  void ShrinkRadii(float size) { radii_.Shrink(size); }

  // Returns a quickly computed rect enclosed by the rounded rect.
  FloatRect RadiusCenterRect() const;

  FloatRect TopLeftCorner() const {
    return FloatRect(rect_.X(), rect_.Y(), radii_.TopLeft().Width(),
                     radii_.TopLeft().Height());
  }
  FloatRect TopRightCorner() const {
    return FloatRect(rect_.MaxX() - radii_.TopRight().Width(), rect_.Y(),
                     radii_.TopRight().Width(), radii_.TopRight().Height());
  }
  FloatRect BottomLeftCorner() const {
    return FloatRect(rect_.X(), rect_.MaxY() - radii_.BottomLeft().Height(),
                     radii_.BottomLeft().Width(), radii_.BottomLeft().Height());
  }
  FloatRect BottomRightCorner() const {
    return FloatRect(rect_.MaxX() - radii_.BottomRight().Width(),
                     rect_.MaxY() - radii_.BottomRight().Height(),
                     radii_.BottomRight().Width(),
                     radii_.BottomRight().Height());
  }

  bool XInterceptsAtY(float y,
                      float& min_x_intercept,
                      float& max_x_intercept) const;

  void IncludeLogicalEdges(const Radii& edges,
                           bool is_horizontal,
                           bool include_logical_left_edge,
                           bool include_logical_right_edge);

  // Tests whether the quad intersects any part of this rounded rectangle.
  // This only works for convex quads.
  bool IntersectsQuad(const FloatQuad&) const;

  void AdjustRadii();
  bool IsRenderable() const;

  // Constrains the radii to be no more than the size of rect(); radii outside
  // of this range are not defined.  In addition, the radii of the corners are
  // floored to the nearest integer.
  // FIXME: the flooring should not be necessary. At the moment it causes
  // background bleed in some cases.
  // FIXME: this code is almost the same as adjustRadii()/isRenderable(). Get
  // rid of one of them.
  void ConstrainRadii();

  operator SkRRect() const;

  String ToString() const;

 private:
  FloatRect rect_;
  Radii radii_;
};

inline FloatRoundedRect::operator SkRRect() const {
  SkRRect rrect;

  if (IsRounded()) {
    SkVector radii[4];
    radii[SkRRect::kUpperLeft_Corner].set(TopLeftCorner().Width(),
                                          TopLeftCorner().Height());
    radii[SkRRect::kUpperRight_Corner].set(TopRightCorner().Width(),
                                           TopRightCorner().Height());
    radii[SkRRect::kLowerRight_Corner].set(BottomRightCorner().Width(),
                                           BottomRightCorner().Height());
    radii[SkRRect::kLowerLeft_Corner].set(BottomLeftCorner().Width(),
                                          BottomLeftCorner().Height());

    rrect.setRectRadii(Rect(), radii);
  } else {
    rrect.setRect(Rect());
  }

  return rrect;
}

inline bool operator==(const FloatRoundedRect::Radii& a,
                       const FloatRoundedRect::Radii& b) {
  return a.TopLeft() == b.TopLeft() && a.TopRight() == b.TopRight() &&
         a.BottomLeft() == b.BottomLeft() && a.BottomRight() == b.BottomRight();
}

inline bool operator!=(const FloatRoundedRect::Radii& a,
                       const FloatRoundedRect::Radii& b) {
  return !(a == b);
}

inline bool operator==(const FloatRoundedRect& a, const FloatRoundedRect& b) {
  return a.Rect() == b.Rect() && a.GetRadii() == b.GetRadii();
}

inline bool operator!=(const FloatRoundedRect& a, const FloatRoundedRect& b) {
  return !(a == b);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const FloatRoundedRect&);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const FloatRoundedRect::Radii&);

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const FloatRoundedRect&, std::ostream*);
void PrintTo(const FloatRoundedRect::Radii&, std::ostream*);

}  // namespace blink

#endif  // FloatRoundedRect_h
