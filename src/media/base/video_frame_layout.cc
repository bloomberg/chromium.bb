// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_layout.h"

#include <numeric>
#include <sstream>

#include "base/logging.h"

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

// static
size_t VideoFrameLayout::NumPlanes(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
      return 1;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_MT21:
      return 2;
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      return 3;
    case PIXEL_FORMAT_I420A:
      return 4;
    case PIXEL_FORMAT_UNKNOWN:
      // Note: PIXEL_FORMAT_UNKNOWN is used for end-of-stream frame.
      // Set its NumPlanes() to zero to avoid NOTREACHED().
      return 0;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return 0;
}

// static
base::Optional<VideoFrameLayout> VideoFrameLayout::Create(
    VideoPixelFormat format,
    const gfx::Size& coded_size) {
  return CreateWithStrides(format, coded_size,
                           std::vector<int32_t>(NumPlanes(format), 0));
}

// static
base::Optional<VideoFrameLayout> VideoFrameLayout::CreateWithStrides(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    std::vector<int32_t> strides,
    std::vector<size_t> buffer_sizes) {
  return CreateWithPlanes(format, coded_size, PlanesFromStrides(strides),
                          std::move(buffer_sizes));
}

// static
base::Optional<VideoFrameLayout> VideoFrameLayout::CreateWithPlanes(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    std::vector<Plane> planes,
    std::vector<size_t> buffer_sizes,
    size_t buffer_addr_align) {
  // NOTE: Even if format is UNKNOWN, it is valid if coded_sizes is not Empty().
  // TODO(crbug.com/896135): Return base::nullopt,
  // if (format != PIXEL_FORMAT_UNKNOWN || !coded_sizes.IsEmpty())
  // TODO(crbug.com/896135): Return base::nullopt,
  // if (planes.size() != NumPlanes(format))
  // TODO(crbug.com/896135): Return base::nullopt,
  // if (buffer_sizes.size() > planes.size())
  return VideoFrameLayout(format, coded_size, std::move(planes),
                          std::move(buffer_sizes), buffer_addr_align);
}

VideoFrameLayout::VideoFrameLayout(VideoPixelFormat format,
                                   const gfx::Size& coded_size,
                                   std::vector<Plane> planes,
                                   std::vector<size_t> buffer_sizes,
                                   size_t buffer_addr_align)
    : format_(format),
      coded_size_(coded_size),
      planes_(std::move(planes)),
      buffer_sizes_(std::move(buffer_sizes)),
      buffer_addr_align_(buffer_addr_align) {}

VideoFrameLayout::~VideoFrameLayout() = default;
VideoFrameLayout::VideoFrameLayout(const VideoFrameLayout&) = default;
VideoFrameLayout::VideoFrameLayout(VideoFrameLayout&&) = default;
VideoFrameLayout& VideoFrameLayout::operator=(const VideoFrameLayout&) =
    default;

size_t VideoFrameLayout::GetTotalBufferSize() const {
  return std::accumulate(buffer_sizes_.begin(), buffer_sizes_.end(), 0u);
}

std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrameLayout::Plane& plane) {
  ostream << "(" << plane.stride << ", " << plane.offset << ")";
  return ostream;
}

bool VideoFrameLayout::Plane::operator==(
    const VideoFrameLayout::Plane& rhs) const {
  return stride == rhs.stride && offset == rhs.offset;
}

bool VideoFrameLayout::Plane::operator!=(
    const VideoFrameLayout::Plane& rhs) const {
  return !(*this == rhs);
}

bool VideoFrameLayout::operator==(const VideoFrameLayout& rhs) const {
  return format_ == rhs.format_ && coded_size_ == rhs.coded_size_ &&
         planes_ == rhs.planes_ && buffer_sizes_ == rhs.buffer_sizes_;
}

bool VideoFrameLayout::operator!=(const VideoFrameLayout& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrameLayout& layout) {
  ostream << "VideoFrameLayout(format: " << layout.format()
          << ", coded_size: " << layout.coded_size().ToString()
          << ", planes (stride, offset): " << VectorToString(layout.planes())
          << ", buffer_sizes: " << VectorToString(layout.buffer_sizes()) << ")";
  return ostream;
}

}  // namespace media
