// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"

#include <va/va.h>

#include "base/memory/ref_counted_memory.h"
#include "media/base/video_frame.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    scoped_refptr<VideoFrame> input_frame,
    bool keyframe,
    scoped_refptr<VASurface> input_surface,
    scoped_refptr<CodecPicture> picture,
    std::unique_ptr<ScopedVABuffer> coded_buffer)
    : input_frame_(input_frame),
      keyframe_(keyframe),
      input_surface_(input_surface),
      picture_(std::move(picture)),
      coded_buffer_(std::move(coded_buffer)) {
  DCHECK(input_surface_);
  DCHECK(picture_);
  DCHECK(coded_buffer_);
}

VaapiVideoEncoderDelegate::EncodeJob::EncodeJob(
    scoped_refptr<VideoFrame> input_frame,
    bool keyframe)
    : input_frame_(input_frame), keyframe_(keyframe) {}

VaapiVideoEncoderDelegate::EncodeJob::~EncodeJob() = default;

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeResult>
VaapiVideoEncoderDelegate::EncodeJob::CreateEncodeResult(
    const BitstreamBufferMetadata& metadata) && {
  return std::make_unique<EncodeResult>(input_surface_,
                                        std::move(coded_buffer_), metadata);
}

base::TimeDelta VaapiVideoEncoderDelegate::EncodeJob::timestamp() const {
  return input_frame_->timestamp();
}

const scoped_refptr<VideoFrame>&
VaapiVideoEncoderDelegate::EncodeJob::input_frame() const {
  return input_frame_;
}

VABufferID VaapiVideoEncoderDelegate::EncodeJob::coded_buffer_id() const {
  return coded_buffer_->id();
}

const scoped_refptr<VASurface>&
VaapiVideoEncoderDelegate::EncodeJob::input_surface() const {
  return input_surface_;
}

const scoped_refptr<CodecPicture>&
VaapiVideoEncoderDelegate::EncodeJob::picture() const {
  return picture_;
}

VaapiVideoEncoderDelegate::EncodeResult::EncodeResult(
    scoped_refptr<VASurface> surface,
    std::unique_ptr<ScopedVABuffer> coded_buffer,
    const BitstreamBufferMetadata& metadata)
    : surface_(std::move(surface)),
      coded_buffer_(std::move(coded_buffer)),
      metadata_(metadata) {}

VaapiVideoEncoderDelegate::EncodeResult::~EncodeResult() = default;

VASurfaceID VaapiVideoEncoderDelegate::EncodeResult::input_surface_id() const {
  return surface_->id();
}

VABufferID VaapiVideoEncoderDelegate::EncodeResult::coded_buffer_id() const {
  return coded_buffer_->id();
}

const BitstreamBufferMetadata&
VaapiVideoEncoderDelegate::EncodeResult::metadata() const {
  return metadata_;
}

VaapiVideoEncoderDelegate::VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : vaapi_wrapper_(vaapi_wrapper), error_cb_(error_cb) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiVideoEncoderDelegate::~VaapiVideoEncoderDelegate() = default;

size_t VaapiVideoEncoderDelegate::GetBitstreamBufferSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetEncodeBitstreamBufferSize(GetCodedSize());
}

void VaapiVideoEncoderDelegate::BitrateControlUpdate(
    uint64_t encoded_chunk_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BitstreamBufferMetadata VaapiVideoEncoderDelegate::GetMetadata(
    const EncodeJob& encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BitstreamBufferMetadata(payload_size, encode_job.IsKeyframeRequested(),
                                 encode_job.timestamp());
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeResult>
VaapiVideoEncoderDelegate::Encode(std::unique_ptr<EncodeJob> encode_job) {
  if (!PrepareEncodeJob(*encode_job)) {
    VLOGF(1) << "Failed preparing an encode job";
    return nullptr;
  }

  const VASurfaceID va_surface_id = encode_job->input_surface()->id();
  if (!native_input_mode_ && !vaapi_wrapper_->UploadVideoFrameToSurface(
                                 *encode_job->input_frame(), va_surface_id,
                                 encode_job->input_surface()->size())) {
    VLOGF(1) << "Failed to upload frame";
    return nullptr;
  }

  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id)) {
    VLOGF(1) << "Failed to execute encode";
    return nullptr;
  }

  const uint64_t encoded_chunk_size = vaapi_wrapper_->GetEncodedChunkSize(
      encode_job->coded_buffer_id(), va_surface_id);
  if (encoded_chunk_size == 0) {
    VLOGF(1) << "Invalid encoded chunk size";
    return nullptr;
  }

  BitrateControlUpdate(encoded_chunk_size);

  auto metadata = GetMetadata(*encode_job, encoded_chunk_size);
  return std::move(*encode_job).CreateEncodeResult(metadata);
}

}  // namespace media
