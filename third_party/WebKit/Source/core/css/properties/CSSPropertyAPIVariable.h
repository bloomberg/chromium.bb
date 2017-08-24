// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPIVariable_h
#define CSSPropertyAPIVariable_h

#include "core/css/properties/CSSPropertyAPI.h"

namespace blink {

class CSSPropertyAPIVariable : public CSSPropertyAPI {
 public:
  bool IsInherited() const override { return true; }
};

}  // namespace blink

#endif  // CSSPropertyAPIVariable_h
