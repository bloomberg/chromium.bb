// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VariableFontCheck_h
#define VariableFontCheck_h

class SkTypeface;

namespace blink {

class VariableFontCheck {
 public:
  static bool IsVariableFont(SkTypeface*);
};

}  // namespace blink

#endif
