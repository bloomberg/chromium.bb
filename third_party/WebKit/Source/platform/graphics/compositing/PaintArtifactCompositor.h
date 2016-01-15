// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintArtifactCompositor_h
#define PaintArtifactCompositor_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace cc {
class Layer;
}

namespace blink {

class PaintArtifact;
class WebLayer;

// Responsible for managing compositing in terms of a PaintArtifact.
//
// Owns a subtree of the compositor layer tree, and updates it in response to
// changes in the paint artifact.
//
// PaintArtifactCompositor is the successor to PaintLayerCompositor, reflecting
// the new home of compositing decisions after paint in Slimming Paint v2.
class PLATFORM_EXPORT PaintArtifactCompositor {
    WTF_MAKE_NONCOPYABLE(PaintArtifactCompositor);
public:
    PaintArtifactCompositor();
    ~PaintArtifactCompositor();

    // Creates at least a root layer to be managed by this controller. Can't be
    // done in the constructor, since RuntimeEnabledFeatures may not be
    // initialized yet.
    void initializeIfNeeded();

    // Updates the layer tree to match the provided paint artifact.
    // Creates the root layer if not already done.
    void update(const PaintArtifact&);

    // The root layer of the tree managed by this object.
    cc::Layer* rootLayer() const { return m_rootLayer.get(); }

    // Wraps rootLayer(), so that it can be attached as a child of another
    // WebLayer.
    WebLayer* webLayer() const { return m_webLayer.get(); }

private:
    class ContentLayerClientImpl;

    scoped_refptr<cc::Layer> m_rootLayer;
    OwnPtr<WebLayer> m_webLayer;
    Vector<OwnPtr<ContentLayerClientImpl>> m_contentLayerClients;

    // For ~PaintArtifactCompositor on MSVC.
    friend struct WTF::OwnedPtrDeleter<ContentLayerClientImpl>;
};

} // namespace blink

#endif // PaintArtifactCompositor_h
