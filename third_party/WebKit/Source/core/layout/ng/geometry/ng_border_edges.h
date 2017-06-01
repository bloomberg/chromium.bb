// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBorderEdges_h
#define NGBorderEdges_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

namespace NGBorderEdges {

// Which border edges should be painted. Due to fragmentation one or more may
// be skipped.
enum Physical { kTop = 1, kRight = 2, kBottom = 4, kLeft = 8 };

enum Logical {
  kBlockStart = Physical::kTop,
  kLineRight = Physical::kRight,
  kBlockEnd = Physical::kBottom,
  kLineLeft = Physical::kLeft,
  kAll = kBlockStart | kLineRight | kBlockEnd | kLineLeft
};

CORE_EXPORT Physical ToPhysical(Logical, NGWritingMode);
CORE_EXPORT Logical ToLogical(Physical, NGWritingMode);

}  // namespace NGBorderEdges

}  // namespace blink

#endif  // NGBorderEdges_h
