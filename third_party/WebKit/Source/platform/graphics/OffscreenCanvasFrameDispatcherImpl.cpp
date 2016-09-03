// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace blink {

OffscreenCanvasFrameDispatcherImpl::OffscreenCanvasFrameDispatcherImpl(uint32_t clientId, uint32_t localId, uint64_t nonce, int width, int height)
    : m_surfaceId(cc::SurfaceId(clientId, localId, nonce))
    , m_width(width)
    , m_height(height)
{
    DCHECK(!m_service.is_bound());
    Platform::current()->interfaceProvider()->getInterface(mojo::GetProxy(&m_service));
}

void OffscreenCanvasFrameDispatcherImpl::dispatchFrame()
{
    // TODO(563852/xlai): Currently this is just a simple solid-color compositor
    // frame. We need to update this function to extract the image data from canvas.
    cc::CompositorFrame frame;
    frame.metadata.device_scale_factor = 1.0f;
    frame.delegated_frame_data.reset(new cc::DelegatedFrameData);

    const gfx::Rect bounds(m_width, m_height);
    const cc::RenderPassId renderPassId(1, 1);
    std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
    pass->SetAll(renderPassId, bounds, bounds, gfx::Transform(), false);

    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->SetAll(gfx::Transform(), bounds.size(), bounds, bounds, false, 1.f, SkXfermode::kSrcOver_Mode, 0);

    cc::SolidColorDrawQuad* quad = pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
    const bool forceAntialiasingOff = false;
    quad->SetNew(sqs, bounds, bounds, SK_ColorGREEN, forceAntialiasingOff);

    frame.delegated_frame_data->render_pass_list.push_back(std::move(pass));

    m_service->SubmitCompositorFrame(m_surfaceId, std::move(frame));
}

} // namespace blink
