// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_skia_output_surface.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/gl_utils.h"

namespace viz {

FakeSkiaOutputSurface::FakeSkiaOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)), weak_ptr_factory_(this) {}

FakeSkiaOutputSurface::~FakeSkiaOutputSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void FakeSkiaOutputSurface::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void FakeSkiaOutputSurface::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::Reshape(const gfx::Size& size,
                                    float device_scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha,
                                    bool use_stencil) {
  auto& sk_surface = sk_surfaces_[0];
  SkColorType color_type = kRGBA_8888_SkColorType;
  SkImageInfo image_info =
      SkImageInfo::Make(size.width(), size.height(), color_type,
                        kPremul_SkAlphaType, color_space.ToSkColorSpace());
  sk_surface =
      SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);

  DCHECK(sk_surface);
}

void FakeSkiaOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  NOTIMPLEMENTED();
}

uint32_t FakeSkiaOutputSurface::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GL_RGB;
}

OverlayCandidateValidator* FakeSkiaOutputSurface::GetOverlayCandidateValidator()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return nullptr;
}

bool FakeSkiaOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned FakeSkiaOutputSurface::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return 0;
}

gfx::BufferFormat FakeSkiaOutputSurface::GetOverlayBufferFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return gfx::BufferFormat::RGBX_8888;
}

bool FakeSkiaOutputSurface::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

void FakeSkiaOutputSurface::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

unsigned FakeSkiaOutputSurface::UpdateGpuFence() {
  NOTIMPLEMENTED();
  return 0;
}

void FakeSkiaOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  NOTIMPLEMENTED();
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto& sk_surface = sk_surfaces_[0];
  DCHECK(sk_surface);
  DCHECK_EQ(current_render_pass_id_, 0u);
  return sk_surface->getCanvas();
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImage(
    ResourceMetadata metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(metadata, &backend_texture)) {
    DLOG(ERROR) << "Failed to GetGrBackendTexture from mailbox.";
    return nullptr;
  }

  auto sk_color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, metadata.resource_format);
  return SkImage::MakeFromTexture(
      gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin, sk_color_type,
      metadata.alpha_type, metadata.color_space.ToSkColorSpace());
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImageFromYUV(
    std::vector<ResourceMetadata> metadatas,
    SkYUVColorSpace yuv_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

gpu::SyncToken FakeSkiaOutputSurface::ReleasePromiseSkImages(
    std::vector<sk_sp<SkImage>> images) {
  gpu::SyncToken sync_token;
  if (images.empty())
    return sync_token;
  images.clear();
  context_provider()->ContextGL()->GenSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

void FakeSkiaOutputSurface::SkiaSwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeSkiaOutputSurface::SwapBuffersAck,
                                weak_ptr_factory_.GetWeakPtr()));
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK_EQ(current_render_pass_id_, 0u);
  auto& sk_surface = sk_surfaces_[id];

  if (!sk_surface) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    SkImageInfo image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    sk_surface =
        SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);
  }
  return sk_surface->getCanvas();
}

gpu::SyncToken FakeSkiaOutputSurface::SubmitPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sk_surfaces_[current_render_pass_id_]->flush();
  current_render_pass_id_ = 0;

  gpu::SyncToken sync_token;
  context_provider()->ContextGL()->GenSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sk_surfaces_.find(id);
  DCHECK(it != sk_surfaces_.end());
  return it->second->makeImageSnapshot();
}

void FakeSkiaOutputSurface::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  for (const auto& id : ids) {
    auto it = sk_surfaces_.find(id);
    DCHECK(it != sk_surfaces_.end());
    sk_surfaces_.erase(it);
  }
}

void FakeSkiaOutputSurface::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(sk_surfaces_.find(id) != sk_surfaces_.end());
  auto* surface = sk_surfaces_[id].get();
  if (request->result_format() != CopyOutputResult::Format::RGBA_BITMAP ||
      request->is_scaled() ||
      geometry.result_bounds != geometry.result_selection) {
    // TODO(crbug.com/644851): Complete the implementation for all request
    // types, scaling, etc.
    NOTIMPLEMENTED();
    return;
  }
  auto copy_image = surface->makeImageSnapshot()->makeSubset(
      RectToSkIRect(geometry.sampling_bounds));
  // Send copy request by copying into a bitmap.
  SkBitmap bitmap;
  copy_image->asLegacyBitmap(&bitmap);
  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      geometry.result_bounds, bitmap));
}

void FakeSkiaOutputSurface::AddContextLostObserver(
    ContextLostObserver* observer) {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  NOTIMPLEMENTED();
}

bool FakeSkiaOutputSurface::GetGrBackendTexture(
    const ResourceMetadata& metadata,
    GrBackendTexture* backend_texture) {
  DCHECK(!metadata.mailbox_holder.mailbox.IsZero());

  auto* gl = context_provider()->ContextGL();
  gl->WaitSyncTokenCHROMIUM(metadata.mailbox_holder.sync_token.GetConstData());
  auto texture_id =
      gl->CreateAndConsumeTextureCHROMIUM(metadata.mailbox_holder.mailbox.name);
  auto gl_format = TextureStorageFormat(metadata.resource_format);
  GrGLTextureInfo gl_texture_info = {metadata.mailbox_holder.texture_target,
                                     texture_id, gl_format};
  *backend_texture =
      GrBackendTexture(metadata.size.width(), metadata.size.height(),
                       GrMipMapped::kNo, gl_texture_info);
  return true;
}

void FakeSkiaOutputSurface::SwapBuffersAck() {
  client_->DidReceiveSwapBuffersAck();
  client_->DidReceivePresentationFeedback(
      {base::TimeTicks::Now(), base::TimeDelta(), 0});
}

}  // namespace viz
