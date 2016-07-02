// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyTreeState_h
#define PropertyTreeState_h

#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace blink {

class TransformPaintPropertyNode;
class ClipPaintPropertyNode;
class EffectPaintPropertyNode;

// Represents the combination of transform, clip and effect nodes for a particular coordinate space.
// See GeometryMapper.
struct PropertyTreeState {
    PropertyTreeState() : PropertyTreeState(nullptr, nullptr, nullptr) {}

    PropertyTreeState(
        TransformPaintPropertyNode* transform,
        ClipPaintPropertyNode* clip,
        EffectPaintPropertyNode* effect)
    : transform(transform), clip(clip), effect(effect) {}

    RefPtr<TransformPaintPropertyNode> transform;
    RefPtr<ClipPaintPropertyNode> clip;
    RefPtr<EffectPaintPropertyNode> effect;
};

template <class A>
unsigned propertyTreeNodeDepth(const A* node)
{
    unsigned depth = 0;
    while (node) {
        depth++;
        node = node->parent();
    }
    return depth;
}

template <class A>
const A* propertyTreeNearestCommonAncestor(const A* a, const A* b)
{
    // Measure both depths.
    unsigned depthA = propertyTreeNodeDepth<A>(a);
    unsigned depthB = propertyTreeNodeDepth<A>(b);

    // Make it so depthA >= depthB.
    if (depthA < depthB) {
        std::swap(a, b);
        std::swap(depthA, depthB);
    }

    // Make it so depthA == depthB.
    while (depthA > depthB) {
        a = a->parent();
        depthA--;
    }

    // Walk up until we find the ancestor.
    while (a != b) {
        a = a->parent();
        b = b->parent();
    }
    return a;
}

} // namespace blink

#endif // PropertyTreeState_h
