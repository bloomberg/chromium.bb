// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

enum EnumInNamespace {
  // These should be renamed to SHOUT_CAPS.
  NAMED_WRONG,
  NAMED_WRONG_2,
  // This shouldn't exist but if it does renaming them will help us find them.
  K_NAMED_WRONG_3,
};

class T {
 public:
  enum EnumInClass {
    // These should be renamed to SHOUT_CAPS.
    CLASS_NAMED_WRONG,
    CLASS_NAMED_WRONG_22,
    // This shouldn't exist but if it does renaming them will help us find them.
    K_CLASS_NAMED_33_WRONG,
  };

  enum class EnumClassInClass {
    // These should be renamed to SHOUT_CAPS.
    ENUM_CLASS_NAMED_WRONG,
    ENUM_CLASS_NAMED_WRONG_22,
    // This shouldn't exist but if it does renaming them will help us find them.
    K_ENUM_CLASS_NAMED_33_WRONG,
  };
};

}  // namespace blink

enum EnumOutsideNamespace {
  // These should not be renamed.
  OutNamedWrong,
  outNamedWrong2,
  kOutNamedWrong3,
};

void F() {
  // These should be renamed to SHOUT_CAPS.
  blink::EnumInNamespace e1 = blink::NAMED_WRONG;
  blink::EnumInNamespace e2 = blink::NAMED_WRONG_2;
  blink::T::EnumInClass e3 = blink::T::CLASS_NAMED_WRONG;
  blink::T::EnumInClass e4 = blink::T::CLASS_NAMED_WRONG_22;
  blink::T::EnumClassInClass e5 =
      blink::T::EnumClassInClass::ENUM_CLASS_NAMED_WRONG;
  blink::T::EnumClassInClass e6 =
      blink::T::EnumClassInClass::ENUM_CLASS_NAMED_WRONG_22;
  // These should not be renamed.
  EnumOutsideNamespace e7 = OutNamedWrong;
  EnumOutsideNamespace e8 = outNamedWrong2;
}

int G() {
  using blink::NAMED_WRONG;
  using blink::NAMED_WRONG_2;
  using blink::K_NAMED_WRONG_3;
  return NAMED_WRONG | NAMED_WRONG_2 | K_NAMED_WRONG_3;
}
