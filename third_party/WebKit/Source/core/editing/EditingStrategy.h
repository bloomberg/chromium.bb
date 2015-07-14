// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingStrategy_h
#define EditingStrategy_h

#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ComposedTreeTraversal.h"

namespace blink {

template <typename Strategy>
class PositionAlgorithm;

template <typename Strategy>
class PositionIteratorAlgorithm;

// Editing algorithm defined on node traversal.
template <typename Traversal>
class EditingAlgorithm : public Traversal {
public:
    static bool isEmptyNonEditableNodeInEditable(const Node*);
    static bool editingIgnoresContent(const Node*);
    static int lastOffsetForEditing(const Node*);
};

extern template class EditingAlgorithm<NodeTraversal>;
extern template class EditingAlgorithm<ComposedTreeTraversal>;

// DOM tree version of editing algorithm
using EditingStrategy = EditingAlgorithm<NodeTraversal>;
// Composed tree version of editing algorithm
using EditingInComposedTreeStrategy = EditingAlgorithm<ComposedTreeTraversal>;

} // namespace blink

#endif // EditingStrategy_h
