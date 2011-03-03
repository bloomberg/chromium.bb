// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CAPTURE_DATA_H_
#define REMOTING_BASE_CAPTURE_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "media/base/video_frame.h"
#include "remoting/base/types.h"

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
              int width,
              int height,
              media::VideoFrame::Format format);

  // Get the data_planes data of the last capture.
  const DataPlanes& data_planes() const { return data_planes_; }

  // Get the list of updated rectangles in the last capture. The result is
  // written into |rects|.
  const InvalidRects& dirty_rects() const { return dirty_rects_; }

  // Get the width of the image captured.
  int width() const { return width_; }

  // Get the height of the image captured.
  int height() const { return height_; }

  // Get the pixel format of the image captured.
  media::VideoFrame::Format pixel_format() const { return pixel_format_; }

  // Mutating methods.
  InvalidRects& mutable_dirty_rects() { return dirty_rects_; }

 private:
  const DataPlanes data_planes_;
  InvalidRects dirty_rects_;
  int width_;
  int height_;
  media::VideoFrame::Format pixel_format_;

  friend class base::RefCountedThreadSafe<CaptureData>;
  virtual ~CaptureData();
};

}  // namespace remoting

#endif  // REMOTING_BASE_CAPTURE_DATA_H_
