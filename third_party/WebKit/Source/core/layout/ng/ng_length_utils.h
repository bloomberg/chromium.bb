// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLengthUtils_h
#define NGLengthUtils_h

#include "core/CoreExport.h"

namespace blink {
class ComputedStyle;
class LayoutUnit;
class Length;
class NGConstraintSpace;
struct NGBoxMargins;

enum class LengthResolveType { MinSize, MaxSize, ContentSize, MarginSize };

// Convert an inline-axis length to a layout unit using the given constraint
// space.
CORE_EXPORT LayoutUnit resolveInlineLength(const NGConstraintSpace&,
                                           const Length&,
                                           LengthResolveType);

// Convert a block-axis length to a layout unit using the given constraint
// space and content size.
CORE_EXPORT LayoutUnit resolveBlockLength(const NGConstraintSpace&,
                                          const Length&,
                                          LayoutUnit contentSize,
                                          LengthResolveType);

// Resolves the given length to a layout unit, constraining it by the min
// logical width and max logical width properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit computeInlineSizeForFragment(const NGConstraintSpace&,
                                                    const ComputedStyle&);

// Resolves the given length to a layout unit, constraining it by the min
// logical height and max logical height properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit computeBlockSizeForFragment(const NGConstraintSpace&,
                                                   const ComputedStyle&,
                                                   LayoutUnit contentSize);

CORE_EXPORT NGBoxMargins computeMargins(const NGConstraintSpace&,
                                        const ComputedStyle&);

}  // namespace blink

#endif  // NGLengthUtils_h
