// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextIteratorStrategy_h
#define TextIteratorStrategy_h

#include "core/editing/EditingStrategy.h"

namespace blink {

class Position;

class TextIteratorStrategy : public EditingStrategy {
public:
    static Node* pastLastNode(const Node&, int);
    static ContainerNode* parentOrShadowHostNode(const Node&);
    static unsigned depthCrossingShadowBoundaries(const Node&);
    static int shadowDepthOf(const Node&, const Node&);

    static PositionType createLegacyEditingPosition(Node*, unsigned);
};

} // namespace blink

#endif // TextIteratorStrategy_h
