// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/request_manager.h"

#include <sync/sync.h>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/posix/safe_strerror.h"
#include "base/trace_event/trace_event.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

constexpr uint32_t kUndefinedFrameNumber = 0xFFFFFFFF;
}  // namespace

RequestManager::RequestManager(
    cros::mojom::Camera3CallbackOpsRequest callback_ops_request,
    std::unique_ptr<StreamCaptureInterface> capture_interface,
    CameraDeviceContext* device_context,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
    BlobifyCallback blobify_callback,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : callback_ops_(this, std::move(callback_ops_request)),
      capture_interface_(std::move(capture_interface)),
      device_context_(device_context),
      stream_buffer_manager_(
          new StreamBufferManager(device_context_,
                                  std::move(camera_buffer_factory))),
      blobify_callback_(std::move(blobify_callback)),
      ipc_task_runner_(std::move(ipc_task_runner)),
      capturing_(false),
      frame_number_(0),
      partial_result_count_(1),
      first_frame_shutter_time_(base::TimeTicks()),
      weak_ptr_factory_(this) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops_.is_bound());
  DCHECK(device_context_);
  // We use base::Unretained() for the StreamBufferManager here since we
  // guarantee |request_buffer_callback| is only used by RequestBuilder. In
  // addition, since C++ destroys member variables in reverse order of
  // construction, we can ensure that RequestBuilder will be destroyed prior
  // to StreamBufferManager since RequestBuilder constructs after
  // StreamBufferManager.
  auto request_buffer_callback =
      base::BindRepeating(&StreamBufferManager::RequestBuffer,
                          base::Unretained(stream_buffer_manager_.get()));
  request_builder_ = std::make_unique<RequestBuilder>(
      device_context_, std::move(request_buffer_callback));
}

RequestManager::~RequestManager() = default;

void RequestManager::SetUpStreamsAndBuffers(
    VideoCaptureFormat capture_format,
    const cros::mojom::CameraMetadataPtr& static_metadata,
    std::vector<cros::mojom::Camera3StreamPtr> streams) {
  // The partial result count metadata is optional; defaults to 1 in case it
  // is not set in the static metadata.
  const cros::mojom::CameraMetadataEntryPtr* partial_count = GetMetadataEntry(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
  if (partial_count) {
    partial_result_count_ =
        *reinterpret_cast<int32_t*>((*partial_count)->data.data());
  }

  // Set the last received frame number for each stream types to be undefined.
  for (const auto& stream : streams) {
    StreamType stream_type = StreamIdToStreamType(stream->id);
    last_received_frame_number_map_[stream_type] = kUndefinedFrameNumber;
  }

  stream_buffer_manager_->SetUpStreamsAndBuffers(
      capture_format, std::move(static_metadata), std::move(streams));
}

cros::mojom::Camera3StreamPtr RequestManager::GetStreamConfiguration(
    StreamType stream_type) {
  return stream_buffer_manager_->GetStreamConfiguration(stream_type);
}

void RequestManager::StartPreview(
    cros::mojom::CameraMetadataPtr preview_settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(repeating_request_settings_.is_null());

  capturing_ = true;
  repeating_request_settings_ = std::move(preview_settings);

  PrepareCaptureRequest();
}

void RequestManager::StopPreview(base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  capturing_ = false;
  repeating_request_settings_ = nullptr;
  if (callback) {
    capture_interface_->Flush(std::move(callback));
  }
}

void RequestManager::TakePhoto(cros::mojom::CameraMetadataPtr settings,
                               VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  std::vector<uint8_t> frame_orientation(sizeof(int32_t));
  *reinterpret_cast<int32_t*>(frame_orientation.data()) =
      base::checked_cast<int32_t>(device_context_->GetCameraFrameOrientation());
  cros::mojom::CameraMetadataEntryPtr e =
      cros::mojom::CameraMetadataEntry::New();
  e->tag = cros::mojom::CameraMetadataTag::ANDROID_JPEG_ORIENTATION;
  e->type = cros::mojom::EntryType::TYPE_INT32;
  e->count = 1;
  e->data = std::move(frame_orientation);
  AddOrUpdateMetadataEntry(&settings, std::move(e));

  oneshot_request_settings_.push(std::move(settings));
  take_photo_callback_queue_.push(std::move(callback));
}

void RequestManager::PrepareCaptureRequest() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  std::set<StreamType> stream_types;
  cros::mojom::CameraMetadataPtr settings;

  // Reqular request should always have repeating request if the preview is
  // on.
  stream_types.insert(StreamType::kPreview);
  if (!stream_buffer_manager_->HasFreeBuffers(stream_types)) {
    return;
  }
  bool has_still_capture_streams = false;
  if (!oneshot_request_settings_.empty() &&
      stream_buffer_manager_->HasFreeBuffers({StreamType::kStillCapture})) {
    stream_types.insert(StreamType::kStillCapture);
    settings = std::move(oneshot_request_settings_.front());
    oneshot_request_settings_.pop();
    has_still_capture_streams = true;
  } else {
    settings = repeating_request_settings_.Clone();
  }

  auto capture_request = request_builder_->BuildRequest(std::move(stream_types),
                                                        std::move(settings));
  if (has_still_capture_streams) {
    SendCaptureRequest(std::move(capture_request),
                       std::move(take_photo_callback_queue_.front()));
    take_photo_callback_queue_.pop();
  } else {
    SendCaptureRequest(std::move(capture_request), base::DoNothing());
  }
}

void RequestManager::SendCaptureRequest(
    cros::mojom::Camera3CaptureRequestPtr capture_request,
    VideoCaptureDevice::TakePhotoCallback take_photo_callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (!capturing_) {
    return;
  }

  CaptureResult& pending_result = pending_results_[frame_number_];
  pending_result.still_capture_callback = std::move(take_photo_callback);
  pending_result.unsubmitted_buffer_count =
      capture_request->output_buffers.size();

  UpdateCaptureSettings(&capture_request->settings);
  capture_request->frame_number = frame_number_;
  capture_interface_->ProcessCaptureRequest(
      std::move(capture_request),
      base::BindOnce(&RequestManager::OnProcessedCaptureRequest,
                     weak_ptr_factory_.GetWeakPtr()));
  frame_number_++;
}

void RequestManager::OnProcessedCaptureRequest(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (result != 0) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerProcessCaptureRequestFailed,
        FROM_HERE,
        std::string("Process capture request failed: ") +
            base::safe_strerror(-result));
    return;
  }

  PrepareCaptureRequest();
}

void RequestManager::ProcessCaptureResult(
    cros::mojom::Camera3CaptureResultPtr result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  uint32_t frame_number = result->frame_number;
  // A new partial result may be created in either ProcessCaptureResult or
  // Notify.
  CaptureResult& pending_result = pending_results_[frame_number];

  // |result->partial_result| is set to 0 if the capture result contains only
  // the result buffer handles and no result metadata.
  if (result->partial_result != 0) {
    uint32_t result_id = result->partial_result;
    if (result_id > partial_result_count_) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerInvalidPendingResultId,
          FROM_HERE,
          std::string("Invalid pending_result id: ") +
              std::to_string(result_id));
      return;
    }
    if (pending_result.partial_metadata_received.count(result_id)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata,
          FROM_HERE,
          std::string("Received duplicated partial metadata: ") +
              std::to_string(result_id));
      return;
    }
    DVLOG(2) << "Received partial result " << result_id << " for frame "
             << frame_number;
    pending_result.partial_metadata_received.insert(result_id);
    MergeMetadata(&pending_result.metadata, result->result);
  }

  if (result->output_buffers) {
    if (result->output_buffers->size() > kMaxConfiguredStreams) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived,
          FROM_HERE,
          std::string("Incorrect number of output buffers received: ") +
              std::to_string(result->output_buffers->size()));
      return;
    }

    for (auto& stream_buffer : result->output_buffers.value()) {
      DVLOG(2) << "Received capture result for frame " << frame_number
               << " stream_id: " << stream_buffer->stream_id;
      StreamType stream_type = StreamIdToStreamType(stream_buffer->stream_id);
      if (stream_type == StreamType::kUnknown) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived,
            FROM_HERE,
            std::string("Invalid type of output buffers received: ") +
                std::to_string(stream_buffer->stream_id));
        return;
      }

      // The camera HAL v3 API specifies that only one capture result can carry
      // the result buffer for any given frame number.
      if (last_received_frame_number_map_[stream_type] ==
          kUndefinedFrameNumber) {
        last_received_frame_number_map_[stream_type] = frame_number;
      } else {
        if (last_received_frame_number_map_[stream_type] == frame_number) {
          device_context_->SetErrorState(
              media::VideoCaptureError::
                  kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame,
              FROM_HERE,
              std::string("Received multiple result buffers for frame ") +
                  std::to_string(frame_number) + std::string(" for stream ") +
                  std::to_string(stream_buffer->stream_id));
          return;
        } else if (last_received_frame_number_map_[stream_type] >
                   frame_number) {
          device_context_->SetErrorState(
              media::VideoCaptureError::
                  kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder,
              FROM_HERE,
              std::string("Received frame is out-of-order; expect frame number "
                          "greater than ") +
                  std::to_string(last_received_frame_number_map_[stream_type]) +
                  std::string(" but got ") + std::to_string(frame_number));
        } else {
          last_received_frame_number_map_[stream_type] = frame_number;
        }
      }

      if (stream_buffer->status ==
          cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
        // If the buffer is marked as error, its content is discarded for this
        // frame.  Send the buffer to the free list directly through
        // SubmitCaptureResult.
        SubmitCaptureResult(frame_number, stream_type,
                            std::move(stream_buffer));
      } else {
        pending_result.buffers[stream_type] = std::move(stream_buffer);
      }
    }
  }

  TRACE_EVENT1("camera", "Capture Result", "frame_number", frame_number);
  TrySubmitPendingBuffers(frame_number);
}

void RequestManager::TrySubmitPendingBuffers(uint32_t frame_number) {
  if (!pending_results_.count(frame_number)) {
    return;
  }

  CaptureResult& pending_result = pending_results_[frame_number];

  // If the metadata is not ready, or the shutter time is not set, just
  // returned.
  bool is_ready_to_submit =
      pending_result.partial_metadata_received.size() > 0 &&
      *pending_result.partial_metadata_received.rbegin() ==
          partial_result_count_ &&
      !pending_result.reference_time.is_null();
  if (!is_ready_to_submit) {
    return;
  }

  if (!pending_result.buffers.empty()) {
    // Put pending buffers into local map since |pending_result| might be
    // deleted in SubmitCaptureResult(). We should not reference pending_result
    // after SubmitCaptureResult() is triggered.
    std::map<StreamType, cros::mojom::Camera3StreamBufferPtr> buffers =
        std::move(pending_result.buffers);
    for (auto& it : buffers) {
      SubmitCaptureResult(frame_number, it.first, std::move(it.second));
    }
  }
}

void RequestManager::Notify(cros::mojom::Camera3NotifyMsgPtr message) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (message->type == cros::mojom::Camera3MsgType::CAMERA3_MSG_ERROR) {
    auto error = std::move(message->message->get_error());
    uint32_t frame_number = error->frame_number;
    uint64_t error_stream_id = error->error_stream_id;
    StreamType stream_type = StreamIdToStreamType(error_stream_id);
    if (stream_type == StreamType::kUnknown) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg,
          FROM_HERE,
          std::string("Unknown stream in Camera3NotifyMsg: ") +
              std::to_string(error_stream_id));
      return;
    }
    cros::mojom::Camera3ErrorMsgCode error_code = error->error_code;
    HandleNotifyError(frame_number, stream_type, error_code);
  } else if (message->type ==
             cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER) {
    auto shutter = std::move(message->message->get_shutter());
    uint32_t frame_number = shutter->frame_number;
    uint64_t shutter_time = shutter->timestamp;
    DVLOG(2) << "Received shutter time for frame " << frame_number;
    if (!shutter_time) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedInvalidShutterTime,
          FROM_HERE,
          std::string("Received invalid shutter time: ") +
              std::to_string(shutter_time));
      return;
    }
    CaptureResult& pending_result = pending_results_[frame_number];
    // Shutter timestamp is in ns.
    base::TimeTicks reference_time =
        base::TimeTicks() +
        base::TimeDelta::FromMicroseconds(shutter_time / 1000);
    pending_result.reference_time = reference_time;
    if (first_frame_shutter_time_.is_null()) {
      // Record the shutter time of the first frame for calculating the
      // timestamp.
      first_frame_shutter_time_ = reference_time;
    }
    pending_result.timestamp = reference_time - first_frame_shutter_time_;
    TrySubmitPendingBuffers(frame_number);
  }
}

void RequestManager::HandleNotifyError(
    uint32_t frame_number,
    StreamType stream_type,
    cros::mojom::Camera3ErrorMsgCode error_code) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  std::string warning_msg;

  switch (error_code) {
    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_DEVICE:
      // Fatal error and no more frames will be produced by the device.
      device_context_->SetErrorState(
          media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError,
          FROM_HERE, "Fatal device error");
      return;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_REQUEST:
      // An error has occurred in processing the request; the request
      // specified by |frame_number| has been dropped by the camera device.
      // Subsequent requests are unaffected.
      //
      // The HAL will call ProcessCaptureResult with the buffers' state set to
      // STATUS_ERROR.  The content of the buffers will be dropped and the
      // buffers will be reused in SubmitCaptureResult.
      warning_msg =
          std::string("An error occurred while processing request for frame ") +
          std::to_string(frame_number);
      break;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT:
      // An error has occurred in producing the output metadata buffer for a
      // result; the output metadata will not be available for the frame
      // specified by |frame_number|.  Subsequent requests are unaffected.
      warning_msg = std::string(
                        "An error occurred while producing result "
                        "metadata for frame ") +
                    std::to_string(frame_number);
      break;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_BUFFER:
      // An error has occurred in placing the output buffer into a stream for
      // a request. |frame_number| specifies the request for which the buffer
      // was dropped, and |stream_type| specifies the stream that dropped
      // the buffer.
      //
      // The HAL will call ProcessCaptureResult with the buffer's state set to
      // STATUS_ERROR.  The content of the buffer will be dropped and the
      // buffer will be reused in SubmitCaptureResult.
      warning_msg =
          std::string(
              "An error occurred while filling output buffer of stream ") +
          StreamTypeToString(stream_type) + std::string(" in frame ") +
          std::to_string(frame_number);
      break;

    default:
      // To eliminate the warning for not handling CAMERA3_MSG_NUM_ERRORS
      break;
  }

  LOG(WARNING) << warning_msg << stream_type;
  device_context_->LogToClient(warning_msg);

  // If the buffer is already returned by the HAL, submit it and we're done.
  if (pending_results_.count(frame_number)) {
    auto it = pending_results_[frame_number].buffers.find(stream_type);
    if (it != pending_results_[frame_number].buffers.end()) {
      auto stream_buffer = std::move(it->second);
      pending_results_[frame_number].buffers.erase(stream_type);
      SubmitCaptureResult(frame_number, stream_type, std::move(stream_buffer));
    }
  }
}

void RequestManager::SubmitCaptureResult(
    uint32_t frame_number,
    StreamType stream_type,
    cros::mojom::Camera3StreamBufferPtr stream_buffer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_results_.count(frame_number));

  CaptureResult& pending_result = pending_results_[frame_number];
  DVLOG(2) << "Submit capture result of frame " << frame_number
           << " for stream " << static_cast<int>(stream_type);
  for (auto* observer : result_metadata_observers_) {
    observer->OnResultMetadataAvailable(pending_result.metadata);
  }
  uint64_t buffer_id = stream_buffer->buffer_id;

  // Wait on release fence before delivering the result buffer to client.
  if (stream_buffer->release_fence.is_valid()) {
    const int kSyncWaitTimeoutMs = 1000;
    mojo::PlatformHandle fence =
        mojo::UnwrapPlatformHandle(std::move(stream_buffer->release_fence));
    if (!fence.is_valid()) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd,
          FROM_HERE, "Failed to unwrap release fence fd");
      return;
    }
    if (!sync_wait(fence.GetFD().get(), kSyncWaitTimeoutMs)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut,
          FROM_HERE, "Sync wait on release fence timed out");
      return;
    }
  }

  // Deliver the captured data to client.
  if (stream_buffer->status ==
      cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK) {
    gfx::GpuMemoryBuffer* buffer =
        stream_buffer_manager_->GetBufferById(stream_type, buffer_id);
    if (stream_type == StreamType::kPreview) {
      device_context_->SubmitCapturedData(
          buffer, stream_buffer_manager_->GetStreamCaptureFormat(stream_type),
          pending_result.reference_time, pending_result.timestamp);
    } else if (stream_type == StreamType::kStillCapture) {
      DCHECK(pending_result.still_capture_callback);
      const Camera3JpegBlob* header = reinterpret_cast<Camera3JpegBlob*>(
          reinterpret_cast<uintptr_t>(buffer->memory(0)) +
          buffer->GetSize().width() - sizeof(Camera3JpegBlob));
      if (header->jpeg_blob_id != kCamera3JpegBlobId) {
        device_context_->SetErrorState(
            media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob,
            FROM_HERE, "Invalid JPEG blob");
        return;
      }
      // Still capture result from HALv3 already has orientation info in EXIF,
      // so just provide 0 as screen rotation in |blobify_callback_| parameters.
      mojom::BlobPtr blob = blobify_callback_.Run(
          reinterpret_cast<uint8_t*>(buffer->memory(0)), header->jpeg_size,
          stream_buffer_manager_->GetStreamCaptureFormat(stream_type), 0);
      if (blob) {
        std::move(pending_result.still_capture_callback).Run(std::move(blob));
      } else {
        // TODO(wtlee): If it is fatal, we should set error state here.
        LOG(ERROR) << "Failed to blobify the captured JPEG image";
      }
    }
  }
  stream_buffer_manager_->ReleaseBuffer(stream_type, buffer_id);
  pending_result.unsubmitted_buffer_count--;

  if (pending_result.unsubmitted_buffer_count == 0) {
    pending_results_.erase(frame_number);
  }
  // Every time a buffer is released, try to prepare another capture request
  // again.
  PrepareCaptureRequest();
}

size_t RequestManager::GetNumberOfStreams() {
  return stream_buffer_manager_->GetNumberOfStreams();
}

void RequestManager::AddResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(!result_metadata_observers_.count(observer));

  result_metadata_observers_.insert(observer);
}

void RequestManager::RemoveResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(result_metadata_observers_.count(observer));

  result_metadata_observers_.erase(observer);
}

void RequestManager::SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                        cros::mojom::EntryType type,
                                        size_t count,
                                        std::vector<uint8_t> value) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  cros::mojom::CameraMetadataEntryPtr setting =
      cros::mojom::CameraMetadataEntry::New();

  setting->tag = tag;
  setting->type = type;
  setting->count = count;
  setting->data = std::move(value);

  capture_settings_override_.push_back(std::move(setting));
}

void RequestManager::SetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag,
    cros::mojom::EntryType type,
    size_t count,
    std::vector<uint8_t> value) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  cros::mojom::CameraMetadataEntryPtr setting =
      cros::mojom::CameraMetadataEntry::New();

  setting->tag = tag;
  setting->type = type;
  setting->count = count;
  setting->data = std::move(value);

  capture_settings_repeating_override_[tag] = std::move(setting);
}

void RequestManager::UnsetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  auto it = capture_settings_repeating_override_.find(tag);
  if (it == capture_settings_repeating_override_.end()) {
    LOG(ERROR) << "Unset a non-existent metadata: " << tag;
    return;
  }
  capture_settings_repeating_override_.erase(it);
}

void RequestManager::UpdateCaptureSettings(
    cros::mojom::CameraMetadataPtr* capture_settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (capture_settings_override_.empty() &&
      capture_settings_repeating_override_.empty()) {
    return;
  }

  for (const auto& setting : capture_settings_repeating_override_) {
    AddOrUpdateMetadataEntry(capture_settings, setting.second.Clone());
  }

  for (auto& s : capture_settings_override_) {
    AddOrUpdateMetadataEntry(capture_settings, std::move(s));
  }
  capture_settings_override_.clear();
  SortCameraMetadata(capture_settings);
}

RequestManager::CaptureResult::CaptureResult()
    : metadata(cros::mojom::CameraMetadata::New()),
      unsubmitted_buffer_count(0) {}

RequestManager::CaptureResult::~CaptureResult() = default;

}  // namespace media
