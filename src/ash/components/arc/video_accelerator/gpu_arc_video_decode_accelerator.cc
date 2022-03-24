// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/vd_video_decode_accelerator.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Make sure arc::mojom::VideoDecodeAccelerator::Result and
// media::VideoDecodeAccelerator::Error match.
static_assert(static_cast<int>(
                  arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE) ==
                  media::VideoDecodeAccelerator::Error::ILLEGAL_STATE,
              "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT) ==
        media::VideoDecodeAccelerator::Error::INVALID_ARGUMENT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT) ==
        media::VideoDecodeAccelerator::Error::UNREADABLE_INPUT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE) ==
        media::VideoDecodeAccelerator::Error::PLATFORM_FAILURE,
    "enum mismatch");

namespace {

// An arbitrarily chosen limit of the number of output buffers. The number of
// output buffers used is requested from the untrusted client side.
constexpr size_t kMaxOutputBufferCount = 32;

// Maximum number of concurrent ARC video clients.
// Currently we have no way to know the resources are not enough to create more
// VDAs. Arbitrarily chosen a reasonable constant as the limit.
constexpr int kMaxConcurrentClients = 8;

arc::mojom::VideoDecodeAccelerator::Result ConvertErrorCode(
    media::VideoDecodeAccelerator::Error error) {
  switch (error) {
    case media::VideoDecodeAccelerator::ILLEGAL_STATE:
      return arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE;
    case media::VideoDecodeAccelerator::INVALID_ARGUMENT:
      return arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT;
    case media::VideoDecodeAccelerator::UNREADABLE_INPUT:
      return arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT;
    case media::VideoDecodeAccelerator::PLATFORM_FAILURE:
      return arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE;
    default:
      VLOGF(1) << "Unknown error: " << error;
      return ::arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE;
  }
}

}  // namespace

namespace arc {

// static
int GpuArcVideoDecodeAccelerator::instance_count_ = 0;

// static
int GpuArcVideoDecodeAccelerator::initialized_instance_count_ = 0;

GpuArcVideoDecodeAccelerator::GpuArcVideoDecodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager)
    : gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      protected_buffer_manager_(std::move(protected_buffer_manager)) {
  CHECK_LT(instance_count_, std::numeric_limits<int>::max());
  instance_count_++;
  base::UmaHistogramExactLinear(
      "Media.GpuArcVideoDecodeAccelerator.InstanceCount.All", instance_count_,
      /*exclusive_max=*/50);

  // If out-of-process video decoding is enabled, there should only be one
  // GpuArcVideoDecodeAccelerator instance per process. This is enforced in the
  // browser process by the
  // arc::<unnamed namespace>::VideoAcceleratorFactoryService.
  DCHECK(!base::FeatureList::IsEnabled(arc::kOutOfProcessVideoDecoding) ||
         instance_count_ == 1);
}

GpuArcVideoDecodeAccelerator::~GpuArcVideoDecodeAccelerator() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (vda_) {
    // Destroy |vda_| now in case it needs to use *|this| during tear-down.
    vda_.reset();

    // Normally |initialized_instance_count_| should always be > 0 if vda_ is
    // set, but if it isn't and we underflow then we won't be able to create any
    // new decoder forever (b/173700103). So let's use an extra check to avoid
    // this...
    if (initialized_instance_count_ > 0)
      initialized_instance_count_--;
  }
  DCHECK_GT(instance_count_, 0);
  instance_count_--;
}

void GpuArcVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    media::VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  NOTIMPLEMENTED() << "VDA must call ProvidePictureBuffersWithVisibleRect() "
                   << "for ARC++ video decoding";
}

void GpuArcVideoDecodeAccelerator::ProvidePictureBuffersWithVisibleRect(
    uint32_t requested_num_of_buffers,
    media::VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect,
    uint32_t texture_target) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Unretained(this) is safe, this callback will be executed inside the class.
  pending_provide_picture_buffers_requests_.push(
      base::BindOnce(&GpuArcVideoDecodeAccelerator::HandleProvidePictureBuffers,
                     base::Unretained(this), requested_num_of_buffers,
                     dimensions, visible_rect));
  CallPendingProvidePictureBuffers();
}

void GpuArcVideoDecodeAccelerator::DismissPictureBuffer(
    int32_t picture_buffer) {
  VLOGF(2) << "picture_buffer=" << picture_buffer;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // no-ops.
  // DismissPictureBuffer() is required for VDA::ALLOCATE mode,
  // as PictureBuffers are backed by texture IDs and not refcounted then.
  // This class supports VDA::IMPORT mode only however,
  // where PictureBuffers are backed by dmabufs, which are refcounted.
  // Both the client and the VDA itself maintain their own references and
  // can drop them independently when no longer needed, so there is no need to
  // explicitly notify the client about this.
}

void GpuArcVideoDecodeAccelerator::PictureReady(const media::Picture& picture) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  mojom::PicturePtr pic = mojom::Picture::New();
  pic->picture_buffer_id = picture.picture_buffer_id();
  pic->bitstream_id = picture.bitstream_buffer_id();
  pic->crop_rect = picture.visible_rect();
  client_->PictureReady(std::move(pic));
}

void GpuArcVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuArcVideoDecodeAccelerator::NotifyFlushDone() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pending_flush_callbacks_.empty()) {
    VLOGF(1) << "Unexpected NotifyFlushDone() callback received from VDA.";
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
    return;
  }
  // VDA::Flush are processed in order by VDA.
  // NotifyFlushDone() is also processed in order.
  std::move(pending_flush_callbacks_.front())
      .Run(mojom::VideoDecodeAccelerator::Result::SUCCESS);
  pending_flush_callbacks_.pop();
}

void GpuArcVideoDecodeAccelerator::NotifyResetDone() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pending_reset_callback_.is_null()) {
    VLOGF(1) << "Unexpected NotifyResetDone() callback received from VDA.";
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
    return;
  }
  // Reset() is processed asynchronously and any preceding Flush() calls might
  // have been dropped by the VDA.
  // We still need to execute callbacks associated with them,
  // even if we did not receive NotifyFlushDone() for them from the VDA.
  while (!pending_flush_callbacks_.empty()) {
    VLOGF(2) << "Flush is cancelled.";
    std::move(pending_flush_callbacks_.front())
        .Run(mojom::VideoDecodeAccelerator::Result::CANCELLED);
    pending_flush_callbacks_.pop();
  }

  std::move(pending_reset_callback_)
      .Run(mojom::VideoDecodeAccelerator::Result::SUCCESS);
  RunPendingRequests();
}

void GpuArcVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOGF(1) << "error= " << static_cast<int>(error);
  DCHECK(client_);

  // Reset and Return all the pending callbacks with PLATFORM_FAILURE
  while (!pending_flush_callbacks_.empty()) {
    std::move(pending_flush_callbacks_.front())
        .Run(mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
    pending_flush_callbacks_.pop();
  }
  if (pending_reset_callback_) {
    std::move(pending_reset_callback_)
        .Run(mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
  }

  error_state_ = true;
  client_->NotifyError(ConvertErrorCode(error));
  // Because the current state is ERROR, all the pending requests will be
  // dropped, and their callback will return with an error state.
  RunPendingRequests();
}

void GpuArcVideoDecodeAccelerator::RunPendingRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  while (!pending_reset_callback_ && !pending_requests_.empty()) {
    ExecuteRequest(std::move(pending_requests_.front()));
    pending_requests_.pop();
  }
}

void GpuArcVideoDecodeAccelerator::FlushRequest(
    PendingCallback cb,
    media::VideoDecodeAccelerator* vda) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(vda);
  pending_flush_callbacks_.emplace(std::move(cb));
  vda->Flush();
}

void GpuArcVideoDecodeAccelerator::ResetRequest(
    PendingCallback cb,
    media::VideoDecodeAccelerator* vda) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(vda);
  pending_reset_callback_ = std::move(cb);
  vda->Reset();
}

void GpuArcVideoDecodeAccelerator::DecodeRequest(
    media::BitstreamBuffer bitstream_buffer,
    PendingCallback cb,
    media::VideoDecodeAccelerator* vda) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(vda);
  vda->Decode(std::move(bitstream_buffer));
}

void GpuArcVideoDecodeAccelerator::ExecuteRequest(
    std::pair<PendingRequest, PendingCallback> request) {
  DVLOGF(4);
  auto& callback = request.second;
  if (!vda_) {
    VLOGF(1) << "VDA not initialized.";
    if (callback)
      std::move(callback).Run(
          mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE);
    return;
  }
  if (error_state_) {
    VLOGF(1) << "GAVDA state: ERROR";
    if (callback)
      std::move(callback).Run(
          mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
    return;
  }
  // When pending_reset_callback_ isn't null, GAVDA is awaiting a preceding
  // Reset() to be finished. Any requests are pended.
  // There is no need to take pending_flush_callbacks into account.
  // VDA::Reset() can be called while VDA::Flush() are being executed.
  // VDA::Flush()es can be called regardless of whether or not there is a
  // preceding VDA::Flush().
  if (pending_reset_callback_) {
    pending_requests_.emplace(std::move(request));
    return;
  }
  std::move(request.first).Run(std::move(callback), vda_.get());
}

void GpuArcVideoDecodeAccelerator::Initialize(
    mojom::VideoDecodeAcceleratorConfigPtr config,
    mojo::PendingRemote<mojom::VideoDecodeClient> client,
    InitializeCallback callback) {
  VLOGF(2) << "profile = " << config->profile
           << ", secure_mode = " << config->secure_mode;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!client_);
  client_.Bind(std::move(client));
  DCHECK(!pending_init_callback_);
  pending_init_callback_ = std::move(callback);

  InitializeTask(std::move(config));
}

void GpuArcVideoDecodeAccelerator::InitializeTask(
    mojom::VideoDecodeAcceleratorConfigPtr config) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (vda_) {
    VLOGF(1) << "Re-initialization not allowed.";
    return OnInitializeDone(
        mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE);
  }

  if (initialized_instance_count_ >= kMaxConcurrentClients) {
    VLOGF(1) << "Reject to Initialize() due to too many clients: "
             << initialized_instance_count_;
    return OnInitializeDone(
        mojom::VideoDecodeAccelerator::Result::INSUFFICIENT_RESOURCES);
  }

  media::VideoDecodeAccelerator::Config vda_config(config->profile);
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  vda_config.output_mode =
      media::VideoDecodeAccelerator::Config::OutputMode::IMPORT;

  if (base::FeatureList::IsEnabled(arc::kVideoDecoder)) {
    VLOGF(2) << "Using VideoDecoder-backed VdVideoDecodeAccelerator.";
    vda_config.is_deferred_initialization_allowed = true;
    vda_ = media::VdVideoDecodeAccelerator::Create(
        base::BindRepeating(&media::VideoDecoderPipeline::Create), this,
        vda_config, base::SequencedTaskRunnerHandle::Get());
  } else {
    VLOGF(2) << "Using original VDA";
    auto vda_factory = media::GpuVideoDecodeAcceleratorFactory::Create(
        media::GpuVideoDecodeGLClient());
    vda_ = vda_factory->CreateVDA(this, vda_config, gpu_workarounds_,
                                  gpu_preferences_);
  }
#endif

  if (!vda_) {
    VLOGF(1) << "Failed to create VDA.";
    return OnInitializeDone(
        mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
  }

  initialized_instance_count_++;
  VLOGF(2) << "Number of concurrent clients: " << initialized_instance_count_;
  base::UmaHistogramExactLinear(
      "Media.GpuArcVideoDecodeAccelerator.InstanceCount.Initialized",
      initialized_instance_count_, /*exclusive_max=*/50);

  secure_mode_ = absl::nullopt;
  error_state_ = false;
  pending_requests_ = {};
  pending_flush_callbacks_ = {};
  pending_reset_callback_.Reset();

  if (!vda_config.is_deferred_initialization_allowed)
    return OnInitializeDone(mojom::VideoDecodeAccelerator::Result::SUCCESS);
}

void GpuArcVideoDecodeAccelerator::NotifyInitializationComplete(
    media::Status status) {
  DVLOGF(4) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  OnInitializeDone(
      status.is_ok() ? mojom::VideoDecodeAccelerator::Result::SUCCESS
                     : mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
}

void GpuArcVideoDecodeAccelerator::OnInitializeDone(
    mojom::VideoDecodeAccelerator::Result result) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (result != mojom::VideoDecodeAccelerator::Result::SUCCESS)
    error_state_ = true;

  // Report initialization status to UMA.
  UMA_HISTOGRAM_ENUMERATION(
      "Media.GpuArcVideoDecodeAccelerator.InitializeResult", result);
  std::move(pending_init_callback_).Run(result);
}

void GpuArcVideoDecodeAccelerator::Decode(
    mojom::BitstreamBufferPtr bitstream_buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!vda_) {
    VLOGF(1) << "VDA not initialized.";
    return;
  }

  base::ScopedFD handle_fd =
      UnwrapFdFromMojoHandle(std::move(bitstream_buffer->handle_fd));
  if (!handle_fd.is_valid()) {
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return;
  }
  DVLOGF(4) << "fd=" << handle_fd.get();

  // If this is the first input buffer, determine if the playback is secure by
  // querying ProtectedBufferManager. If we can get the corresponding protected
  // buffer, then we consider the playback as secure. Otherwise, we consider it
  // as a normal playback.
  if (!secure_mode_.has_value()) {
    if (!protected_buffer_manager_) {
      DVLOGF(3) << "ProtectedBufferManager is null, treat as normal playback";
      secure_mode_ = false;
    } else {
      secure_mode_ = IsBufferSecure(protected_buffer_manager_.get(), handle_fd);
      VLOGF(2) << "First input buffer is secure buffer? " << *secure_mode_;
    }
  }

  base::subtle::PlatformSharedMemoryRegion shm_region;
  if (*secure_mode_) {
    // Use protected shared memory associated with the given file descriptor.
    shm_region = protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
        std::move(handle_fd));
    if (!shm_region.IsValid()) {
      VLOGF(1) << "No protected shared memory found for handle";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }
  } else {
    size_t handle_size;
    if (!media::GetFileSize(handle_fd.get(), &handle_size)) {
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }
    shm_region = base::subtle::PlatformSharedMemoryRegion::Take(
        std::move(handle_fd),
        base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, handle_size,
        base::UnguessableToken::Create());
    if (!shm_region.IsValid()) {
      VLOGF(1) << "Cannot take file descriptor based shared memory";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }
  }

  // Use Unretained(this) is safe, this callback will be executed in
  // ExecuteRequest().
  // This callback is immediately called or stored into a member variable.
  // All the callbacks thus should be called or deleted before |this| is
  // invalidated.
  ExecuteRequest(
      {base::BindOnce(
           &GpuArcVideoDecodeAccelerator::DecodeRequest, base::Unretained(this),
           media::BitstreamBuffer(
               bitstream_buffer->bitstream_id, std::move(shm_region),
               bitstream_buffer->bytes_used, bitstream_buffer->offset)),
       PendingCallback()});
}

void GpuArcVideoDecodeAccelerator::HandleProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect) {
  DVLOGF(2) << "requested_num_of_buffers=" << requested_num_of_buffers
            << ", dimensions=" << dimensions.ToString()
            << ", visible_rect=" << visible_rect.ToString();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);

  // Unretained(this) is safe, this callback will be executed inside the class.
  DCHECK(!current_provide_picture_buffers_cb_);
  current_provide_picture_buffers_cb_ = base::BindOnce(
      &GpuArcVideoDecodeAccelerator::OnAssignPictureBuffersCalled,
      base::Unretained(this), dimensions);

  auto pbf = mojom::PictureBufferFormat::New();
  pbf->min_num_buffers = requested_num_of_buffers;
  pbf->coded_size = dimensions;
  client_->ProvidePictureBuffers(std::move(pbf), visible_rect);
}

bool GpuArcVideoDecodeAccelerator::CallPendingProvidePictureBuffers() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (current_provide_picture_buffers_cb_ ||
      pending_provide_picture_buffers_requests_.empty()) {
    return false;
  }

  std::move(pending_provide_picture_buffers_requests_.front()).Run();
  pending_provide_picture_buffers_requests_.pop();
  return true;
}

void GpuArcVideoDecodeAccelerator::AssignPictureBuffers(uint32_t count) {
  VLOGF(2) << "count=" << count;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!current_provide_picture_buffers_cb_) {
    VLOGF(1) << "AssignPictureBuffers is not called right after "
             << "Client::ProvidePictureBuffers()";
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return;
  }
  std::move(current_provide_picture_buffers_cb_).Run(count);

  if (!CallPendingProvidePictureBuffers()) {
    // No further request. Now we can wait for the first imported buffer.
    awaiting_first_import_ = true;
  }
}

void GpuArcVideoDecodeAccelerator::OnAssignPictureBuffersCalled(
    const gfx::Size& dimensions,
    uint32_t count) {
  DVLOGF(3) << "dimensions=" << dimensions.ToString() << ", count=" << count;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!vda_) {
    VLOGF(1) << "VDA not initialized.";
    return;
  }
  if (count > kMaxOutputBufferCount) {
    VLOGF(1) << "Too many output buffers requested"
             << ", count =" << count;
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return;
  }
  coded_size_ = dimensions;
  output_buffer_count_ = static_cast<size_t>(count);
}

void GpuArcVideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    mojom::HalPixelFormat format,
    mojo::ScopedHandle handle,
    std::vector<VideoFramePlane> planes,
    mojom::BufferModifierPtr modifier_ptr) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!vda_) {
    VLOGF(1) << "VDA not initialized.";
    return;
  }
  if (current_provide_picture_buffers_cb_) {
    DVLOGF(3) << "AssignPictureBuffers() for the last "
              << "Client::ProvidePictureBuffers() hasn't been called, ignored.";
    return;
  }

  if (picture_buffer_id < 0 ||
      static_cast<size_t>(picture_buffer_id) >= output_buffer_count_) {
    VLOGF(1) << "Invalid picture_buffer_id=" << picture_buffer_id;
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return;
  }

  base::ScopedFD handle_fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!handle_fd.is_valid()) {
    client_->NotifyError(
        mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return;
  }

  media::VideoPixelFormat pixel_format;
  switch (format) {
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12:
      pixel_format = media::PIXEL_FORMAT_YV12;
      break;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_NV12:
      pixel_format = media::PIXEL_FORMAT_NV12;
      break;
    default:
      VLOGF(1) << "Unsupported format: " << format;
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
  }
  uint64_t modifier = gfx::NativePixmapHandle::kNoModifier;
  if (modifier_ptr) {
    modifier = modifier_ptr->val;
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  DCHECK(secure_mode_.has_value());
  if (*secure_mode_) {
    // Get protected output buffer associated with |handle_fd|.
    // Duplicating handle here is needed as ownership of passed fd is
    // transferred to AllocateProtectedNativePixmap().
    auto protected_native_pixmap =
        protected_buffer_manager_->GetProtectedNativePixmapHandleFor(
            std::move(handle_fd));
    if (protected_native_pixmap.planes.size() == 0) {
      VLOGF(1) << "No protected native pixmap found for handle";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
      return;
    }
    gmb_handle.native_pixmap_handle = std::move(protected_native_pixmap);

    // Explicitly verify the GPU Memory Buffer Handle here. Note that we do not
    // do this for non-protected content because the verification happens on
    // creation in that path.
    if (!media::VerifyGpuMemoryBufferHandle(pixel_format, coded_size_,
                                            gmb_handle)) {
      VLOGF(1) << "Invalid GpuMemoryBufferHandle for protected content";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }
  } else {
    std::vector<base::ScopedFD> handle_fds =
        DuplicateFD(std::move(handle_fd), planes.size());
    if (handle_fds.empty()) {
      VLOGF(1) << "Failed to duplicate fd";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }

    // Verification of the GPU Memory Buffer Handle is handled under the hood in
    // this call.
    auto buffer_handle = CreateGpuMemoryBufferHandle(
        pixel_format, modifier, coded_size_, std::move(handle_fds), planes);
    if (!buffer_handle) {
      VLOGF(1) << "Failed to create GpuMemoryBufferHandle";
      client_->NotifyError(
          mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
      return;
    }
    gmb_handle = std::move(buffer_handle).value();
  }
  gmb_handle.id = media::GetNextGpuMemoryBufferId();

  // This is the first time of ImportBufferForPicture() after
  // AssignPictureBuffers() is called. Call VDA::AssignPictureBuffers() here.
  if (awaiting_first_import_) {
    awaiting_first_import_ = false;
    gfx::Size picture_size(gmb_handle.native_pixmap_handle.planes[0].stride,
                           coded_size_.height());
    std::vector<media::PictureBuffer> buffers;
    for (size_t id = 0; id < output_buffer_count_; ++id) {
      buffers.push_back(
          media::PictureBuffer(static_cast<int32_t>(id), picture_size));
    }

    vda_->AssignPictureBuffers(std::move(buffers));
  }

  vda_->ImportBufferForPicture(picture_buffer_id, pixel_format,
                               std::move(gmb_handle));
}

void GpuArcVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!vda_) {
    VLOGF(1) << "VDA not initialized.";
    return;
  }
  if (current_provide_picture_buffers_cb_) {
    DVLOGF(3) << "AssignPictureBuffers() for the last "
              << "Client::ProvidePictureBuffers() hasn't been called, ignored.";
    return;
  }

  vda_->ReusePictureBuffer(picture_buffer_id);
}

void GpuArcVideoDecodeAccelerator::Flush(FlushCallback callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Use Unretained(this) is safe, this callback will be executed in
  // ExecuteRequest().
  // This callback is immediately called or stored into a member variable.
  // All the callbacks thus should be called or deleted before |this| is
  // invalidated.
  ExecuteRequest({base::BindOnce(&GpuArcVideoDecodeAccelerator::FlushRequest,
                                 base::Unretained(this)),
                  std::move(callback)});
}

void GpuArcVideoDecodeAccelerator::Reset(ResetCallback callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Use Unretained(this) is safe, this callback will be executed in
  // ExecuteRequest().
  // This callback is immediately called or stored into a member variable.
  // All the callbacks thus should be called or deleted before |this| is
  // invalidated.
  ExecuteRequest({base::BindOnce(&GpuArcVideoDecodeAccelerator::ResetRequest,
                                 base::Unretained(this)),
                  std::move(callback)});
}

}  // namespace arc
