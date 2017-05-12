// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPositionedFloat_h
#define NGPositionedFloat_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

// Contains the information necessary for copying back data to a FloatingObject.
struct CORE_EXPORT NGPositionedFloat {
  NGPositionedFloat(RefPtr<NGPhysicalFragment> fragment,
                    const NGLogicalOffset& logical_offset,
                    const NGPhysicalOffset& paint_offset)
      : fragment(fragment),
        logical_offset(logical_offset),
        paint_offset(paint_offset) {}

  RefPtr<NGPhysicalFragment> fragment;
  NGLogicalOffset logical_offset;

  // In the case where a legacy FloatingObject is attached to not its own
  // parent, e.g. a float surrounded by a bunch of nested empty divs,
  // NG float fragment's LeftOffset() cannot be used as legacy FloatingObject's
  // left offset because that offset should be relative to the original float
  // parent.
  // {@code paint_offset} is calculated when we know to which parent this float
  // would be attached.
  NGPhysicalOffset paint_offset;
};

}  // namespace blink

#endif  // NGPositionedFloat_h
