// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EditingStrategy_h
#define EditingStrategy_h

#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ComposedTreeTraversal.h"

namespace blink {

template <typename Strategy>
class PositionIteratorAlgorithm;

class Position;

class EditingStrategy : public NodeTraversal {
public:
    using PositionType = Position;
    using PositionIteratorType = PositionIteratorAlgorithm<EditingStrategy>;

    // |disconnected| is optional output parameter having true if specified
    // positions don't have common ancestor.
    static short comparePositions(Node* containerA, int offsetA, Node* containerB, int offsetB, bool* disconnected = nullptr);
    static bool editingIgnoresContent(const Node*);
    static int lastOffsetForEditing(const Node*);
};

} // namespace blink

#endif // EditingStrategy_h
