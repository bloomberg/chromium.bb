// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingInputsUpdater_h
#define CompositingInputsUpdater_h

#include "core/layout/LayoutGeometryMap.h"

namespace blink {

class Layer;

class CompositingInputsUpdater {
public:
    explicit CompositingInputsUpdater(Layer* rootLayer);
    ~CompositingInputsUpdater();

    void update();

#if ENABLE(ASSERT)
    static void assertNeedsCompositingInputsUpdateBitsCleared(Layer*);
#endif

private:
    enum UpdateType {
        DoNotForceUpdate,
        ForceUpdate,
    };

    struct AncestorInfo {
        AncestorInfo()
            : ancestorStackingContext(0)
            , enclosingCompositedLayer(0)
            , lastScrollingAncestor(0)
            , hasAncestorWithClipOrOverflowClip(false)
            , hasAncestorWithClipPath(false)
        {
        }

        Layer* ancestorStackingContext;
        Layer* enclosingCompositedLayer;
        // Notice that lastScrollingAncestor isn't the same thing as
        // ancestorScrollingLayer. The former is just the nearest scrolling
        // along the Layer::parent() chain. The latter is the layer that
        // actually controls the scrolling of this layer, which we find on the
        // containing block chain.
        Layer* lastScrollingAncestor;
        bool hasAncestorWithClipOrOverflowClip;
        bool hasAncestorWithClipPath;
    };

    void updateRecursive(Layer*, UpdateType, AncestorInfo);

    LayoutGeometryMap m_geometryMap;
    Layer* m_rootLayer;
};

} // namespace blink

#endif // CompositingInputsUpdater_h
