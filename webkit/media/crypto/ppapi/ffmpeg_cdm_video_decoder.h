// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_VIDEO_DECODER_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "webkit/media/crypto/ppapi/content_decryption_module.h"

struct AVCodecContext;
struct AVFrame;

namespace webkit_media {

class FFmpegCdmVideoDecoder {
 public:
  explicit FFmpegCdmVideoDecoder(cdm::Allocator* allocator);
  ~FFmpegCdmVideoDecoder();
  bool Initialize(const cdm::VideoDecoderConfig& config);
  void Deinitialize();
  void Reset();

  // Returns true when |format| and |data_size| specify a supported video
  // output configuration.
  static bool IsValidOutputConfig(cdm::VideoFormat format,
                                  const cdm::Size& data_size);

  // Decodes |compressed_frame|. Stores output frame in |decoded_frame| and
  // returns |cdm::kSuccess| when an output frame is available. Returns
  // |cdm::kNeedMoreData| when |compressed_frame| does not produce an output
  // frame. Returns |cdm::kDecodeError| when decoding fails.
  cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                          int32_t compressed_frame_size,
                          int64_t timestamp,
                          cdm::VideoFrame* decoded_frame);

 private:
  // Allocates storage, then copies video frame stored in |av_frame_| to
  // |cdm_video_frame|. Returns true when allocation and copy succeed.
  bool CopyAvFrameTo(cdm::VideoFrame* cdm_video_frame);

  void ReleaseFFmpegResources();

  // FFmpeg structures owned by this object.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool is_initialized_;

  cdm::Allocator* const allocator_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCdmVideoDecoder);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_VIDEO_DECODER_H_
