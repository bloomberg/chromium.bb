// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/decode_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoFrame;
}  // namespace media

namespace content {

// This class decodes video for WebRTC using a media::VideoDecoder. In
// particular, either GpuVideoDecoder or MojoVideoDecoder are used to provide
// access to hardware decoding in the GPU process.
//
// Lifecycle methods are called on the WebRTC worker thread. Decoding happens on
// a WebRTC DecodingThread, which is an rtc::PlatformThread owend by WebRTC; it
// does not have a TaskRunner.
//
// To stop decoding, WebRTC stops the DecodingThread and then calls Release() on
// the worker. Calling the DecodedImageCallback after the DecodingThread is
// stopped is illegal but, because we decode on the media thread, there is no
// way to synchronize this correctly.
class CONTENT_EXPORT RTCVideoDecoderAdapter : public webrtc::VideoDecoder {
 public:
  using CreateVideoDecoderCB =
      base::RepeatingCallback<std::unique_ptr<media::VideoDecoder>(
          media::MediaLog*)>;

  // Creates and initializes an RTCVideoDecoderAdapter. Returns nullptr if
  // |video_codec_type| cannot be supported.
  // Called on the worker thread.
  static std::unique_ptr<RTCVideoDecoderAdapter> Create(
      webrtc::VideoCodecType video_codec_type,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      CreateVideoDecoderCB create_video_decoder_cb);

  // Called on the worker thread.
  static void DeleteSoonOnMediaThread(
      std::unique_ptr<webrtc::VideoDecoder> rtc_video_decoder_adapter,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  // Called on |media_task_runner_|.
  ~RTCVideoDecoderAdapter() override;

  // webrtc::VideoDecoder implementation.
  // Called on the DecodingThread.
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  // Called on the DecodingThread.
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  // Called on the DecodingThread.
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;
  // Called on the worker thread.
  int32_t Release() override;
  // Called on the worker thread and on the DecodingThread.
  const char* ImplementationName() const override;

 private:
  // |create_video_decoder_cb| will always be called on |media_task_runner|.
  // Called on the worker thread.
  RTCVideoDecoderAdapter(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      CreateVideoDecoderCB create_video_decoder_cb,
      webrtc::VideoCodecType video_codec_type);

  bool InitializeSync();
  void InitializeOnMediaThread(media::VideoDecoder::InitCB init_cb);
  void DecodeOnMediaThread();
  void OnDecodeDone(media::DecodeStatus status);
  void OnOutput(const scoped_refptr<media::VideoFrame>& frame);

  // Construction parameters.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  CreateVideoDecoderCB create_video_decoder_cb_;
  webrtc::VideoCodecType video_codec_type_;

  // Media thread members.
  // |media_log_| must outlive |video_decoder_| because it is passed as a raw
  // pointer.
  std::unique_ptr<media::MediaLog> media_log_;
  std::unique_ptr<media::VideoDecoder> video_decoder_;
  int32_t outstanding_decode_requests_ = 0;

  // Shared members.
  base::Lock lock_;
  int32_t consecutive_error_count_ = 0;
  bool has_error_ = false;
  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;
  // Requests that have not been submitted to the decoder yet.
  base::circular_deque<scoped_refptr<media::DecoderBuffer>> pending_buffers_;
  // Record of timestamps that have been sent to be decoded. Removing a
  // timestamp will cause the frame to be dropped when it is output.
  base::circular_deque<base::TimeDelta> decode_timestamps_;

  // Thread management.
  THREAD_CHECKER(worker_thread_checker_);
  THREAD_CHECKER(decoding_thread_checker_);

  base::WeakPtr<RTCVideoDecoderAdapter> weak_this_;
  base::WeakPtrFactory<RTCVideoDecoderAdapter> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_
