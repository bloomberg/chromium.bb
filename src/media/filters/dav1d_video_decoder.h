// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"

struct Dav1dContext;
struct Dav1dPicture;

namespace media {
class MediaLog;

class MEDIA_EXPORT Dav1dVideoDecoder : public VideoDecoder {
 public:
  explicit Dav1dVideoDecoder(MediaLog* media_log);
  ~Dav1dVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::RepeatingClosure& reset_cb) override;

 private:
  enum class DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Releases any configured decoder and clears |dav1d_decoder_|.
  void CloseDecoder();

  // Invokes the decoder and calls |output_cb_| for any returned frames.
  bool DecodeBuffer(scoped_refptr<DecoderBuffer> buffer);

  scoped_refptr<VideoFrame> CopyImageToVideoFrame(const Dav1dPicture* img);

  THREAD_CHECKER(thread_checker_);

  // Used to report error messages to the client.
  MediaLog* const media_log_ = nullptr;

  // Current decoder state. Used to ensure methods are called as expected.
  DecoderState state_ = DecoderState::kUninitialized;

  // Callback given during Initialize() used for delivering decoded frames.
  OutputCB output_cb_;

  // The configuration passed to Initialize(), saved since some fields are
  // needed to annotate video frames after decoding.
  VideoDecoderConfig config_;

  // The allocated decoder; null before Initialize() and anytime after
  // CloseDecoder().
  Dav1dContext* dav1d_decoder_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Dav1dVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
