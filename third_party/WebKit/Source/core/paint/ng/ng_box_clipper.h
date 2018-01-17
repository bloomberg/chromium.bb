// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxClipper_h
#define NGBoxClipper_h

#include "core/paint/BoxClipperBase.h"

namespace blink {

class NGPaintFragment;

class NGBoxClipper : public BoxClipperBase {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NGBoxClipper(const NGPaintFragment&, const PaintInfo&);
};

}  // namespace blink

#endif  // NGBoxClipper_h
