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
  static inline ColorBehavior ignore() { return ColorBehavior(Type::Ignore); }
  bool isIgnore() const { return m_type == Type::Ignore; }

  // This specifies that images will not be transformed (to the extent
  // possible), but that SkImages will be tagged with the embedded SkColorSpace
  // (or sRGB if there was no embedded color profile).
  static inline ColorBehavior tag() { return ColorBehavior(Type::Tag); }
  bool isTag() const { return m_type == Type::Tag; }

  // This specifies that images will be transformed to the specified target
  // color space, and that SkImages will not be tagged with any SkColorSpace.
  static inline ColorBehavior transformTo(const gfx::ColorSpace& target) {
    return ColorBehavior(Type::TransformTo, target);
  }
  bool isTransformToTargetColorSpace() const {
    return m_type == Type::TransformTo;
  }
  const gfx::ColorSpace& targetColorSpace() const {
    DCHECK(m_type == Type::TransformTo);
    return m_target;
  }

  // Set the target color profile into which all images with embedded color
  // profiles should be converted. Note that only the first call to this
  // function in this process has any effect.
  static void setGlobalTargetColorProfile(const gfx::ICCProfile&);
  static void setGlobalTargetColorSpaceForTesting(const gfx::ColorSpace&);
  static const gfx::ColorSpace& globalTargetColorSpace();

  // Return the behavior of transforming to the color space specified above, or
  // sRGB, if the above has not yet been called.
  static ColorBehavior transformToGlobalTarget();

  // Transform to a target color space to be used by tests.
  static ColorBehavior transformToTargetForTesting();

  bool operator==(const ColorBehavior&) const;
  bool operator!=(const ColorBehavior&) const;

 private:
  enum class Type {
    Ignore,
    Tag,
    TransformTo,
  };
  ColorBehavior(Type type) : m_type(type) {}
  ColorBehavior(Type type, const gfx::ColorSpace& target)
      : m_type(type), m_target(target) {}
  Type m_type;
  gfx::ColorSpace m_target;
};

}  // namespace blink

#endif  // ColorBehavior_h
