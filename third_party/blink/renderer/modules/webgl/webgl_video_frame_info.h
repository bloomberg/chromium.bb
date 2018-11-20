// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_INFO_H_

#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class WebGLVideoFrameInfo final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLVideoFrameInfo* Create(WebMediaPlayer::VideoFrameUploadMetadata*);

  double currentTime() const { return current_time_; }
  unsigned long textureWidth() const { return texture_width_; }
  unsigned long textureHeight() const { return texture_height_; }

 private:
  explicit WebGLVideoFrameInfo(WebMediaPlayer::VideoFrameUploadMetadata*);

  double current_time_ = 0;
  unsigned long texture_width_ = 0;
  unsigned long texture_height_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_FRAME_INFO_H_
