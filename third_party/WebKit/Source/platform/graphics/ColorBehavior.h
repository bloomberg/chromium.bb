// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorBehavior_h
#define ColorBehavior_h

#include "platform/PlatformExport.h"
#include "public/platform/WebVector.h"
#include "ui/gfx/color_space.h"

namespace blink {

class PLATFORM_EXPORT ColorBehavior {
 public:
  // This specifies to ignore color profiles embedded in images entirely. No
  // transformations will be applied to any pixel data, and no SkImages will be
  // tagged with an SkColorSpace.
  static inline ColorBehavior Ignore() { return ColorBehavior(Type::kIgnore); }
  bool IsIgnore() const { return type_ == Type::kIgnore; }

  // This specifies that images will not be transformed (to the extent
  // possible), but that SkImages will be tagged with the embedded SkColorSpace
  // (or sRGB if there was no embedded color profile).
  static inline ColorBehavior Tag() { return ColorBehavior(Type::kTag); }
  bool IsTag() const { return type_ == Type::kTag; }

  // This specifies that images will be transformed to the specified target
  // color space, and that SkImages will not be tagged with any SkColorSpace.
  static inline ColorBehavior TransformTo(const gfx::ColorSpace& target) {
    return ColorBehavior(Type::kTransformTo, target);
  }
  bool IsTransformToTargetColorSpace() const {
    return type_ == Type::kTransformTo;
  }
  const gfx::ColorSpace& TargetColorSpace() const {
    DCHECK(type_ == Type::kTransformTo);
    return target_;
  }

  // Set the target color profile into which all images with embedded color
  // profiles should be converted. Note that only the first call to this
  // function in this process has any effect.
  static void SetGlobalTargetColorProfile(const gfx::ICCProfile&);
  static void SetGlobalTargetColorSpaceForTesting(const gfx::ColorSpace&);
  static const gfx::ColorSpace& GlobalTargetColorSpace();

  // Return the behavior of transforming to the color space specified above, or
  // sRGB, if the above has not yet been called.
  static ColorBehavior TransformToGlobalTarget();

  // Transform to a target color space to be used by tests.
  static ColorBehavior TransformToTargetForTesting();

  bool operator==(const ColorBehavior&) const;
  bool operator!=(const ColorBehavior&) const;

 private:
  enum class Type {
    kIgnore,
    kTag,
    kTransformTo,
  };
  ColorBehavior(Type type) : type_(type) {}
  ColorBehavior(Type type, const gfx::ColorSpace& target)
      : type_(type), target_(target) {}
  Type type_;
  gfx::ColorSpace target_;
};

}  // namespace blink

#endif  // ColorBehavior_h
