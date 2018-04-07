// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGEdge_h
#define NGEdge_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/layout_unit.h"

namespace blink {

// Struct to represent a simple edge that has start and end.
struct CORE_EXPORT NGEdge {
  LayoutUnit start;
  LayoutUnit end;
};

}  // namespace blink

#endif  // NGEdge_h
