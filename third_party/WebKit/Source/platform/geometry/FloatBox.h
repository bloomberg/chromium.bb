/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
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
#ifndef FloatBox_h
#define FloatBox_h

#include <cmath>
#include <iosfwd>
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT FloatBox {
  DISALLOW_NEW();

 public:
  FloatBox() : x_(0), y_(0), z_(0), width_(0), height_(0), depth_(0) {}

  FloatBox(float x, float y, float z, float width, float height, float depth)
      : x_(x), y_(y), z_(z), width_(width), height_(height), depth_(depth) {}

  FloatBox(const FloatBox& box)
      : x_(box.X()),
        y_(box.Y()),
        z_(box.Z()),
        width_(box.Width()),
        height_(box.Height()),
        depth_(box.Depth()) {}

  void SetOrigin(const FloatPoint3D& origin) {
    x_ = origin.X();
    y_ = origin.Y();
    z_ = origin.Z();
  }

  void SetSize(const FloatPoint3D& origin) {
    DCHECK_GE(origin.X(), 0);
    DCHECK_GE(origin.Y(), 0);
    DCHECK_GE(origin.Z(), 0);

    width_ = origin.X();
    height_ = origin.Y();
    depth_ = origin.Z();
  }

  void Move(const FloatPoint3D& location) {
    x_ += location.X();
    y_ += location.Y();
    z_ += location.Z();
  }

  void Flatten() {
    z_ = 0;
    depth_ = 0;
  }

  void ExpandTo(const FloatPoint3D& low, const FloatPoint3D& high);
  void ExpandTo(const FloatPoint3D& point) { ExpandTo(point, point); }

  void ExpandTo(const FloatBox& box) {
    ExpandTo(FloatPoint3D(box.X(), box.Y(), box.Z()),
             FloatPoint3D(box.Right(), box.Bottom(), box.front()));
  }

  void UnionBounds(const FloatBox& box) {
    if (box.IsEmpty())
      return;

    if (IsEmpty()) {
      *this = box;
      return;
    }

    ExpandTo(box);
  }

  bool IsEmpty() const {
    return (width_ <= 0 && height_ <= 0) || (width_ <= 0 && depth_ <= 0) ||
           (height_ <= 0 && depth_ <= 0);
  }

  float Right() const { return x_ + width_; }
  float Bottom() const { return y_ + height_; }
  float front() const { return z_ + depth_; }
  float X() const { return x_; }
  float Y() const { return y_; }
  float Z() const { return z_; }
  float Width() const { return width_; }
  float Height() const { return height_; }
  float Depth() const { return depth_; }

  String ToString() const;

 private:
  float x_;
  float y_;
  float z_;
  float width_;
  float height_;
  float depth_;
};

inline bool operator==(const FloatBox& a, const FloatBox& b) {
  return a.X() == b.X() && a.Y() == b.Y() && a.Z() == b.Z() &&
         a.Width() == b.Width() && a.Height() == b.Height() &&
         a.Depth() == b.Depth();
}

inline bool operator!=(const FloatBox& a, const FloatBox& b) {
  return !(a == b);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatBox&);

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const FloatBox&, std::ostream*);

}  // namespace blink

#endif
