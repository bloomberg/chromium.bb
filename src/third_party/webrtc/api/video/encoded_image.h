/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_ENCODED_IMAGE_H_
#define API_VIDEO_ENCODED_IMAGE_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/color_space.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// TODO(bug.webrtc.org/9378): This is a legacy api class, which is slowly being
// cleaned up. Direct use of its members is strongly discouraged.
class RTC_EXPORT EncodedImage {
 public:
  EncodedImage();
  EncodedImage(EncodedImage&&);
  // Discouraged: potentially expensive.
  EncodedImage(const EncodedImage&);
  EncodedImage(uint8_t* buffer, size_t length, size_t capacity);

  ~EncodedImage();

  EncodedImage& operator=(EncodedImage&&);
  // Discouraged: potentially expensive.
  EncodedImage& operator=(const EncodedImage&);

  // TODO(nisse): Change style to timestamp(), set_timestamp(), for consistency
  // with the VideoFrame class.
  // Set frame timestamp (90kHz).
  void SetTimestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

  // Get frame timestamp (90kHz).
  uint32_t Timestamp() const { return timestamp_rtp_; }

  void SetEncodeTime(int64_t encode_start_ms, int64_t encode_finish_ms);

  absl::optional<int> SpatialIndex() const {
    return spatial_index_;
  }
  void SetSpatialIndex(absl::optional<int> spatial_index) {
    RTC_DCHECK_GE(spatial_index.value_or(0), 0);
    RTC_DCHECK_LT(spatial_index.value_or(0), kMaxSpatialLayers);
    spatial_index_ = spatial_index;
  }

  const webrtc::ColorSpace* ColorSpace() const {
    return color_space_ ? &*color_space_ : nullptr;
  }
  void SetColorSpace(const absl::optional<webrtc::ColorSpace>& color_space) {
    color_space_ = color_space;
  }

  size_t size() const { return size_; }
  void set_size(size_t new_size) {
    RTC_DCHECK_LE(new_size, capacity());
    size_ = new_size;
  }
  size_t capacity() const { return buffer_ ? capacity_ : encoded_data_.size(); }

  void set_buffer(uint8_t* buffer, size_t capacity) {
    buffer_ = buffer;
    capacity_ = capacity;
  }

  void Allocate(size_t capacity) {
    encoded_data_.SetSize(capacity);
    buffer_ = nullptr;
  }

  uint8_t* data() { return buffer_ ? buffer_ : encoded_data_.data(); }
  const uint8_t* data() const {
    return buffer_ ? buffer_ : encoded_data_.cdata();
  }
  // TODO(nisse): At some places, code accepts a const ref EncodedImage, but
  // still writes to it, to clear padding at the end of the encoded data.
  // Padding is required by ffmpeg; the best way to deal with that is likely to
  // make this class ensure that buffers always have a few zero padding bytes.
  uint8_t* mutable_data() const { return const_cast<uint8_t*>(data()); }

  // TODO(bugs.webrtc.org/9378): Delete. Used by code that wants to modify a
  // buffer corresponding to a const EncodedImage. Requires an un-owned buffer.
  uint8_t* buffer() const { return buffer_; }

  // Hack to workaround lack of ownership of the encoded data. If we don't
  // already own the underlying data, make an owned copy.
  void Retain();

  uint32_t _encodedWidth = 0;
  uint32_t _encodedHeight = 0;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms_ = 0;
  int64_t capture_time_ms_ = 0;
  VideoFrameType _frameType = VideoFrameType::kVideoFrameDelta;
  VideoRotation rotation_ = kVideoRotation_0;
  VideoContentType content_type_ = VideoContentType::UNSPECIFIED;
  bool _completeFrame = false;
  int qp_ = -1;  // Quantizer value.

  // When an application indicates non-zero values here, it is taken as an
  // indication that all future frames will be constrained with those limits
  // until the application indicates a change again.
  PlayoutDelay playout_delay_ = {-1, -1};

  struct Timing {
    uint8_t flags = VideoSendTiming::kInvalid;
    int64_t encode_start_ms = 0;
    int64_t encode_finish_ms = 0;
    int64_t packetization_finish_ms = 0;
    int64_t pacer_exit_ms = 0;
    int64_t network_timestamp_ms = 0;
    int64_t network2_timestamp_ms = 0;
    int64_t receive_start_ms = 0;
    int64_t receive_finish_ms = 0;
  } timing_;

 private:
  // TODO(bugs.webrtc.org/9378): We're transitioning to always owning the
  // encoded data.
  rtc::CopyOnWriteBuffer encoded_data_;
  size_t size_;      // Size of encoded frame data.
  // Non-null when used with an un-owned buffer.
  uint8_t* buffer_;
  // Allocated size of _buffer; relevant only if it's non-null.
  size_t capacity_;
  uint32_t timestamp_rtp_ = 0;
  absl::optional<int> spatial_index_;
  absl::optional<webrtc::ColorSpace> color_space_;
};

}  // namespace webrtc

#endif  // API_VIDEO_ENCODED_IMAGE_H_
