// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorBehavior_h
#define ColorBehavior_h

#include "platform/PlatformExport.h"
#include "ui/gfx/color_space.h"

namespace blink {

class PLATFORM_EXPORT ColorBehavior {
 public:
  // This specifies to ignore color profiles embedded in images entirely. No
  // transformations will be applied to any pixel data, and no SkImages will be
  // tagged with an SkColorSpace.
  static ColorBehavior Ignore() { return ColorBehavior(Type::kIgnore); }
  bool IsIgnore() const { return type_ == Type::kIgnore; }

  // This specifies that images will not be transformed (to the extent
  // possible), but that SkImages will be tagged with the embedded SkColorSpace
  // (or sRGB if there was no embedded color profile).
  static ColorBehavior Tag() { return ColorBehavior(Type::kTag); }
  bool IsTag() const { return type_ == Type::kTag; }

  // This specifies that images will be transformed to sRGB, and that SkImages
  // will not be tagged with any SkColorSpace.
  static ColorBehavior TransformToSRGB() {
    return ColorBehavior(Type::kTransformToSRGB);
  }
  bool IsTransformToSRGB() const { return type_ == Type::kTransformToSRGB; }

  bool operator==(const ColorBehavior&) const;
  bool operator!=(const ColorBehavior&) const;

 private:
  enum class Type {
    kIgnore,
    kTag,
    kTransformToSRGB,
  };
  ColorBehavior(Type type) : type_(type) {}
  Type type_;
};

}  // namespace blink

#endif  // ColorBehavior_h
