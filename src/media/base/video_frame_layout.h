// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
#define MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A class to describes how physical buffer is allocated for video frame.
// In stores format, coded size of the frame and size of physical buffers
// which can be used to allocate buffer(s) hardware expected.
// It also stores stride (bytes per line) and offset per color plane as Plane.
// stride is to calculate each color plane's size (note that a buffer may
// contains multiple color planes.)
// offset is to describe a start point of each plane from buffer's dmabuf fd.
// Note that it is copyable.
class MEDIA_EXPORT VideoFrameLayout {
 public:
  struct Plane {
    Plane() = default;
    Plane(int32_t stride, size_t offset) : stride(stride), offset(offset) {}

    bool operator==(const Plane& rhs) const {
      return stride == rhs.stride && offset == rhs.offset;
    }

    // Strides of a plane, typically greater or equal to the
    // width of the surface divided by the horizontal sampling period. Note that
    // strides can be negative if the image layout is bottom-up.
    int32_t stride = 0;

    // Offset of a plane, which stands for the offset of a start point of a
    // color plane from a buffer fd.
    size_t offset = 0;
  };

  enum {
    kDefaultPlaneCount = 4,
    kDefaultBufferCount = 4,
  };

  // Constructor with strides and buffers' size.
  // If strides and buffer_sizes are not assigned, strides, offsets and
  // buffer_sizes are {0, 0, 0, 0}.
  VideoFrameLayout(VideoPixelFormat format,
                   const gfx::Size& coded_size,
                   std::vector<int32_t> strides =
                       std::vector<int32_t>(kDefaultPlaneCount, 0),
                   std::vector<size_t> buffer_sizes =
                       std::vector<size_t>(kDefaultBufferCount, 0));

  // Constructor with plane's stride/offset, and buffers' size.
  // If buffer_sizes are not assigned, it is {0, 0, 0, 0}.
  VideoFrameLayout(VideoPixelFormat format,
                   const gfx::Size& coded_size,
                   std::vector<Plane> planes,
                   std::vector<size_t> buffer_sizes =
                       std::vector<size_t>(kDefaultBufferCount, 0));

  VideoFrameLayout();
  VideoFrameLayout(const VideoFrameLayout&);
  VideoFrameLayout(VideoFrameLayout&&);
  VideoFrameLayout& operator=(const VideoFrameLayout&);
  ~VideoFrameLayout();

  VideoPixelFormat format() const { return format_; }
  const gfx::Size& coded_size() const { return coded_size_; }

  // Return number of buffers. Note that num_planes >= num_buffers.
  size_t num_buffers() const { return buffer_sizes_.size(); }

  // Returns number of planes. Note that num_planes >= num_buffers.
  size_t num_planes() const { return planes_.size(); }

  const std::vector<Plane>& planes() const { return planes_; }
  const std::vector<size_t>& buffer_sizes() const { return buffer_sizes_; }

  // Returns sum of bytes of all buffers.
  size_t GetTotalBufferSize() const;

  // Composes VideoFrameLayout as human readable string.
  std::string ToString() const;

  // Returns false if it is invalid.
  bool IsValid() const { return format_ != PIXEL_FORMAT_UNKNOWN; }

 private:
  VideoPixelFormat format_;

  // Width and height of the video frame in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |coded_size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  gfx::Size coded_size_;

  // Layout property for each color planes, e.g. stride and buffer offset.
  std::vector<Plane> planes_;

  // Vector of sizes for each buffer, typically greater or equal to the area of
  // |coded_size_|.
  std::vector<size_t> buffer_sizes_;
};

// Outputs VideoFrameLayout::Plane to stream.
std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrameLayout::Plane& plane);

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_LAYOUT_H_
