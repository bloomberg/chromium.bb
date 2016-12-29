// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorBehavior_h
#define ColorBehavior_h

#include "platform/PlatformExport.h"
#include "public/platform/WebVector.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorSpace;

namespace blink {

class PLATFORM_EXPORT ColorBehavior {
 public:
  // This specifies to ignore color profiles embedded in images entirely. No
  // transformations will be applied to any pixel data, and no SkImages will be
  // tagged with an SkColorSpace.
  static inline ColorBehavior ignore() {
    return ColorBehavior(Type::Ignore, nullptr);
  }
  bool isIgnore() const { return m_type == Type::Ignore; }

  // This specifies that images will not be transformed (to the extent
  // possible), but that SkImages will be tagged with the embedded SkColorSpace
  // (or sRGB if there was no embedded color profile).
  static inline ColorBehavior tag() {
    return ColorBehavior(Type::Tag, nullptr);
  }
  bool isTag() const { return m_type == Type::Tag; }

  // This specifies that images will be transformed to the specified target
  // color space, and that SkImages will not be tagged with any SkColorSpace.
  static inline ColorBehavior transformTo(sk_sp<SkColorSpace> target) {
    return ColorBehavior(Type::TransformTo, std::move(target));
  }
  bool isTransformToTargetColorSpace() const {
    return m_type == Type::TransformTo;
  }
  sk_sp<SkColorSpace> targetColorSpace() const {
    DCHECK(m_type == Type::TransformTo);
    return m_target;
  }

  // Set the target color profile into which all images with embedded color
  // profiles should be converted. Note that only the first call to this
  // function in this process has any effect.
  static void setGlobalTargetColorSpace(const sk_sp<SkColorSpace>&);
  // This is the same as the above function, but will always take effect.
  static void setGlobalTargetColorSpaceForTesting(const sk_sp<SkColorSpace>&);
  static sk_sp<SkColorSpace> globalTargetColorSpace();

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
  ColorBehavior(Type type, sk_sp<SkColorSpace> target)
      : m_type(type), m_target(std::move(target)) {}
  Type m_type;
  sk_sp<SkColorSpace> m_target;
};

}  // namespace blink

#endif  // ColorBehavior_h
