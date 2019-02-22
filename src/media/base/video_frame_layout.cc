// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_layout.h"

#include <numeric>
#include <sstream>

namespace media {

namespace {

template <class T>
std::string VectorToString(const std::vector<T>& vec) {
  std::ostringstream result;
  std::string delim;
  result << "[";
  for (auto& v : vec) {
    result << delim;
    result << v;
    if (delim.size() == 0)
      delim = ", ";
  }
  result << "]";
  return result.str();
}

std::vector<VideoFrameLayout::Plane> PlanesFromStrides(
    const std::vector<int32_t> strides) {
  std::vector<VideoFrameLayout::Plane> planes(strides.size());
  for (size_t i = 0; i < strides.size(); i++) {
    planes[i].stride = strides[i];
  }
  return planes;
}

}  // namespace

VideoFrameLayout::VideoFrameLayout(VideoPixelFormat format,
                                   const gfx::Size& coded_size,
                                   std::vector<int32_t> strides,
                                   std::vector<size_t> buffer_sizes)
    : format_(format),
      coded_size_(coded_size),
      planes_(PlanesFromStrides(strides)),
      buffer_sizes_(std::move(buffer_sizes)) {}

VideoFrameLayout::VideoFrameLayout(VideoPixelFormat format,
                                   const gfx::Size& coded_size,
                                   std::vector<Plane> planes,
                                   std::vector<size_t> buffer_sizes)
    : format_(format),
      coded_size_(coded_size),
      planes_(std::move(planes)),
      buffer_sizes_(std::move(buffer_sizes)) {}

VideoFrameLayout::VideoFrameLayout()
    : format_(PIXEL_FORMAT_UNKNOWN),
      planes_(kDefaultPlaneCount),
      buffer_sizes_(kDefaultBufferCount, 0) {}

VideoFrameLayout::~VideoFrameLayout() = default;
VideoFrameLayout::VideoFrameLayout(const VideoFrameLayout&) = default;
VideoFrameLayout::VideoFrameLayout(VideoFrameLayout&&) = default;
VideoFrameLayout& VideoFrameLayout::operator=(const VideoFrameLayout&) =
    default;

size_t VideoFrameLayout::GetTotalBufferSize() const {
  return std::accumulate(buffer_sizes_.begin(), buffer_sizes_.end(), 0u);
}

std::string VideoFrameLayout::ToString() const {
  std::ostringstream s;
  s << "VideoFrameLayout format: " << VideoPixelFormatToString(format_)
    << ", coded_size: " << coded_size_.ToString()
    << ", num_buffers: " << num_buffers()
    << ", buffer_sizes: " << VectorToString(buffer_sizes_)
    << ", num_planes: " << num_planes()
    << ", planes (stride, offset): " << VectorToString(planes_);
  return s.str();
}

std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrameLayout::Plane& plane) {
  ostream << "(" << plane.stride << ", " << plane.offset << ")";
  return ostream;
}

}  // namespace media
