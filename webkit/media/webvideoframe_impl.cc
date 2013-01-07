// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "webkit/media/webvideoframe_impl.h"

namespace webkit_media {

WebVideoFrameImpl::WebVideoFrameImpl(
    scoped_refptr<media::VideoFrame> video_frame)
    : video_frame(video_frame) {
}

WebVideoFrameImpl::~WebVideoFrameImpl() {}

}  // namespace webkit_media
