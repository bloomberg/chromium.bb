// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingPropertyUpdater_h
#define CompositingPropertyUpdater_h

#include "core/rendering/RenderGeometryMap.h"

namespace WebCore {

class RenderLayer;

class CompositingPropertyUpdater {
private:
    struct AncestorInfo {
        AncestorInfo()
            : enclosingCompositedLayer(0)
            , ancestorScrollingLayer(0)
        {
        }

        RenderLayer* enclosingCompositedLayer;
        RenderLayer* ancestorScrollingLayer;
    };

public:
    explicit CompositingPropertyUpdater(RenderLayer* rootRenderLayer);
    ~CompositingPropertyUpdater();

    enum UpdateType {
        DoNotForceUpdate,
        ForceUpdate,
    };

    void updateAncestorDependentProperties(RenderLayer*, UpdateType, AncestorInfo = AncestorInfo());

#if !ASSERT_DISABLED
    static void assertNeedsToUpdateAncestorDependantPropertiesBitsCleared(RenderLayer*);
#endif

private:
    RenderGeometryMap m_geometryMap;
    RenderLayer* m_rootRenderLayer;
};

} // namespace WebCore

#endif // CompositingPropertyUpdater_h
