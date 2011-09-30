// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CAPTURE_DATA_H_
#define REMOTING_BASE_CAPTURE_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

struct DataPlanes {
  DataPlanes();

  static const int kPlaneCount = 3;
  uint8* data[kPlaneCount];
  int strides[kPlaneCount];
};

// Stores the data and information of a capture to pass off to the
// encoding thread.
class CaptureData : public base::RefCountedThreadSafe<CaptureData> {
 public:
  CaptureData(const DataPlanes &data_planes,
              const SkISize& size,
              media::VideoFrame::Format format);

  // Get the data_planes data of the previous capture.
  const DataPlanes& data_planes() const { return data_planes_; }

  // Get the dirty region from the previous capture.
  const SkRegion& dirty_region() const { return dirty_region_; }

  // Return the size of the image captured.
  SkISize size() const { return size_; }

  // Get the pixel format of the image captured.
  media::VideoFrame::Format pixel_format() const { return pixel_format_; }

  // Mutating methods.
  SkRegion& mutable_dirty_region() { return dirty_region_; }

  // Return the time spent on capturing.
  int capture_time_ms() const { return capture_time_ms_; }

  // Set the time spent on capturing.
  void set_capture_time_ms(int capture_time_ms) {
    capture_time_ms_ = capture_time_ms;
  }

  int64 client_sequence_number() const { return client_sequence_number_; }

  void set_client_sequence_number(int64 client_sequence_number) {
    client_sequence_number_ = client_sequence_number;
  }

 private:
  const DataPlanes data_planes_;
  SkRegion dirty_region_;
  SkISize size_;
  media::VideoFrame::Format pixel_format_;

  // Time spent in capture. Unit is in milliseconds.
  int capture_time_ms_;

  // Sequence number supplied by client for performance tracking.
  int64 client_sequence_number_;

  friend class base::RefCountedThreadSafe<CaptureData>;
  virtual ~CaptureData();
};

}  // namespace remoting

#endif  // REMOTING_BASE_CAPTURE_DATA_H_
