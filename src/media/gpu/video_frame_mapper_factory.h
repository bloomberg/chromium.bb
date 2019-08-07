// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_
#define MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_

#include <memory>

#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

// A factory function for VideoFrameMapper.
// The appropriate VideoFrameMapper is a platform-dependent.
class MEDIA_GPU_EXPORT VideoFrameMapperFactory {
 public:
  // Create an instance of the frame mapper.
  static std::unique_ptr<VideoFrameMapper> CreateMapper(
      VideoPixelFormat format);

  // |linear_buffer_mapper| stands for a created mapper type. If true, the
  // mapper will expect frames passed to it to be in linear format.
  static std::unique_ptr<VideoFrameMapper> CreateMapper(
      VideoPixelFormat format,
      bool force_linear_buffer_mapper);
};

}  // namespace media

#endif  // MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_
