// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_encode_accelerator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/clients/mojo_media_log_service.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/mojom/video_encoder_info.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

// File-static mojom::VideoEncodeAcceleratorClient implementation to trampoline
// method calls to its |client_|. Note that this class is thread hostile when
// bound.
class VideoEncodeAcceleratorClient
    : public mojom::VideoEncodeAcceleratorClient {
 public:
  VideoEncodeAcceleratorClient(
      VideoEncodeAccelerator::Client* client,
      mojo::PendingAssociatedReceiver<mojom::VideoEncodeAcceleratorClient>
          receiver);

  VideoEncodeAcceleratorClient(const VideoEncodeAcceleratorClient&) = delete;
  VideoEncodeAcceleratorClient& operator=(const VideoEncodeAcceleratorClient&) =
      delete;

  ~VideoEncodeAcceleratorClient() override = default;

  // mojom::VideoEncodeAcceleratorClient impl.
  void RequireBitstreamBuffers(uint32_t input_count,
                               const gfx::Size& input_coded_size,
                               uint32_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(VideoEncodeAccelerator::Error error) override;
  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

 private:
  raw_ptr<VideoEncodeAccelerator::Client> client_;
  mojo::AssociatedReceiver<mojom::VideoEncodeAcceleratorClient> receiver_;
};

VideoEncodeAcceleratorClient::VideoEncodeAcceleratorClient(
    VideoEncodeAccelerator::Client* client,
    mojo::PendingAssociatedReceiver<mojom::VideoEncodeAcceleratorClient>
        receiver)
    : client_(client), receiver_(this, std::move(receiver)) {
  DCHECK(client_);
}

void VideoEncodeAcceleratorClient::RequireBitstreamBuffers(
    uint32_t input_count,
    const gfx::Size& input_coded_size,
    uint32_t output_buffer_size) {
  DVLOG(2) << __func__ << " input_count= " << input_count
           << " input_coded_size= " << input_coded_size.ToString()
           << " output_buffer_size=" << output_buffer_size;
  client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                   output_buffer_size);
}

void VideoEncodeAcceleratorClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << "B,  key_frame=" << metadata.key_frame;
  client_->BitstreamBufferReady(bitstream_buffer_id, metadata);
}

void VideoEncodeAcceleratorClient::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DVLOG(2) << __func__;
  client_->NotifyError(error);
}

void VideoEncodeAcceleratorClient::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  DVLOG(2) << __func__;
  client_->NotifyEncoderInfoChange(info);
}

}  // anonymous namespace

MojoVideoEncodeAccelerator::MojoVideoEncodeAccelerator(
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> vea)
    : vea_(std::move(vea)) {
  DVLOG(1) << __func__;
  DCHECK(vea_);
}

VideoEncodeAccelerator::SupportedProfiles
MojoVideoEncodeAccelerator::GetSupportedProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTREACHED() << "GetSupportedProfiles() should never be called."
               << "Use VEA provider or GPU factories";
  return {};
}

bool MojoVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(2) << __func__ << " " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client)
    return false;

  // Get a mojom::VideoEncodeAcceleratorClient bound to a local implementation
  // (VideoEncodeAcceleratorClient) and send the remote.
  mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient>
      vea_client_remote;
  vea_client_ = std::make_unique<VideoEncodeAcceleratorClient>(
      client, vea_client_remote.InitWithNewEndpointAndPassReceiver());

  vea_.set_disconnect_handler(
      base::BindOnce(&MojoVideoEncodeAccelerator::MojoDisconnectionHandler,
                     base::Unretained(this)));

  // NullMediaLog silently and safely does nothing.
  if (!media_log)
    media_log = std::make_unique<media::NullMediaLog>();

  // Use `mojo::MakeSelfOwnedReceiver` for MediaLog so logs may go through even
  // after `MojoVideoEncodeAccelerator` is destructed.
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoMediaLogService>(media_log->Clone()),
      std::move(media_log_pending_receiver));

  bool result = false;
  vea_->Initialize(config, std::move(vea_client_remote),
                   std::move(media_log_pending_remote), &result);
  return result;
}

void MojoVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DVLOG(2) << __func__ << " tstamp=" << frame->timestamp();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t num_planes = VideoFrame::NumPlanes(frame->format());
  DCHECK_EQ(num_planes, frame->layout().num_planes());
  DCHECK(vea_.is_bound());

  // GPU memory path: Pass-through.
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    vea_->Encode(frame, force_keyframe,
                 base::BindOnce([](scoped_refptr<VideoFrame>) {}, frame));
    return;
  }

  // Mappable memory path: Copy buffer to shared memory.
  if (frame->format() != PIXEL_FORMAT_I420 &&
      frame->format() != PIXEL_FORMAT_NV12) {
    DLOG(ERROR) << "Unexpected pixel format: "
                << VideoPixelFormatToString(frame->format());
    return;
  }
  if (frame->storage_type() != VideoFrame::STORAGE_SHMEM &&
      frame->storage_type() != VideoFrame::STORAGE_UNOWNED_MEMORY) {
    DLOG(ERROR) << "Unexpected storage type: "
                << VideoFrame::StorageTypeToString(frame->storage_type());
    return;
  }

  scoped_refptr<MojoSharedBufferVideoFrame> mojo_frame =
      MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame);
  if (!mojo_frame) {
    DLOG(ERROR) << "Failed creating MojoSharedBufferVideoFrame from YUV";
    return;
  }
  vea_->Encode(
      std::move(mojo_frame), force_keyframe,
      base::BindOnce([](scoped_refptr<VideoFrame>) {}, std::move(frame)));
}

void MojoVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(2) << __func__ << " buffer.id()= " << buffer.id()
           << " buffer.size()= " << buffer.size() << "B";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(buffer.region().IsValid());

  auto buffer_handle =
      mojo::WrapPlatformSharedMemoryRegion(buffer.TakeRegion());

  vea_->UseOutputBitstreamBuffer(buffer.id(), std::move(buffer_handle));
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->RequestEncodingParametersChangeWithBitrate(bitrate, framerate);
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->RequestEncodingParametersChangeWithLayers(bitrate, framerate);
}

bool MojoVideoEncodeAccelerator::IsFlushSupported() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  bool flush_support = false;
  vea_->IsFlushSupported(&flush_support);
  return flush_support;
}

void MojoVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->Flush(std::move(flush_callback));
}

void MojoVideoEncodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vea_client_.reset();
  vea_.reset();
  // See media::VideoEncodeAccelerator for more info on this peculiar pattern.
  delete this;
}

MojoVideoEncodeAccelerator::~MojoVideoEncodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoVideoEncodeAccelerator::MojoDisconnectionHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (vea_client_) {
    vea_client_->NotifyError(
        VideoEncodeAccelerator::Error::kPlatformFailureError);
  }
}

}  // namespace media
