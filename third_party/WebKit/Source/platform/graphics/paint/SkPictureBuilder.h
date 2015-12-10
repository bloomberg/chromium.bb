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
    STACK_ALLOCATED();
public:
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

    PassRefPtr<const SkPicture> endRecording()
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
