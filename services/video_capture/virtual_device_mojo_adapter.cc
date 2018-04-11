// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/virtual_device_mojo_adapter.h"

#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/scoped_buffer_pool_reservation.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/scoped_access_permission_media_to_mojo_adapter.h"

namespace {

void OnNewBufferHandleAcknowleged(
    video_capture::mojom::VirtualDevice::RequestFrameBufferCallback callback,
    int32_t buffer_id) {
  std::move(callback).Run(buffer_id);
}

}  // anonymous namespace

namespace video_capture {

VirtualDeviceMojoAdapter::VirtualDeviceMojoAdapter(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    const media::VideoCaptureDeviceInfo& device_info,
    mojom::ProducerPtr producer)
    : service_ref_(std::move(service_ref)),
      device_info_(device_info),
      producer_(std::move(producer)),
      buffer_pool_(new media::VideoCaptureBufferPoolImpl(
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          max_buffer_pool_buffer_count())) {}

VirtualDeviceMojoAdapter::~VirtualDeviceMojoAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count() {
  // The maximum number of video frame buffers in-flight at any one time
  // If all buffers are still in use by consumers when new frames are produced
  // those frames get dropped.
  static const int kMaxBufferCount = 3;

  return kMaxBufferCount;
}

void VirtualDeviceMojoAdapter::RequestFrameBuffer(
    const gfx::Size& dimension,
    media::VideoPixelFormat pixel_format,
    media::VideoPixelStorage pixel_storage,
    RequestFrameBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int buffer_id_to_drop = media::VideoCaptureBufferPool::kInvalidId;
  const int buffer_id = buffer_pool_->ReserveForProducer(
      dimension, pixel_format, pixel_storage, 0 /* frame_feedback_id */,
      &buffer_id_to_drop);

  // Remove dropped buffer if there is one.
  if (buffer_id_to_drop != media::VideoCaptureBufferPool::kInvalidId) {
    auto entry_iter = std::find(known_buffer_ids_.begin(),
                                known_buffer_ids_.end(), buffer_id_to_drop);
    if (entry_iter != known_buffer_ids_.end()) {
      known_buffer_ids_.erase(entry_iter);
      producer_->OnBufferRetired(buffer_id_to_drop);
      if (receiver_.is_bound()) {
        receiver_->OnBufferRetired(buffer_id_to_drop);
      }
    }
  }

  // No buffer available.
  if (buffer_id == media::VideoCaptureBufferPool::kInvalidId) {
    std::move(callback).Run(mojom::kInvalidBufferId);
    return;
  }

  if (!base::ContainsValue(known_buffer_ids_, buffer_id)) {
    if (receiver_.is_bound()) {
      receiver_->OnNewBufferHandle(
          buffer_id, buffer_pool_->GetHandleForInterProcessTransit(
                         buffer_id, true /* read_only */));
    }
    known_buffer_ids_.push_back(buffer_id);

    // Invoke the response back only after the producer have acked
    // that it has received the newly created buffer. This is need
    // because the |producer_| and the |callback| are bound to different
    // message pipes, so the order for calls to |producer_| and |callback|
    // is not guaranteed.
    producer_->OnNewBufferHandle(
        buffer_id,
        buffer_pool_->GetHandleForInterProcessTransit(buffer_id,
                                                      false /* read_only */),
        base::BindOnce(&OnNewBufferHandleAcknowleged, base::Passed(&callback),
                       buffer_id));
    return;
  }
  std::move(callback).Run(buffer_id);
}

void VirtualDeviceMojoAdapter::OnFrameReadyInBuffer(
    int32_t buffer_id,
    ::media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unknown buffer ID.
  if (!base::ContainsValue(known_buffer_ids_, buffer_id)) {
    return;
  }

  // Notify receiver if there is one.
  if (receiver_.is_bound()) {
    buffer_pool_->HoldForConsumers(buffer_id, 1 /* num_clients */);
    auto access_permission = std::make_unique<
        media::ScopedBufferPoolReservation<media::ConsumerReleaseTraits>>(
        buffer_pool_, buffer_id);
    mojom::ScopedAccessPermissionPtr access_permission_proxy;
    mojo::MakeStrongBinding<mojom::ScopedAccessPermission>(
        std::make_unique<ScopedAccessPermissionMediaToMojoAdapter>(
            std::move(access_permission)),
        mojo::MakeRequest(&access_permission_proxy));
    receiver_->OnFrameReadyInBuffer(buffer_id, 0 /* frame_feedback_id */,
                                    std::move(access_permission_proxy),
                                    std::move(frame_info));
  }
  buffer_pool_->RelinquishProducerReservation(buffer_id);
}

void VirtualDeviceMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojom::ReceiverPtr receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver.set_connection_error_handler(
      base::Bind(&VirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose,
                 base::Unretained(this)));
  receiver_ = std::move(receiver);
  receiver_->OnStarted();

  // Notify receiver of known buffers */
  for (auto buffer_id : known_buffer_ids_) {
    receiver_->OnNewBufferHandle(buffer_id,
                                 buffer_pool_->GetHandleForInterProcessTransit(
                                     buffer_id, true /* read_only */));
  }
}

void VirtualDeviceMojoAdapter::OnReceiverReportingUtilization(
    int32_t frame_feedback_id,
    double utilization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(nullptr);
}

void VirtualDeviceMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VirtualDeviceMojoAdapter::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_.is_bound())
    return;
  // Unsubscribe from connection error callbacks.
  receiver_.set_connection_error_handler(base::Closure());
  receiver_.reset();
}

void VirtualDeviceMojoAdapter::OnReceiverConnectionErrorOrClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

}  // namespace video_capture
