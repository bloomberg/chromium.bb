// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkPictureBuilder_h
#define SkPictureBuilder_h

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

namespace blink {

// When slimming paint ships we can remove this SkPicture abstraction and
// rely on PaintController here.
class SkPictureBuilder final {
    WTF_MAKE_NONCOPYABLE(SkPictureBuilder);
public:
    // Constructs a new builder with the given bounds for the resulting recorded picture. If
    // |metadata| is specified, that metadata is propagated to the builder's internal canvas. If
    // |containingContext| is specified, the device scale factor, printing, and disabled state are
    // propagated to the builder's internal context.
    SkPictureBuilder(const FloatRect& bounds, SkMetaData* metaData = nullptr, GraphicsContext* containingContext = nullptr)
        : m_bounds(bounds)
    {
        GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
        if (containingContext && containingContext->contextDisabled())
            disabledMode = GraphicsContext::FullyDisabled;

        m_paintController = PaintController::create();
        m_paintController->beginSkippingCache();
        m_context = adoptPtr(new GraphicsContext(*m_paintController, disabledMode, metaData));

        if (containingContext) {
            m_context->setDeviceScaleFactor(containingContext->deviceScaleFactor());
            m_context->setPrinting(containingContext->printing());
        }
    }

    GraphicsContext& context() { return *m_context; }

    // Returns a picture capturing all drawing performed on the builder's context since
    // construction, or nullptr if the picture could not be created.
    PassRefPtr<SkPicture> endRecording()
    {
        m_context->beginRecording(m_bounds);
        m_paintController->endSkippingCache();
        m_paintController->commitNewDisplayItems();
        m_paintController->paintArtifact().replay(*m_context);
        return m_context->endRecording();
    }

private:
    OwnPtr<PaintController> m_paintController;
    OwnPtr<GraphicsContext> m_context;
    FloatRect m_bounds;
};

} // namespace blink

#endif // SkPictureBuilder_h
