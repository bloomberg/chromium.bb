// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_rgba_to_yuva_converter.h"

#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "media/renderers/video_frame_yuv_converter.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace {

// Given a gpu::MailboxHolder and a viz::RasterContextProvider, create scoped
// access to the texture as an SkImage.
class ScopedAcceleratedSkImage {
 public:
  static std::unique_ptr<ScopedAcceleratedSkImage> Create(
      viz::RasterContextProvider* provider,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      const gpu::MailboxHolder& mailbox_holder) {
    auto* ri = provider->RasterInterface();
    DCHECK(ri);
    GrDirectContext* gr_context = provider->GrContext();
    DCHECK(gr_context);

    if (!mailbox_holder.mailbox.IsSharedImage()) {
      DLOG(ERROR) << "Cannot created SkImage for non-SharedImage mailbox.";
      return nullptr;
    }

    ri->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());

    uint32_t texture_id =
        ri->CreateAndConsumeForGpuRaster(mailbox_holder.mailbox);
    if (!texture_id) {
      DLOG(ERROR) << "Failed to create texture for mailbox.";
      return nullptr;
    }
    ri->BeginSharedImageAccessDirectCHROMIUM(
        texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

    GrGLTextureInfo gl_info = {
        mailbox_holder.texture_target,
        texture_id,
        viz::TextureStorageFormat(format),
    };
    GrBackendTexture backend_texture(size.width(), size.height(),
                                     GrMipmapped::kNo, gl_info);

    SkColorType color_type = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format);
    sk_sp<SkImage> sk_image = SkImage::MakeFromTexture(
        gr_context, backend_texture, surface_origin, color_type,
        kOpaque_SkAlphaType, color_space.ToSkColorSpace());
    if (!sk_image) {
      DLOG(ERROR) << "Failed to SkImage for StaticBitmapImage.";
      ri->EndSharedImageAccessDirectCHROMIUM(texture_id);
      ri->DeleteGpuRasterTexture(texture_id);
      return nullptr;
    }

    return absl::WrapUnique<ScopedAcceleratedSkImage>(
        new ScopedAcceleratedSkImage(provider, texture_id,
                                     std::move(sk_image)));
  }

  ~ScopedAcceleratedSkImage() {
    auto* ri = provider_->RasterInterface();
    DCHECK(ri);
    GrDirectContext* gr_context = provider_->GrContext();
    DCHECK(gr_context);

    sk_image_ = nullptr;
    if (texture_id_) {
      ri->EndSharedImageAccessDirectCHROMIUM(texture_id_);
      ri->DeleteGpuRasterTexture(texture_id_);
    }
  }

  sk_sp<SkImage> sk_image() { return sk_image_; }

 private:
  ScopedAcceleratedSkImage(viz::RasterContextProvider* provider,
                           uint32_t texture_id,
                           sk_sp<SkImage> sk_image)
      : provider_(provider), texture_id_(texture_id), sk_image_(sk_image) {}

  viz::RasterContextProvider* const provider_;
  uint32_t texture_id_ = 0;
  sk_sp<SkImage> sk_image_;
};

}  // namespace

namespace media {

bool CopyRGBATextureToVideoFrame(viz::RasterContextProvider* provider,
                                 viz::ResourceFormat src_format,
                                 const gfx::Size& src_size,
                                 const gfx::ColorSpace& src_color_space,
                                 GrSurfaceOrigin src_surface_origin,
                                 const gpu::MailboxHolder& src_mailbox_holder,
                                 VideoFrame* dst_video_frame,
                                 gpu::SyncToken& completion_sync_token) {
  auto* ri = provider->RasterInterface();
  DCHECK(ri);

  // Create an accelerated SkImage for the source.
  auto scoped_sk_image = ScopedAcceleratedSkImage::Create(
      provider, src_format, src_size, src_color_space, src_surface_origin,
      src_mailbox_holder);
  if (!scoped_sk_image) {
    DLOG(ERROR)
        << "Failed to create accelerated SkImage for RGBA to YUVA conversion.";
    return false;
  }

  // Create SkSurfaces for the destination planes.
  sk_sp<SkSurface> sk_surfaces[SkYUVAInfo::kMaxPlanes];
  VideoFrameYUVMailboxesHolder holder;
  if (!holder.VideoFrameToPlaneSkSurfaces(dst_video_frame, provider,
                                          sk_surfaces)) {
    DLOG(ERROR) << "Failed to create SkSurfaces for VideoFrame.";
    return false;
  }

  // Make GrContext wait for `dst_video_frame`.
  ri->Flush();
  WaitAndReplaceSyncTokenClient client(ri);
  dst_video_frame->UpdateReleaseSyncToken(&client);

  // Do the blit.
  skia::BlitRGBAToYUVA(
      scoped_sk_image->sk_image(),
      SkRect::MakeWH(src_size.width(), src_size.height()), sk_surfaces,
      holder.yuva_info(),
      SkRect::MakeWH(holder.yuva_info().width(), holder.yuva_info().height()));
  provider->GrContext()->flushAndSubmit(false);
  ri->Flush();

  // Set `completion_sync_token` to mark the completion of the copy.
  ri->GenUnverifiedSyncTokenCHROMIUM(completion_sync_token.GetData());

  // Make `dst_video_frame` wait on the token.
  SimpleSyncTokenClient simple_client(completion_sync_token);
  dst_video_frame->UpdateReleaseSyncToken(&simple_client);
  return true;
}

}  // namespace media
