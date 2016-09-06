// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcherImpl_h
#define OffscreenCanvasFrameDispatcherImpl_h

#include "cc/surfaces/surface_id.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcherImpl final : public OffscreenCanvasFrameDispatcher {
public:
    explicit OffscreenCanvasFrameDispatcherImpl(uint32_t clientId, uint32_t localId, uint64_t nonce, int width, int height);

    // OffscreenCanvasFrameDispatcher implementation.
    ~OffscreenCanvasFrameDispatcherImpl() override {}
    void dispatchFrame() override;

private:
    const cc::SurfaceId m_surfaceId;
    const int m_width;
    const int m_height;
    mojom::blink::OffscreenCanvasFrameReceiverPtr m_service;
};

} // namespace blink

#endif // OffscreenCanvasFrameDispatcherImpl_h
