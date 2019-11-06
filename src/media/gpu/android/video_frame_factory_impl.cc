// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/shared_image_video.h"
#include "media/gpu/command_buffer_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

TextureOwner::Mode GetTextureOwnerMode(
    VideoFrameFactory::OverlayMode overlay_mode) {
  const bool a_image_reader_supported =
      base::android::AndroidImageReader::GetInstance().IsSupported();

  switch (overlay_mode) {
    case VideoFrameFactory::OverlayMode::kDontRequestPromotionHints:
    case VideoFrameFactory::OverlayMode::kRequestPromotionHints:
      return a_image_reader_supported && base::FeatureList::IsEnabled(
                                             media::kAImageReaderVideoOutput)
                 ? TextureOwner::Mode::kAImageReaderInsecure
                 : TextureOwner::Mode::kSurfaceTextureInsecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlSecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
    case VideoFrameFactory::OverlayMode::kSurfaceControlInsecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderInsecureSurfaceControl;
  }

  NOTREACHED();
  return TextureOwner::Mode::kSurfaceTextureInsecure;
}

scoped_refptr<gpu::SharedContextState> GetSharedContext(
    gpu::CommandBufferStub* stub,
    gpu::ContextResult* result) {
  auto shared_context =
      stub->channel()->gpu_channel_manager()->GetSharedContextState(result);
  if (*result != gpu::ContextResult::kSuccess)
    return nullptr;
  return shared_context;
}

void ContextStateResultUMA(gpu::ContextResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.GpuVideoFrameFactory.SharedContextStateResult", result);
}

}  // namespace

using gpu::gles2::AbstractTexture;

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb,
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gpu_video_frame_factory_)
    gpu_task_runner_->DeleteSoon(FROM_HERE, gpu_video_frame_factory_.release());
}

void VideoFrameFactoryImpl::Initialize(OverlayMode overlay_mode,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!gpu_video_frame_factory_);
  overlay_mode_ = overlay_mode;
  gpu_video_frame_factory_ = std::make_unique<GpuVideoFrameFactory>();
  base::PostTaskAndReplyWithResult(
      gpu_task_runner_.get(), FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::Initialize,
                 base::Unretained(gpu_video_frame_factory_.get()), overlay_mode,
                 get_stub_cb_),
      std::move(init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;
  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    texture_owner_ = nullptr;
  } else {
    // If |surface_bundle| is using a TextureOwner, then get it.
    texture_owner_ =
        surface_bundle->overlay ? nullptr : surface_bundle->texture_owner_;

    // Start a new image group.  Note that there's no reason that we can't have
    // more than one group per surface bundle; it's okay if we're called
    // mulitiple times with the same surface bundle.  It just helps to combine
    // the callbacks if we don't, especially since AndroidOverlay doesn't know
    // how to remove destruction callbacks.  That's one reason why we don't just
    // make the CodecImage register itself.  The other is that the threading is
    // easier if we do it this way, since the image group is constructed on the
    // proper thread to talk to the overlay.
    image_group =
        base::MakeRefCounted<CodecImageGroup>(gpu_task_runner_, surface_bundle);
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoFrameFactory::SetImageGroup,
                     base::Unretained(gpu_video_frame_factory_.get()),
                     std::move(image_group)));
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    OnceOutputCb output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before trying to
  // create the textures for it.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  auto image_ready_cb = base::BindOnce(
      &VideoFrameFactoryImpl::OnImageReady, std::move(output_cb), timestamp,
      coded_size, natural_size, texture_owner_, pixel_format, overlay_mode_,
      enable_threaded_texture_mailboxes_);

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoFrameFactory::CreateImage,
                     base::Unretained(gpu_video_frame_factory_.get()),
                     base::Passed(&output_buffer), texture_owner_,
                     std::move(promotion_hint_cb), std::move(image_ready_cb),
                     base::ThreadTaskRunnerHandle::Get()));
}

// static
void VideoFrameFactoryImpl::OnImageReady(
    OnceOutputCb output_cb,
    base::TimeDelta timestamp,
    gfx::Size coded_size,
    gfx::Size natural_size,
    scoped_refptr<TextureOwner> texture_owner,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool enable_threaded_texture_mailboxes,
    gpu::Mailbox mailbox,
    VideoFrame::ReleaseMailboxCB release_cb,
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  TRACE_EVENT0("media", "VideoVideoFrameFactoryImpl::OnVideoFrameImageReady");

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);

  // TODO(liberato): We should set the promotion hint cb here on the image.  We
  // should also set the output buffer params; we shouldn't send the output
  // buffer to the gpu thread, since the codec image isn't in use anyway.  We
  // can access it on any thread.  We'll also need to get new images when we
  // switch texture owners.  That's left for future work.

  // TODO(liberato): When we switch to a pool, we need to provide some way to
  // call MaybeRenderEarly that doesn't depend on |release_cb|.  I suppose we
  // could get a RepeatingCallback that's a "reuse cb", that we'd attach to the
  // VideoFrame's release cb, since we have to wait for the sync token anyway.
  // That would run on the gpu thread, and could MaybeRenderEarly.

  gfx::Rect visible_rect(coded_size);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, timestamp);

  frame->set_ycbcr_info(ycbcr_info);
  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  const bool is_surface_control =
      overlay_mode == OverlayMode::kSurfaceControlSecure ||
      overlay_mode == OverlayMode::kSurfaceControlInsecure;
  const bool wants_promotion_hints =
      overlay_mode == OverlayMode::kRequestPromotionHints;

  // Remember that we can't access |texture_owner|, but we can check if we have
  // one here.
  bool allow_overlay = false;
  if (is_surface_control) {
    DCHECK(texture_owner);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!texture_owner|, if we want to get hints.  It's required, else we won't
    // get hints.
    allow_overlay = !texture_owner || wants_promotion_hints;
  }

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hints);
  frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER,
                                !!texture_owner);

  frame->SetReleaseMailboxCB(std::move(release_cb));

  // Note that we don't want to handle the CodecImageGroup here.  It needs to be
  // accessed on the gpu thread.  Once we move to pooling, only the initial
  // create / destroy operations will affect it anyway, so it might as well stay
  // on the gpu thread.

  std::move(output_cb).Run(std::move(frame));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Hop through |gpu_task_runner_| to ensure it comes after pending frames.
  gpu_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     std::move(closure));
}

GpuVideoFrameFactory::GpuVideoFrameFactory() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

GpuVideoFrameFactory::~GpuVideoFrameFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

scoped_refptr<TextureOwner> GpuVideoFrameFactory::Initialize(
    VideoFrameFactoryImpl::OverlayMode overlay_mode,
    VideoFrameFactoryImpl::GetStubCb get_stub_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stub_ = get_stub_cb.Run();
  if (!MakeContextCurrent(stub_))
    return nullptr;
  stub_->AddDestructionObserver(this);

  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuVideoFrameFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    return nullptr;
  }

  // Make the shared context current.
  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      shared_context->context(), shared_context->surface());
  if (!shared_context->IsCurrent(nullptr)) {
    result = gpu::ContextResult::kTransientFailure;
    LOG(ERROR)
        << "GpuVideoFrameFactory: Unable to make shared context current.";
    ContextStateResultUMA(result);
    return nullptr;
  }
  return TextureOwner::Create(TextureOwner::CreateTexture(shared_context),
                              GetTextureOwnerMode(overlay_mode));
}

void GpuVideoFrameFactory::CreateImage(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner_,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoFrameFactoryImpl::ImageReadyCB image_ready_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Generate a shared image mailbox.
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();

  bool success =
      CreateImageInternal(std::move(output_buffer), std::move(texture_owner_),
                          mailbox, std::move(promotion_hint_cb));
  TRACE_EVENT0("media", "GpuVideoFrameFactory::CreateVideoFrame");
  if (!success)
    return;

  // Try to render this frame if possible.
  internal::MaybeRenderEarly(&images_);

  // This callback destroys the shared image when video frame is
  // released/destroyed. This callback has a weak pointer to the shared image
  // stub because shared image stub could be destroyed before video frame. In
  // those cases there is no need to destroy the shared image as the shared
  // image stub destruction will cause all the shared images to be destroyed.
  auto destroy_shared_image =
      stub_->channel()->shared_image_stub()->GetSharedImageDestructionCallback(
          mailbox);

  // Guarantee that the SharedImage is destroyed even if the VideoFrame is
  // dropped. Otherwise we could keep shared images we don't need alive.
  auto release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      BindToCurrentLoop(std::move(destroy_shared_image)), gpu::SyncToken());

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(image_ready_cb), mailbox,
                                       std::move(release_cb), ycbcr_info_));
}

bool GpuVideoFrameFactory::CreateImageInternal(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner_,
    gpu::Mailbox mailbox,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return false;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return false;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return false;

  gfx::Size size = output_buffer->size();

  // Create a Texture and a CodecImage to back it.
  std::unique_ptr<AbstractTexture> texture = decoder_helper_->CreateTexture(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size.width(), size.height(), GL_RGBA,
      GL_UNSIGNED_BYTE);
  auto image = base::MakeRefCounted<CodecImage>(
      std::move(output_buffer), texture_owner_, std::move(promotion_hint_cb));
  images_.push_back(image.get());

  // Add |image| to our current image group.  This makes sure that any overlay
  // lasts as long as the images.  For TextureOwner, it doesn't do much.
  image_group_->AddCodecImage(image.get());

  // Attach the image to the texture.
  // Either way, we expect this to be UNBOUND (i.e., decoder-managed).  For
  // overlays, BindTexImage will return true, causing it to transition to the
  // BOUND state, and thus receive ScheduleOverlayPlane calls.  For TextureOwner
  // backed images, BindTexImage will return false, and CopyTexImage will be
  // tried next.
  // TODO(liberato): consider not binding this as a StreamTextureImage if we're
  // using an overlay.  There's no advantage.  We'd likely want to create (and
  // initialize to a 1x1 texture) a 2D texture above in that case, in case
  // somebody tries to sample from it.  Be sure that promotion hints still
  // work properly, though -- they might require a stream texture image.
  GLuint texture_owner_service_id =
      texture_owner_ ? texture_owner_->GetTextureId() : 0;
  texture->BindStreamTextureImage(image.get(), texture_owner_service_id);

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuVideoFrameFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    return false;
  }

  // Create a shared image.
  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if media has a
  // colorspace and wire it here.
  // TODO(vikassoni): This shared image need to be thread safe eventually for
  // webview to work with shared images.
  auto shared_image = std::make_unique<SharedImageVideo>(
      mailbox, gfx::ColorSpace::CreateSRGB(), std::move(image),
      std::move(texture), std::move(shared_context),
      false /* is_thread_safe */);

  if (!ycbcr_info_)
    ycbcr_info_ = shared_image->GetYcbcrInfo();

  // Register it with shared image mailbox as well as legacy mailbox. This
  // keeps |shared_image| around until its destruction cb is called.
  // NOTE: Currently none of the video mailbox consumer uses shared image
  // mailbox.
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), /* legacy_mailbox */ true);

  return true;
}

void GpuVideoFrameFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
  decoder_helper_ = nullptr;
}

void GpuVideoFrameFactory::OnImageDestructed(CodecImage* image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

void GpuVideoFrameFactory::SetImageGroup(
    scoped_refptr<CodecImageGroup> image_group) {
  image_group_ = std::move(image_group);

  if (!image_group_)
    return;

  image_group_->SetDestructionCb(base::BindRepeating(
      &GpuVideoFrameFactory::OnImageDestructed, weak_factory_.GetWeakPtr()));
}

}  // namespace media
