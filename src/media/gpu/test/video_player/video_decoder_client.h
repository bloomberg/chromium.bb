// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

class GpuVideoDecodeAcceleratorFactory;
class VideoFrame;

namespace test {

class EncodedDataHelper;
class FrameRenderer;
class VideoFrameProcessor;

// TODO(dstaessens@) Remove allocation mode, temporary added here so we can
// support the thumbnail test for older platforms that don't support import.
enum class AllocationMode {
  kImport,    // Client allocates video frame memory.
  kAllocate,  // Video decoder allocates video frame memory.
};

// Video decoder client configuration.
struct VideoDecoderClientConfig {
  // The maximum number of bitstream buffer decodes that can be requested
  // without waiting for the result of the previous decode requests.
  size_t max_outstanding_decode_requests = 1;
  // How the pictures buffers should be allocated.
  AllocationMode allocation_mode = AllocationMode::kImport;
};

// The video decoder client is responsible for the communication between the
// video player and the video decoder. It also communicates with the frame
// renderer and other components. The video decoder client can only have one
// active decoder at any time. To decode a different stream the DestroyDecoder()
// and CreateDecoder() functions have to be called to destroy and re-create the
// decoder.
//
// All communication with the decoder is done on the |decoder_client_thread_|,
// so callbacks scheduled by the decoder can be executed asynchronously. This is
// necessary if we don't want to interrupt the test flow.
class VideoDecoderClient : public VideoDecodeAccelerator::Client {
 public:
  ~VideoDecoderClient() override;

  // Return an instance of the VideoDecoderClient. The |frame_renderer| and
  // |frame_processors| will not be owned by the decoder client, the caller
  // should guarantee they outlive the decoder client. The |event_cb| will be
  // called whenever an event occurs (e.g. frame decoded) and should be
  // thread-safe.
  static std::unique_ptr<VideoDecoderClient> Create(
      const VideoPlayer::EventCallback& event_cb,
      std::unique_ptr<FrameRenderer> frame_renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const VideoDecoderClientConfig& config);

  // Create a decoder with specified |config| and video |stream|. The video
  // will not be owned by the decoder client, the caller should guarantee it
  // exists until DestroyDecoder() is called.
  void CreateDecoder(const VideoDecodeAccelerator::Config& config,
                     const std::vector<uint8_t>& stream);
  // Destroy the currently active decoder.
  void DestroyDecoder();

  // Wait until all frame processors have finished processing. Returns whether
  // processing was successful.
  bool WaitForFrameProcessors();
  // Get the frame renderer associated with the video decoder client.
  FrameRenderer* GetFrameRenderer() const;

  // Start decoding the video stream, decoder should be idle when this function
  // is called. This function is non-blocking, for each frame decoded a
  // 'kFrameDecoded' event will be thrown.
  void Play();
  // Queue decoder flush. This function is non-blocking, a kFlushing/kFlushDone
  // event is thrown upon start/finish.
  void Flush();
  // Queue decoder reset. This function is non-blocking, a kResetting/kResetDone
  // event is thrown upon start/finish.
  void Reset();

 private:
  enum class VideoDecoderClientState : size_t {
    kUninitialized = 0,
    kIdle,
    kDecoding,
    kFlushing,
    kResetting,
  };

  VideoDecoderClient(
      const VideoPlayer::EventCallback& event_cb,
      std::unique_ptr<FrameRenderer> renderer,
      std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
      const VideoDecoderClientConfig& config);

  bool Initialize();

  // VideoDecodeAccelerator::Client implementation
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat pixel_format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& size,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  void CreateDecoderFactoryTask(base::WaitableEvent* done);
  void CreateDecoderTask(VideoDecodeAccelerator::Config config,
                         const std::vector<uint8_t>* stream,
                         base::WaitableEvent* done);
  void DestroyDecoderTask(base::WaitableEvent* done);

  // Instruct the decoder to decode the next video stream fragment.
  void DecodeNextFragmentTask();
  // Start decoding video stream fragments.
  void PlayTask();
  // Instruct the decoder to perform a flush.
  void FlushTask();
  // Instruct the decoder to perform a Reset.
  void ResetTask();

  // Fire the specified event.
  void FireEvent(VideoPlayerEvent event);

  // Called when a picture buffer is ready to be re-used.
  void ReusePictureBufferTask(int32_t picture_buffer_id);

  // Get the next bitstream buffer id to be used.
  int32_t GetNextBitstreamBufferId();
  // Get the next picture buffer id to be used.
  int32_t GetNextPictureBufferId();

  VideoPlayer::EventCallback event_cb_;
  std::unique_ptr<FrameRenderer> const frame_renderer_;
  std::vector<std::unique_ptr<VideoFrameProcessor>> const frame_processors_;

  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> decoder_factory_;
  std::unique_ptr<VideoDecodeAccelerator> decoder_;
  VideoDecodeAccelerator::Config decoder_config_;
  base::Thread decoder_client_thread_;

  // Decoder client state, should only be accessed on the decoder client thread.
  VideoDecoderClientState decoder_client_state_;

  // Map of video frames the decoder uses as output, keyed on picture buffer id.
  std::map<int32_t, scoped_refptr<VideoFrame>> video_frames_;

  int32_t next_bitstream_buffer_id_ = 0;
  int32_t next_picture_buffer_id_ = 0;

  // Index of the frame that's currently being decoded.
  size_t current_frame_index_ = 0;
  // The current number of outgoing bitstream buffers decode requests.
  size_t num_outstanding_decode_requests_ = 0;
  // Video decoder client configuration.
  const VideoDecoderClientConfig decoder_client_config_;

  // TODO(dstaessens@) Replace with StreamParser.
  std::unique_ptr<media::test::EncodedDataHelper> encoded_data_helper_;

  SEQUENCE_CHECKER(video_player_sequence_checker_);
  SEQUENCE_CHECKER(decoder_client_sequence_checker_);

  base::WeakPtr<VideoDecoderClient> weak_this_;
  base::WeakPtrFactory<VideoDecoderClient> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderClient);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
