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
    // |disconnected| is optional output parameter having true if specified
    // positions don't have common ancestor.
    static short comparePositions(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected = nullptr);
    static bool isEmptyNonEditableNodeInEditable(const Node*);
    static bool editingIgnoresContent(const Node*);
    static int lastOffsetForEditing(const Node*);
};

// DOM tree version of editing algorithm
class EditingStrategy : public EditingAlgorithm<NodeTraversal> {
public:
    using PositionIteratorType = PositionIteratorAlgorithm<EditingStrategy>;
    using PositionType = PositionAlgorithm<EditingStrategy>;
};

// Composed tree version of editing algorithm
class EditingInComposedTreeStrategy : public EditingAlgorithm<ComposedTreeTraversal> {
public:
    using PositionIteratorType = PositionIteratorAlgorithm<EditingInComposedTreeStrategy>;
    using PositionType = PositionAlgorithm<EditingInComposedTreeStrategy>;

    // Returns true if the given node is considered as selection boundary.
    static bool isSelectionBoundaryShadowHost(const Node&);

    // Don't use |parentOrShadowHostNode()| in composed tree specific algorithm.
    // This function is provided here for sharing algorithm with
    // |TextIteratorAlgorithm|, which handles shadow tree within in
    // DOM traversal.
    static ContainerNode* parentOrShadowHostNode(const Node&);
};

extern template class EditingAlgorithm<NodeTraversal>;
extern template class EditingAlgorithm<ComposedTreeTraversal>;

} // namespace blink

#endif // EditingStrategy_h
