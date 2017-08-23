// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPositionedFloat_h
#define NGPositionedFloat_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {

// Contains the information necessary for copying back data to a FloatingObject.
struct CORE_EXPORT NGPositionedFloat {
  NGPositionedFloat(RefPtr<NGLayoutResult> layout_result,
                    const NGLogicalOffset& logical_offset);

  RefPtr<NGLayoutResult> layout_result;
  NGLogicalOffset logical_offset;
};

}  // namespace blink

#endif  // NGPositionedFloat_h
