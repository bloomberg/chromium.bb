// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthListPropertyFunctions_h
#define LengthListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "platform/Length.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ComputedStyle;
class CSSProperty;

class LengthListPropertyFunctions {
  STATIC_ONLY(LengthListPropertyFunctions);

 public:
  static ValueRange GetValueRange(const CSSProperty&);
  static bool GetInitialLengthList(const CSSProperty&, Vector<Length>& result);
  static bool GetLengthList(const CSSProperty&,
                            const ComputedStyle&,
                            Vector<Length>& result);
  static void SetLengthList(const CSSProperty&,
                            ComputedStyle&,
                            Vector<Length>&& length_list);
};

}  // namespace blink

#endif  // LengthListPropertyFunctions_h
