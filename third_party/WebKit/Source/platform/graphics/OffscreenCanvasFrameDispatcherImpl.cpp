// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/returned_resource.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace blink {

OffscreenCanvasFrameDispatcherImpl::OffscreenCanvasFrameDispatcherImpl(
    uint32_t clientId,
    uint32_t localId,
    uint64_t nonce,
    int width,
    int height)
    : m_surfaceId(cc::SurfaceId(cc::FrameSinkId(clientId, 0 /* sink_id */),
                                localId,
                                nonce)),
      m_width(width),
      m_height(height),
      m_nextResourceId(1u),
      m_binding(this) {
  DCHECK(!m_sink.is_bound());
  mojom::blink::OffscreenCanvasCompositorFrameSinkProviderPtr provider;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::GetProxy(&provider));
  provider->CreateCompositorFrameSink(m_surfaceId,
                                      m_binding.CreateInterfacePtrAndBind(),
                                      mojo::GetProxy(&m_sink));
}

void OffscreenCanvasFrameDispatcherImpl::dispatchFrame(
    RefPtr<StaticBitmapImage> image) {
  if (!image)
    return;
  if (!verifyImageSize(image->imageForCurrentFrame()))
    return;
  cc::CompositorFrame frame;
  frame.metadata.device_scale_factor = 1.0f;
  frame.delegated_frame_data.reset(new cc::DelegatedFrameData);

  const gfx::Rect bounds(m_width, m_height);
  const cc::RenderPassId renderPassId(1, 1);
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetAll(renderPassId, bounds, bounds, gfx::Transform(), false);

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds.size(), bounds, bounds, false, 1.f,
              SkXfermode::kSrcOver_Mode, 0);

  if (!image->isTextureBacked() && !isMainThread()) {
    // TODO(xlai): Implement unaccelerated 2d canvas on worker.
    // See crbug.com/563858.
    // This is a temporary code that submits a solidColor frame.
    cc::SolidColorDrawQuad* quad =
        pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
    const bool forceAntialiasingOff = false;
    quad->SetNew(sqs, bounds, bounds, SK_ColorGREEN, forceAntialiasingOff);
  } else {
    cc::TransferableResource resource;
    resource.id = m_nextResourceId;
    resource.format = cc::ResourceFormat::RGBA_8888;
    // TODO(crbug.com/645590): filter should respect the image-rendering CSS property of associated canvas element.
    resource.filter = GL_LINEAR;
    resource.size = gfx::Size(m_width, m_height);
    if (!image->isTextureBacked()) {
      std::unique_ptr<cc::SharedBitmap> bitmap =
          Platform::current()->allocateSharedBitmap(IntSize(m_width, m_height));
      if (!bitmap)
        return;
      unsigned char* pixels = bitmap->pixels();
      DCHECK(pixels);
      SkImageInfo imageInfo =
          SkImageInfo::Make(m_width, m_height, kN32_SkColorType,
                            image->isPremultiplied() ? kPremul_SkAlphaType
                                                     : kUnpremul_SkAlphaType);
      // TODO(xlai): Optimize to avoid copying pixels. See crbug.com/651456.
      image->imageForCurrentFrame()->readPixels(imageInfo, pixels,
                                                imageInfo.minRowBytes(), 0, 0);
      resource.mailbox_holder.mailbox = bitmap->id();
      resource.is_software = true;

      // Hold ref to |bitmap|, to keep it alive until the browser ReturnResources.
      // It guarantees that the shared bitmap is not re-used or deleted.
      m_sharedBitmaps.add(getNextResourceIdAndIncrement(), std::move(bitmap));
    } else {
      image->ensureMailbox();
      resource.mailbox_holder = gpu::MailboxHolder(
          image->getMailbox(), image->getSyncToken(), GL_TEXTURE_2D);
      resource.read_lock_fences_enabled = false;
      resource.is_software = false;

      // Hold ref to |image|, to keep it alive until the browser ReturnResources.
      // It guarantees that the resource is not re-used or deleted.
      m_cachedImages.add(getNextResourceIdAndIncrement(), std::move(image));
    }
    // TODO(crbug.com/646022): making this overlay-able.
    resource.is_overlay_candidate = false;

    frame.delegated_frame_data->resource_list.push_back(std::move(resource));

    cc::TextureDrawQuad* quad =
        pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
    gfx::Size rectSize(m_width, m_height);

    const bool needsBlending = true;
    // TOOD(crbug.com/645993): this should be inherited from WebGL context's creation settings.
    const bool premultipliedAlpha = true;
    const gfx::PointF uvTopLeft(0.f, 0.f);
    const gfx::PointF uvBottomRight(1.f, 1.f);
    float vertexOpacity[4] = {1.f, 1.f, 1.f, 1.f};
    const bool yflipped = false;
    // TODO(crbug.com/645994): this should be true when using style "image-rendering: pixelated".
    const bool nearestNeighbor = false;
    quad->SetAll(sqs, bounds, bounds, bounds, needsBlending, resource.id,
                 gfx::Size(), premultipliedAlpha, uvTopLeft, uvBottomRight,
                 SK_ColorTRANSPARENT, vertexOpacity, yflipped, nearestNeighbor,
                 false);
  }

  frame.delegated_frame_data->render_pass_list.push_back(std::move(pass));

  m_sink->SubmitCompositorFrame(std::move(frame), base::Closure());
}

void OffscreenCanvasFrameDispatcherImpl::ReturnResources(
    Vector<cc::mojom::blink::ReturnedResourcePtr> resources) {
  for (const auto& resource : resources) {
    m_cachedImages.remove(resource->id);
    m_sharedBitmaps.remove(resource->id);
  }
}

bool OffscreenCanvasFrameDispatcherImpl::verifyImageSize(
    const sk_sp<SkImage>& image) {
  if (image && image->width() == m_width && image->height() == m_height)
    return true;
  return false;
}

}  // namespace blink
