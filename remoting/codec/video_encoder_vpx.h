// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_VPX_H_
#define REMOTING_CODEC_VIDEO_ENCODER_VPX_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_encoder.h"

typedef struct vpx_image vpx_image_t;

namespace webrtc {
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class VideoEncoderVpx : public VideoEncoder {
 public:
  // Create encoder for the specified protocol.
  static scoped_ptr<VideoEncoderVpx> CreateForVP8();
  static scoped_ptr<VideoEncoderVpx> CreateForVP9I420();
  static scoped_ptr<VideoEncoderVpx> CreateForVP9I444();

  virtual ~VideoEncoderVpx();

  // VideoEncoder interface.
  virtual scoped_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) OVERRIDE;

 private:
  typedef base::Callback<ScopedVpxCodec(const webrtc::DesktopSize&)>
      CreateCodecCallback;
  typedef base::Callback<void(const webrtc::DesktopSize&,
                              scoped_ptr<vpx_image_t>* out_image,
                              scoped_ptr<uint8[]>* out_image_buffer)>
      CreateImageCallback;

  VideoEncoderVpx(const CreateCodecCallback& create_codec,
                  const CreateImageCallback& create_image);

  // Initializes the codec for frames of |size|. Returns true if successful.
  bool Initialize(const webrtc::DesktopSize& size);

  // Prepares |image_| for encoding. Writes updated rectangles into
  // |updated_region|.
  void PrepareImage(const webrtc::DesktopFrame& frame,
                    webrtc::DesktopRegion* updated_region);

  // Updates the active map according to |updated_region|. Active map is then
  // given to the encoder to speed up encoding.
  void PrepareActiveMap(const webrtc::DesktopRegion& updated_region);

  CreateCodecCallback create_codec_;
  CreateImageCallback create_image_;

  ScopedVpxCodec codec_;
  base::TimeTicks timestamp_base_;

  // VPX image and buffer to hold the actual YUV planes.
  scoped_ptr<vpx_image_t> image_;
  scoped_ptr<uint8[]> image_buffer_;

  // Active map used to optimize out processing of un-changed macroblocks.
  scoped_ptr<uint8[]> active_map_;
  int active_map_width_;
  int active_map_height_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderVpx);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_VP8_H_
