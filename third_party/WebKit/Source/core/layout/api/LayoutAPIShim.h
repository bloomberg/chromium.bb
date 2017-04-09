// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutAPIShim_h
#define LayoutAPIShim_h

#include "core/layout/api/LayoutItem.h"

namespace blink {

class LayoutObject;

// TODO(pilgrim): Remove this kludge once clients have a real API and no longer
// need access to the underlying LayoutObject.
class LayoutAPIShim {
 public:
  static LayoutObject* LayoutObjectFrom(LayoutItem item) {
    return item.GetLayoutObject();
  }

  static const LayoutObject* ConstLayoutObjectFrom(LayoutItem item) {
    return item.GetLayoutObject();
  }
};

}  // namespace blink

#endif  // LayoutAPIShim_h
