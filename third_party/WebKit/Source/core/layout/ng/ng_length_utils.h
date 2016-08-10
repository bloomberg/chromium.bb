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

enum class LengthResolveType { MinSize, MaxSize, ContentSize };

// Convert an inline-axis length to a layout unit using the given constraint
// space.
CORE_EXPORT LayoutUnit resolveInlineLength(LengthResolveType,
                                           const Length&,
                                           const NGConstraintSpace&);

// Convert a block-axis length to a layout unit using the given constraint
// space.
CORE_EXPORT LayoutUnit resolveBlockLength(LengthResolveType,
                                          const Length&,
                                          const NGConstraintSpace&,
                                          LayoutUnit contentContribution);

// Resolves the given length to a layout unit, constraining it by the min
// logical width and max logical width properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit computeInlineSizeForFragment(const NGConstraintSpace&,
                                                    const ComputedStyle&);

// Resolves the given length to a layout unit, constraining it by the min
// logical height and max logical height properties from the ComputedStyle
// object.
CORE_EXPORT LayoutUnit
computeBlockSizeForFragment(const NGConstraintSpace&,
                            const ComputedStyle& style,
                            LayoutUnit contentContribution);

}  // namespace blink

#endif  // NGLengthUtils_h
