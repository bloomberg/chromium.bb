// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionAdjuster_h
#define SelectionAdjuster_h

#include "core/editing/VisibleSelection.h"
#include "wtf/Allocator.h"

namespace blink {

// |SelectionAdjuster| adjusts positions in |VisibleSelection| directly without
// calling |validate()|. Users of |SelectionAdjuster| should keep invariant of
// |VisibleSelection|, e.g. all positions are canonicalized.
class CORE_EXPORT SelectionAdjuster final {
    STATIC_ONLY(SelectionAdjuster);
public:
    static void adjustSelectionInComposedTree(VisibleSelectionInComposedTree*, const VisibleSelection&);
    static void adjustSelectionInDOMTree(VisibleSelection*, const VisibleSelectionInComposedTree&);
    static void adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelection*);
    static void adjustSelectionToAvoidCrossingShadowBoundaries(VisibleSelectionInComposedTree*);
};

} // namespace blink

#endif // SelectionAdjuster_h
