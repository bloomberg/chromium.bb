// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingLayerPropertyUpdater_h
#define CompositingLayerPropertyUpdater_h

#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutObject;

class CompositingLayerPropertyUpdater {
  STATIC_ONLY(CompositingLayerPropertyUpdater);

 public:
  static void Update(const LayoutObject&);
};

}  // namespace blink

#endif  // PaintPropertyTreeBuilder_h
