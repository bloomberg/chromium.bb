// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

enum EnumInNamespace {
  // These should be renamed to SHOUT_CAPS.
  NamedWrong,
  namedWrong2,
  // This shouldn't exist but if it does renaming them will help us find them.
  kNamedWrong3,
};

class T {
 public:
  enum EnumInClass {
    // These should be renamed to SHOUT_CAPS.
    ClassNamedWrong,
    classNamedWrong22,
    // This shouldn't exist but if it does renaming them will help us find them.
    kClassNamed33Wrong,
  };

  enum class EnumClassInClass {
    // These should be renamed to SHOUT_CAPS.
    EnumClassNamedWrong,
    enumClassNamedWrong22,
    // This shouldn't exist but if it does renaming them will help us find them.
    kEnumClassNamed33Wrong,
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
  blink::EnumInNamespace e1 = blink::NamedWrong;
  blink::EnumInNamespace e2 = blink::namedWrong2;
  blink::T::EnumInClass e3 = blink::T::ClassNamedWrong;
  blink::T::EnumInClass e4 = blink::T::classNamedWrong22;
  blink::T::EnumClassInClass e5 =
      blink::T::EnumClassInClass::EnumClassNamedWrong;
  blink::T::EnumClassInClass e6 =
      blink::T::EnumClassInClass::enumClassNamedWrong22;
  // These should not be renamed.
  EnumOutsideNamespace e7 = OutNamedWrong;
  EnumOutsideNamespace e8 = outNamedWrong2;
}

int G() {
  using blink::NamedWrong;
  using blink::namedWrong2;
  using blink::kNamedWrong3;
  return NamedWrong | namedWrong2 | kNamedWrong3;
}
