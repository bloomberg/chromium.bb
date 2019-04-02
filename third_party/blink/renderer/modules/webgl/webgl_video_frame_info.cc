// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_video_frame_info.h"

namespace blink {

WebGLVideoFrameInfo* WebGLVideoFrameInfo::Create(
    WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr) {
  return MakeGarbageCollected<WebGLVideoFrameInfo>(frame_metadata_ptr);
}

WebGLVideoFrameInfo::WebGLVideoFrameInfo(
    WebMediaPlayer::VideoFrameUploadMetadata* frame_metadata_ptr) {
  current_time_ = frame_metadata_ptr->timestamp.InMicrosecondsF();
  texture_width_ = frame_metadata_ptr->visible_rect.width();
  texture_height_ = frame_metadata_ptr->visible_rect.height();
}

}  // namespace blink
