// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/video_frame.h"

namespace pp {

// VideoFrame ------------------------------------------------------------------

VideoFrame::VideoFrame()
    : video_frame_() {
  video_frame_.image_data = image_data_.pp_resource();
}

VideoFrame::VideoFrame(const ImageData& image_data, PP_TimeTicks timestamp)
    : image_data_(image_data), video_frame_() {
  video_frame_.timestamp = timestamp;
  video_frame_.image_data = image_data_.pp_resource();
}

VideoFrame::VideoFrame(PassRef, const PP_VideoFrame& pp_video_frame)
    : video_frame_(pp_video_frame) {
  // Take over the image_data resource in the frame.
  image_data_ = ImageData(PASS_REF, video_frame_.image_data);
}

VideoFrame::VideoFrame(const VideoFrame& other)
    : video_frame_() {
  set_image_data(other.image_data());
  set_timestamp(other.timestamp());
}

VideoFrame::~VideoFrame() {
}

VideoFrame& VideoFrame::operator=(const VideoFrame& other) {
  if (this == &other)
    return *this;

  set_image_data(other.image_data());
  set_timestamp(other.timestamp());

  return *this;
}

}  // namespace pp
