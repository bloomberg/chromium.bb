// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include <memory>
#include <utility>

#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {
namespace {

void ReleaseCallbackOnContextThread(
    std::unique_ptr<viz::SingleReleaseCallback> callback,
    const gpu::SyncToken sync_token) {
  callback->Run(sync_token, /* is_lost = */ false);
}

}  // namespace

AcceleratedStaticBitmapImage::MailboxRef::MailboxRef(
    const gpu::SyncToken& sync_token,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : sync_token_(sync_token),
      context_thread_ref_(context_thread_ref),
      context_task_runner_(std::move(context_task_runner)),
      release_callback_(std::move(release_callback)) {
  DCHECK(!is_cross_thread() || sync_token_.verified_flush());
}

AcceleratedStaticBitmapImage::MailboxRef::~MailboxRef() {
  if (context_thread_ref_ == base::PlatformThread::CurrentRef()) {
    ReleaseCallbackOnContextThread(std::move(release_callback_), sync_token_);
  } else {
    context_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReleaseCallbackOnContextThread,
                                  std::move(release_callback_), sync_token_));
  }
}

const gpu::SyncToken&
AcceleratedStaticBitmapImage::MailboxRef::GetOrCreateSyncToken(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (!sync_token_.HasData()) {
    DCHECK(!is_cross_thread());
    DCHECK(context_provider_wrapper);
    context_provider_wrapper->ContextProvider()
        ->InterfaceBase()
        ->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
  }
  return sync_token_;
}

// static
void AcceleratedStaticBitmapImage::ReleaseTexture(void* ctx) {
  auto* release_ctx = static_cast<ReleaseContext*>(ctx);
  if (release_ctx->context_provider_wrapper) {
    if (release_ctx->texture_id) {
      auto* ri = release_ctx->context_provider_wrapper->ContextProvider()
                     ->RasterInterface();
      ri->EndSharedImageAccessDirectCHROMIUM(release_ctx->texture_id);
      ri->DeleteGpuRasterTexture(release_ctx->texture_id);
    }
  }

  delete release_ctx;
}

// static
scoped_refptr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  return base::AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, shared_image_texture_id, sk_image_info,
      texture_target, is_origin_top_left, kDefaultImageOrientation,
      std::move(context_provider_wrapper), context_thread_ref,
      std::move(context_task_runner), std::move(release_callback)));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    GLuint shared_image_texture_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left,
    const ImageOrientation& orientation,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : StaticBitmapImage(orientation),
      mailbox_(mailbox),
      sk_image_info_(sk_image_info),
      texture_target_(texture_target),
      is_origin_top_left_(is_origin_top_left),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      mailbox_ref_(
          base::MakeRefCounted<MailboxRef>(sync_token,
                                           context_thread_ref,
                                           std::move(context_task_runner),
                                           std::move(release_callback))),
      paint_image_content_id_(cc::PaintImage::GetNextContentId()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(mailbox_.IsSharedImage());

  if (shared_image_texture_id)
    InitializeTextureBacking(shared_image_texture_id);
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

IntSize AcceleratedStaticBitmapImage::Size() const {
  return IntSize(sk_image_info_.width(), sk_image_info_.height());
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::MakeUnaccelerated() {
  CreateImageFromMailboxIfNeeded();
  return UnacceleratedStaticBitmapImage::Create(
      texture_backing_->GetAcceleratedSkImage()->makeNonTextureImage(),
      orientation_);
}

bool AcceleratedStaticBitmapImage::CopyToTexture(
    gpu::gles2::GLES2Interface* dest_gl,
    GLenum dest_target,
    GLuint dest_texture_id,
    GLint dest_level,
    bool unpack_premultiply_alpha,
    bool unpack_flip_y,
    const IntPoint& dest_point,
    const IntRect& source_sub_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return false;

  // This method should only be used for cross-context copying, otherwise it's
  // wasting overhead.
  DCHECK(mailbox_ref_->is_cross_thread() ||
         dest_gl != ContextProvider()->ContextGL());
  DCHECK(mailbox_.IsSharedImage());

  // Get a texture id that |destProvider| knows about and copy from it.
  dest_gl->WaitSyncTokenCHROMIUM(
      mailbox_ref_->GetOrCreateSyncToken(ContextProviderWrapper())
          .GetConstData());
  GLuint source_texture_id =
      dest_gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox_.name);
  dest_gl->BeginSharedImageAccessDirectCHROMIUM(
      source_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  dest_gl->CopySubTextureCHROMIUM(
      source_texture_id, 0, dest_target, dest_texture_id, dest_level,
      dest_point.X(), dest_point.Y(), source_sub_rectangle.X(),
      source_sub_rectangle.Y(), source_sub_rectangle.Width(),
      source_sub_rectangle.Height(), unpack_flip_y ? GL_FALSE : GL_TRUE,
      GL_FALSE, unpack_premultiply_alpha ? GL_FALSE : GL_TRUE);
  dest_gl->EndSharedImageAccessDirectCHROMIUM(source_texture_id);
  dest_gl->DeleteTextures(1, &source_texture_id);

  // We need to update the texture holder's sync token to ensure that when this
  // mailbox is recycled or deleted, it is done after the copy operation above.
  gpu::SyncToken sync_token;
  dest_gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  mailbox_ref_->set_sync_token(sync_token);

  return true;
}

PaintImage AcceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsValid())
    return PaintImage();

  CreateImageFromMailboxIfNeeded();

  return CreatePaintImageBuilder()
      .set_texture_backing(texture_backing_, paint_image_content_id_)
      .set_completion_state(PaintImage::CompletionState::DONE)
      .TakePaintImage();
}

void AcceleratedStaticBitmapImage::Draw(
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode image_clamping_mode,
    ImageDecodingMode decode_mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return;
  auto paint_image_decoding_mode = ToPaintImageDecodingMode(decode_mode);
  if (paint_image.decoding_mode() != paint_image_decoding_mode) {
    paint_image = PaintImageBuilder::WithCopy(std::move(paint_image))
                      .set_decoding_mode(paint_image_decoding_mode)
                      .TakePaintImage();
  }
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect,
                                image_clamping_mode,
                                should_respect_image_orientation, paint_image);
}

bool AcceleratedStaticBitmapImage::IsValid() const {
  if (texture_backing_ && (!skia_context_provider_wrapper_ ||
                           !texture_backing_->GetAcceleratedSkImage()->isValid(
                               ContextProvider()->GetGrContext()))) {
    return false;
  }

  if (mailbox_ref_->is_cross_thread()) {
    // If context is is from another thread, validity cannot be verified. Just
    // assume valid. Potential problem will be detected later.
    return true;
  }

  return !!context_provider_wrapper_;
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  auto context = ContextProviderWrapper();
  return context ? context->ContextProvider() : nullptr;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
AcceleratedStaticBitmapImage::ContextProviderWrapper() const {
  return texture_backing_ ? skia_context_provider_wrapper_
                          : context_provider_wrapper_;
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_backing_)
    return;
  InitializeTextureBacking(0u);
}

void AcceleratedStaticBitmapImage::InitializeTextureBacking(
    GLuint shared_image_texture_id) {
  DCHECK(!shared_image_texture_id || !mailbox_ref_->is_cross_thread());

  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  gpu::raster::RasterInterface* shared_ri =
      context_provider_wrapper->ContextProvider()->RasterInterface();
  GrContext* shared_gr_context =
      context_provider_wrapper->ContextProvider()->GetGrContext();
  DCHECK(shared_ri &&
         shared_gr_context);  // context isValid already checked in callers

  GLuint shared_context_texture_id = 0u;
  bool should_delete_texture_on_release = true;

  if (shared_image_texture_id) {
    shared_context_texture_id = shared_image_texture_id;
    should_delete_texture_on_release = false;
  } else {
    shared_ri->WaitSyncTokenCHROMIUM(
        mailbox_ref_->GetOrCreateSyncToken(context_provider_wrapper)
            .GetConstData());
    shared_context_texture_id =
        shared_ri->CreateAndConsumeForGpuRaster(mailbox_);
    shared_ri->BeginSharedImageAccessDirectCHROMIUM(
        shared_context_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  }

  GrGLTextureInfo texture_info;
  texture_info.fTarget = texture_target_;
  texture_info.fID = shared_context_texture_id;
  texture_info.fFormat =
      CanvasColorParams(sk_image_info_).GLSizedInternalFormat();
  GrBackendTexture backend_texture(sk_image_info_.width(),
                                   sk_image_info_.height(), GrMipMapped::kNo,
                                   texture_info);

  GrSurfaceOrigin origin = IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin;

  auto* release_ctx = new ReleaseContext;
  release_ctx->mailbox_ref = mailbox_ref_;
  if (should_delete_texture_on_release)
    release_ctx->texture_id = shared_context_texture_id;
  release_ctx->context_provider_wrapper = context_provider_wrapper;

  sk_sp<SkImage> sk_image = SkImage::MakeFromTexture(
      shared_gr_context, backend_texture, origin, sk_image_info_.colorType(),
      sk_image_info_.alphaType(), sk_image_info_.refColorSpace(),
      &ReleaseTexture, release_ctx);

  if (!sk_image) {
    ReleaseTexture(release_ctx);
  } else {
    skia_context_provider_wrapper_ = std::move(context_provider_wrapper);
    texture_backing_ = sk_sp<MailboxTextureBacking>(
        new MailboxTextureBacking(std::move(sk_image)));
  }
}

void AcceleratedStaticBitmapImage::EnsureSyncTokenVerified() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (mailbox_ref_->verified_flush())
    return;

  if (mailbox_ref_->is_cross_thread()) {
    // Was originally created on another thread. Should already have a sync
    // token from the original source context, already verified if needed.
    NOTREACHED() << "Cross-thread SyncToken should already be verified.";
    return;
  }

  if (!ContextProviderWrapper())
    return;

  auto sync_token =
      mailbox_ref_->GetOrCreateSyncToken(ContextProviderWrapper());
  int8_t* token_data = sync_token.GetData();
  ContextProvider()->InterfaceBase()->VerifySyncTokensCHROMIUM(&token_data, 1);
  sync_token.SetVerifyFlush();
  mailbox_ref_->set_sync_token(sync_token);
}

gpu::MailboxHolder AcceleratedStaticBitmapImage::GetMailboxHolder() const {
  if (!IsValid())
    return gpu::MailboxHolder();

  return gpu::MailboxHolder(
      mailbox_, mailbox_ref_->GetOrCreateSyncToken(ContextProviderWrapper()),
      texture_target_);
}

void AcceleratedStaticBitmapImage::Transfer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureSyncTokenVerified();

  // SkImage is bound to the current thread so is no longer valid to use
  // cross-thread.
  texture_backing_.reset();

  DETACH_FROM_THREAD(thread_checker_);
}

bool AcceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
  return sk_image_info_.isOpaque();
}

scoped_refptr<StaticBitmapImage>
AcceleratedStaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> color_space,
    SkColorType color_type) {
  DCHECK(color_space);
  DCHECK(color_type == kRGBA_F16_SkColorType ||
         color_type == kRGBA_8888_SkColorType);

  if (!ContextProviderWrapper())
    return nullptr;

  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSkImage();
  if (SkColorSpace::Equals(color_space.get(), skia_image->colorSpace()) &&
      color_type == skia_image->colorType()) {
    return this;
  }

  auto image_info = skia_image->imageInfo()
                        .makeColorSpace(color_space)
                        .makeColorType(color_type);

  auto usage_flags = ContextProviderWrapper()
                         ->ContextProvider()
                         ->SharedImageInterface()
                         ->UsageForMailbox(mailbox_);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      Size(), ContextProviderWrapper(), kLow_SkFilterQuality,
      CanvasColorParams(image_info), IsOriginTopLeft(),
      CanvasResourceProvider::RasterMode::kGPU, usage_flags);
  if (!provider) {
    return nullptr;
  }

  provider->Canvas()->drawImage(PaintImageForCurrentFrame(), 0, 0, nullptr);
  return provider->Snapshot(orientation_);
}

}  // namespace blink
