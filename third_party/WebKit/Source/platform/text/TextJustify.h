// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextJustify_h
#define TextJustify_h

namespace blink {

enum TextJustify {
  kTextJustifyAuto = 0x0,
  kTextJustifyNone = 0x1,
  kTextJustifyInterWord = 0x2,
  kTextJustifyDistribute = 0x3
};

}  // namespace blink

#endif  // TextJustify_h
