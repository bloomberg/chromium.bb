/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_SIZE_H_

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

#if defined(OS_MAC)
typedef struct CGSize CGSize;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT IntSize {
  DISALLOW_NEW();

 public:
  constexpr IntSize() : width_(0), height_(0) {}
  constexpr IntSize(int width, int height) : width_(width), height_(height) {}
  constexpr explicit IntSize(const gfx::Size& s)
      : IntSize(s.width(), s.height()) {}
  constexpr explicit IntSize(const gfx::Vector2d& v) : IntSize(v.x(), v.y()) {}

  constexpr int width() const { return width_; }
  constexpr int height() const { return height_; }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }

  constexpr bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }
  constexpr bool IsZero() const { return !width_ && !height_; }

  float AspectRatio() const {
    return static_cast<float>(width_) / static_cast<float>(height_);
  }

  void Enlarge(int width, int height) {
    width_ += width;
    height_ += height;
  }

  void Scale(float width_scale, float height_scale) {
    width_ = static_cast<int>(static_cast<float>(width_) * width_scale);
    height_ = static_cast<int>(static_cast<float>(height_) * height_scale);
  }

  void Scale(float scale) { this->Scale(scale, scale); }

  IntSize ExpandedTo(const IntSize& other) const {
    return IntSize(width_ > other.width_ ? width_ : other.width_,
                   height_ > other.height_ ? height_ : other.height_);
  }

  IntSize ShrunkTo(const IntSize& other) const {
    return IntSize(width_ < other.width_ ? width_ : other.width_,
                   height_ < other.height_ ? height_ : other.height_);
  }

  void ClampNegativeToZero() { *this = ExpandedTo(IntSize()); }

  void SetToMin(const IntSize& minimum_size) {
    if (width_ < minimum_size.width())
      width_ = minimum_size.width();
    if (height_ < minimum_size.height())
      height_ = minimum_size.height();
  }

  // Return area in a uint64_t to avoid overflow.
  uint64_t Area() const { return static_cast<uint64_t>(width()) * height(); }

  int DiagonalLengthSquared() const {
    return width_ * width_ + height_ * height_;
  }

  IntSize TransposedSize() const { return IntSize(height_, width_); }

#if defined(OS_MAC)
  explicit IntSize(const CGSize&);
  explicit operator CGSize() const;
#endif

  // These are deleted during blink geometry type to gfx migration.
  // Use ToGfxSize() and ToGfxVector2d() instead.
  operator gfx::Size() const = delete;
  operator gfx::Vector2d() const = delete;

  String ToString() const;

 private:
  int width_, height_;
};

inline IntSize& operator+=(IntSize& a, const IntSize& b) {
  a.set_width(a.width() + b.width());
  a.set_height(a.height() + b.height());
  return a;
}

inline IntSize& operator-=(IntSize& a, const IntSize& b) {
  a.set_width(a.width() - b.width());
  a.set_height(a.height() - b.height());
  return a;
}

inline IntSize operator+(const IntSize& a, const IntSize& b) {
  return IntSize(a.width() + b.width(), a.height() + b.height());
}

inline IntSize operator-(const IntSize& a, const IntSize& b) {
  return IntSize(a.width() - b.width(), a.height() - b.height());
}

inline IntSize operator-(const IntSize& size) {
  return IntSize(-size.width(), -size.height());
}

inline bool operator==(const IntSize& a, const IntSize& b) {
  return a.width() == b.width() && a.height() == b.height();
}

inline bool operator!=(const IntSize& a, const IntSize& b) {
  return a.width() != b.width() || a.height() != b.height();
}

// Temporary converter to replace ToIntSize(const IntPoint&).
constexpr IntSize ToIntSize(const gfx::Point& p) {
  return IntSize(p.x(), p.y());
}

// Temporary converter to replace IntPoint(const IntSize&).
constexpr gfx::Point ToGfxPoint(const IntSize& s) {
  return gfx::Point(s.width(), s.height());
}

// Use this only for logical sizes, which can not be negative. Things that are
// offsets instead, and can be negative, should use a gfx::Vector2d.
constexpr gfx::Size ToGfxSize(const IntSize& s) {
  return gfx::Size(s.width(), s.height());
}
// IntSize is used as an offset, which can be negative, but gfx::Size can not.
// The Vector2d type is used for offsets instead.
constexpr gfx::Vector2d ToGfxVector2d(const IntSize& s) {
  return gfx::Vector2d(s.width(), s.height());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const IntSize&);

}  // namespace blink

namespace WTF {

template <>
struct CrossThreadCopier<blink::IntSize>
    : public CrossThreadCopierPassThrough<blink::IntSize> {
  STATIC_ONLY(CrossThreadCopier);
};

}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_SIZE_H_
