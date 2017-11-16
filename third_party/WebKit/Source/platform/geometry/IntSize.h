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

#ifndef IntSize_h
#define IntSize_h

#include "build/build_config.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebCommon.h"

#if defined(OS_MACOSX)
typedef struct CGSize CGSize;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace gfx {
class Size;
}

namespace blink {

class PLATFORM_EXPORT IntSize {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  IntSize() : width_(0), height_(0) {}
  IntSize(int width, int height) : width_(width), height_(height) {}

  int Width() const { return width_; }
  int Height() const { return height_; }

  void SetWidth(int width) { width_ = width; }
  void SetHeight(int height) { height_ = height; }

  bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }
  bool IsZero() const { return !width_ && !height_; }

  float AspectRatio() const {
    return static_cast<float>(width_) / static_cast<float>(height_);
  }

  void Expand(int width, int height) {
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

  void ClampToMinimumSize(const IntSize& minimum_size) {
    if (width_ < minimum_size.Width())
      width_ = minimum_size.Width();
    if (height_ < minimum_size.Height())
      height_ = minimum_size.Height();
  }

  // Return area in a uint64_t to avoid overflow.
  uint64_t Area() const { return static_cast<uint64_t>(Width()) * Height(); }

  int DiagonalLengthSquared() const {
    return width_ * width_ + height_ * height_;
  }

  IntSize TransposedSize() const { return IntSize(height_, width_); }

#if defined(OS_MACOSX)
  explicit IntSize(const CGSize&);
  explicit operator CGSize() const;
#endif

  operator gfx::Size() const;

  String ToString() const;

 private:
  int width_, height_;
};

inline IntSize& operator+=(IntSize& a, const IntSize& b) {
  a.SetWidth(a.Width() + b.Width());
  a.SetHeight(a.Height() + b.Height());
  return a;
}

inline IntSize& operator-=(IntSize& a, const IntSize& b) {
  a.SetWidth(a.Width() - b.Width());
  a.SetHeight(a.Height() - b.Height());
  return a;
}

inline IntSize operator+(const IntSize& a, const IntSize& b) {
  return IntSize(a.Width() + b.Width(), a.Height() + b.Height());
}

inline IntSize operator-(const IntSize& a, const IntSize& b) {
  return IntSize(a.Width() - b.Width(), a.Height() - b.Height());
}

inline IntSize operator-(const IntSize& size) {
  return IntSize(-size.Width(), -size.Height());
}

inline bool operator==(const IntSize& a, const IntSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

inline bool operator!=(const IntSize& a, const IntSize& b) {
  return a.Width() != b.Width() || a.Height() != b.Height();
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const IntSize&);

// Redeclared here to avoid ODR issues.
// See platform/testing/GeometryPrinters.h.
void PrintTo(const IntSize&, std::ostream*);

}  // namespace blink

#endif  // IntSize_h
