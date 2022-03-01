// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/device_service.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if !defined(OS_ANDROID)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

namespace content {

namespace {

#if !defined(OS_ANDROID)
constexpr int32_t kMouseCursorStackingIndex = 1;
#endif

// Transfers ownership of an object to a std::unique_ptr with a custom deleter
// that ensures the object is destroyed on the UI BrowserThread.
template <typename T>
std::unique_ptr<T, BrowserThread::DeleteOnUIThread> RescopeToUIThread(
    std::unique_ptr<T>&& ptr) {
  return std::unique_ptr<T, BrowserThread::DeleteOnUIThread>(ptr.release());
}

// Adapter for a VideoFrameReceiver to notify once frame consumption is
// complete. VideoFrameReceiver requires owning an object that it will destroy
// once consumption is complete. This class adapts between that scheme and
// running a "done callback" to notify that consumption is complete.
class ScopedFrameDoneHelper final
    : public base::ScopedClosureRunner,
      public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  explicit ScopedFrameDoneHelper(base::OnceClosure done_callback)
      : base::ScopedClosureRunner(std::move(done_callback)) {}
  ~ScopedFrameDoneHelper() final = default;
};

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

}  // namespace

#if !defined(OS_ANDROID)
FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice()
    : cursor_controller_(
          RescopeToUIThread(std::make_unique<MouseCursorOverlayController>())) {
  DCHECK(cursor_controller_);
}
#else
FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice() = default;
#endif

FrameSinkVideoCaptureDevice::~FrameSinkVideoCaptureDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_) << "StopAndDeAllocate() was never called after start.";
}

void FrameSinkVideoCaptureDevice::AllocateAndStartWithReceiver(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  DCHECK(receiver);

  // If the device has already ended on a fatal error, abort immediately.
  if (fatal_error_message_) {
    receiver->OnLog(*fatal_error_message_);
    receiver->OnError(media::VideoCaptureError::
                          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError);
    return;
  }

  capture_params_ = params;
  WillStart();
  DCHECK(!receiver_);
  receiver_ = std::move(receiver);

  // Shutdown the prior capturer, if any.
  MaybeStopConsuming();

  capturer_ = std::make_unique<viz::ClientFrameSinkVideoCapturer>(
      base::BindRepeating(&FrameSinkVideoCaptureDevice::CreateCapturer,
                          base::Unretained(this)));

  capturer_->SetFormat(capture_params_.requested_format.pixel_format);
  capturer_->SetMinCapturePeriod(
      base::Microseconds(base::saturated_cast<int64_t>(
          base::Time::kMicrosecondsPerSecond /
          capture_params_.requested_format.frame_rate)));
  const auto& constraints = capture_params_.SuggestConstraints();
  capturer_->SetResolutionConstraints(constraints.min_frame_size,
                                      constraints.max_frame_size,
                                      constraints.fixed_aspect_ratio);

  if (target_) {
    capturer_->ChangeTarget(target_);
  }

#if !defined(OS_ANDROID)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MouseCursorOverlayController::Start,
                     cursor_controller_->GetWeakPtr(),
                     capturer_->CreateOverlay(kMouseCursorStackingIndex),
                     base::ThreadTaskRunnerHandle::Get()));
#endif

  receiver_->OnStarted();

  if (!suspend_requested_) {
    MaybeStartConsuming();
  }

  DCHECK(!wake_lock_);
  RequestWakeLock();
}

void FrameSinkVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // FrameSinkVideoCaptureDevice does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED();
}

void FrameSinkVideoCaptureDevice::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_ && !suspend_requested_) {
    capturer_->RequestRefreshFrame();
  }
}

void FrameSinkVideoCaptureDevice::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = true;
  MaybeStopConsuming();
}

void FrameSinkVideoCaptureDevice::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = false;
  MaybeStartConsuming();
}

void FrameSinkVideoCaptureDevice::Crop(
    const base::Token& crop_id,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(
      media::mojom::CropRequestResult::kUnsupportedCaptureDevice);
}

void FrameSinkVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }

#if !defined(OS_ANDROID)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorOverlayController::Stop,
                                cursor_controller_->GetWeakPtr()));
#endif

  MaybeStopConsuming();
  capturer_.reset();
  if (receiver_) {
    receiver_.reset();
    DidStop();
  }
}

void FrameSinkVideoCaptureDevice::OnUtilizationReport(
    int frame_feedback_id,
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Assumption: The mojo InterfacePtr in |frame_callbacks_| should be valid at
  // this point because this method will always be called before the
  // VideoFrameReceiver signals it is done consuming the frame.
  const auto index = static_cast<size_t>(frame_feedback_id);
  DCHECK_LT(index, frame_callbacks_.size());
  frame_callbacks_[index]->ProvideFeedback(feedback);
}

void FrameSinkVideoCaptureDevice::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks);

  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  if (!receiver_ || !data) {
    callbacks_remote->Done();
    return;
  }

  // Search for the next available element in |frame_callbacks_| and bind
  // |callbacks| there.
  size_t index = 0;
  for (;; ++index) {
    if (index == frame_callbacks_.size()) {
      // The growth of |frame_callbacks_| should be bounded because the
      // viz::mojom::FrameSinkVideoCapturer should enforce an upper-bound on the
      // number of frames in-flight.
      constexpr size_t kMaxInFlightFrames = 32;  // Arbitrarily-chosen limit.
      DCHECK_LT(frame_callbacks_.size(), kMaxInFlightFrames);
      frame_callbacks_.emplace_back(std::move(callbacks_remote));
      break;
    }
    if (!frame_callbacks_[index].is_bound()) {
      frame_callbacks_[index] = std::move(callbacks_remote);
      break;
    }
  }
  const BufferId buffer_id = static_cast<BufferId>(index);

#if !defined(OS_ANDROID)
  info->metadata.interactive_content =
      cursor_controller_->IsUserInteractingWithView();
#else
  // Since we don't have a cursor controller, on Android we'll just always
  // assume the user is interacting with the view.
  info->metadata.interactive_content = true;
#endif

  // Pass the video frame to the VideoFrameReceiver. This is done by first
  // passing the shared memory buffer handle and then notifying it that a new
  // frame is ready to be read from the buffer.
  receiver_->OnNewBuffer(buffer_id, std::move(data));
  receiver_->OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(
          buffer_id, buffer_id,
          std::make_unique<ScopedFrameDoneHelper>(
              media::BindToCurrentLoop(base::BindOnce(
                  &FrameSinkVideoCaptureDevice::OnFramePropagationComplete,
                  weak_factory_.GetWeakPtr(), buffer_id))),
          std::move(info)),
      {});
}

void FrameSinkVideoCaptureDevice::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method would never be called if FrameSinkVideoCaptureDevice explicitly
  // called capturer_->StopAndResetConsumer(), because the binding is closed at
  // that time. Therefore, a call to this method means that the capturer cannot
  // continue; and that's a permanent failure.
  OnFatalError("Capturer service cannot continue.");
}

void FrameSinkVideoCaptureDevice::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (receiver_) {
    if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      receiver_->OnLog(message);
    } else {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&media::VideoFrameReceiver::OnLog,
                         base::Unretained(receiver_.get()), message));
    }
  }
}

void FrameSinkVideoCaptureDevice::OnTargetChanged(
    const absl::optional<viz::VideoCaptureTarget>& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  target_ = target;
  if (capturer_) {
    capturer_->ChangeTarget(target_);
  }
}

void FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnTargetChanged(absl::nullopt);
  OnFatalError("Capture target has been permanently lost.");
}

void FrameSinkVideoCaptureDevice::WillStart() {}

void FrameSinkVideoCaptureDevice::DidStop() {}

void FrameSinkVideoCaptureDevice::CreateCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  CreateCapturerViaGlobalManager(std::move(receiver));
}

// static
void FrameSinkVideoCaptureDevice::CreateCapturerViaGlobalManager(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  // Send the receiver to UI thread because that's where HostFrameSinkManager
  // lives.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                 receiver) {
            viz::HostFrameSinkManager* const manager =
                GetHostFrameSinkManager();
            DCHECK(manager);
            manager->CreateVideoCapturer(std::move(receiver));
          },
          std::move(receiver)));
}

void FrameSinkVideoCaptureDevice::MaybeStartConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_ || !capturer_) {
    return;
  }

  capturer_->Start(this, viz::mojom::BufferFormatPreference::kDefault);
}

void FrameSinkVideoCaptureDevice::MaybeStopConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_)
    capturer_->StopAndResetConsumer();
}

void FrameSinkVideoCaptureDevice::OnFramePropagationComplete(
    BufferId buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify the VideoFrameReceiver that the buffer is no longer valid.
  if (receiver_) {
    receiver_->OnBufferRetired(buffer_id);
  }

  // Notify the capturer that consumption of the frame is complete.
  const size_t index = static_cast<size_t>(buffer_id);
  DCHECK_LT(index, frame_callbacks_.size());
  auto& callbacks_ptr = frame_callbacks_[index];
  callbacks_ptr->Done();
  callbacks_ptr.reset();
}

void FrameSinkVideoCaptureDevice::OnFatalError(std::string message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fatal_error_message_ = std::move(message);
  if (receiver_) {
    receiver_->OnLog(*fatal_error_message_);
    receiver_->OnError(media::VideoCaptureError::
                           kFrameSinkVideoCaptureDeviceEncounteredFatalError);
  }

  StopAndDeAllocate();
}

void FrameSinkVideoCaptureDevice::RequestWakeLock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  auto receiver = wake_lock_provider.BindNewPipeAndPassReceiver();
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BindWakeLockProvider, std::move(receiver)));
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "screen capture",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

}  // namespace content
